#pragma once
#include "mpmc_ring.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
namespace wg {
// Experimental executor. Lock-free admission vs the mutex-gated MPMCPool.
// Not a lock-free *system*: std::function may allocate and semaphore is OS backed.
// Single-owner shutdown contract; no lifetime guarantee for concurrent destructor.
class MPMCFastPool {
    inline static thread_local MPMCFastPool* executing_pool_=nullptr;
    static constexpr std::uint64_t STOP=std::uint64_t(1)<<63;
    MPMCRing<std::function<void()>> queue_;
    // Reliable notification channel: credit counter and predicate share a mutex.
    // This is NOT a lock-free execution system; only the ring remains lock-free.
    mutable std::mutex wake_mu_;
    std::condition_variable wake_cv_;
    std::size_t queued_credits_{0};
    std::atomic<std::uint64_t> status_{0}; // high bit stopped, low bits submitters active
    std::atomic<std::size_t> pending_{0},completed_{0},failed_{0};
    std::vector<std::thread> workers_;
public:
    MPMCFastPool(std::size_t threads,std::size_t cap):queue_(cap){
        if(!threads||threads>512||cap>4096)throw std::invalid_argument("bounds");
        for(std::size_t i=0;i<threads;i++)workers_.emplace_back([this]{run();});
    }
    MPMCFastPool(const MPMCFastPool&)=delete;
    MPMCFastPool& operator=(const MPMCFastPool&)=delete;
    ~MPMCFastPool(){shutdown();}
    bool submit(std::function<void()> fn){
        auto state=status_.load(std::memory_order_acquire);
        do{
            if(state & STOP)return false;
        }while(!status_.compare_exchange_weak(state,state+1,std::memory_order_acq_rel,std::memory_order_acquire));
        pending_.fetch_add(1,std::memory_order_release);
        const bool ok=queue_.try_push(std::move(fn));
        if(ok){
            {std::lock_guard<std::mutex> lock(wake_mu_);queued_credits_++;}
            wake_cv_.notify_one();
        } else if(pending_.fetch_sub(1,std::memory_order_acq_rel)==1){
            {std::lock_guard<std::mutex> lock(wake_mu_);}
            wake_cv_.notify_all();
        }
        status_.fetch_sub(1,std::memory_order_acq_rel);
        return ok;
    }
    void wait_idle(){
        if(executing_pool_==this) throw std::logic_error("worker cannot wait for its own task");
        std::unique_lock<std::mutex> lock(wake_mu_);
        wake_cv_.wait(lock,[this]{return pending_.load(std::memory_order_acquire)==0;});
    }
    // Two-phase structured-concurrency shutdown: workers can request stop,
    // but only the owning thread joins. Previously worker-initiated shutdown
    // could join itself and terminate during destruction.
    void request_stop(){
        if(status_.fetch_or(STOP,std::memory_order_acq_rel)&STOP)return;
        // STOP rejects new submitters; any admitted submitter must finish
        // publication before stop tokens are released.
        while((status_.load(std::memory_order_acquire)&~STOP)!=0)std::this_thread::yield();
        {std::lock_guard<std::mutex> lock(wake_mu_);}
        wake_cv_.notify_all();
    }
    void shutdown(){
        for(const auto& t:workers_)
            if(t.joinable() && t.get_id()==std::this_thread::get_id())
                throw std::logic_error("worker must request_stop; external owner joins");
        request_stop();
        for(auto& t:workers_)if(t.joinable())t.join();
    }
    bool stop_requested()const{return (status_.load(std::memory_order_acquire)&STOP)!=0;}
    std::size_t completed()const{return completed_.load();}
    std::size_t failed()const{return failed_.load();}
    std::size_t pending_count()const{return pending_.load(std::memory_order_acquire);}
    std::uint64_t status_snapshot()const{return status_.load(std::memory_order_acquire);}
private:
    bool drained_after_stop()const{
        const auto status=status_.load(std::memory_order_acquire);
        // After STOP excludes new admissions and existing submitters have
        // finished, pending=0 is a safe termination condition.
        return (status&STOP) && (status&~STOP)==0 &&
               pending_.load(std::memory_order_acquire)==0;
    }
    void run(){executing_pool_=this;for(;;){
        {
            std::unique_lock<std::mutex> lock(wake_mu_);
            wake_cv_.wait(lock,[this]{return queued_credits_>0 || drained_after_stop();});
            if(queued_credits_==0){executing_pool_=nullptr;return;}
            queued_credits_--;
        }
        std::function<void()> fn;
        // A credited producer has completed release-store publication, but
        // another consumer can transiently reserve the earlier ring position.
        while(!queue_.try_pop(fn))std::this_thread::yield();
        try{fn();}catch(...){failed_.fetch_add(1);}
        completed_.fetch_add(1);
        if(pending_.fetch_sub(1,std::memory_order_acq_rel)==1){
            {std::lock_guard<std::mutex> lock(wake_mu_);}
            wake_cv_.notify_all();
        }
    }}
};
}
