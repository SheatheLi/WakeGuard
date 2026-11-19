#pragma once
#include "mpmc_ring.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>
namespace wg {
// Experimental executor. Lock-free admission vs the mutex-gated MPMCPool.
// Not a lock-free *system*: std::function may allocate and semaphore is OS backed.
// Single-owner shutdown contract; no lifetime guarantee for concurrent destructor.
class MPMCFastPool {
    static constexpr std::uint64_t STOP=std::uint64_t(1)<<63;
    MPMCRing<std::function<void()>> queue_;
    std::counting_semaphore<65536> tokens_{0};
    std::atomic<std::uint64_t> status_{0}; // high bit stopped, low bits submitters active
    std::atomic<std::size_t> pending_{0},completed_{0},failed_{0};
    std::condition_variable idle_cv_;
    std::mutex idle_mu_;
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
        if(ok)tokens_.release();
        else if(pending_.fetch_sub(1,std::memory_order_acq_rel)==1)idle_cv_.notify_all();
        status_.fetch_sub(1,std::memory_order_release);
        return ok;
    }
    void wait_idle(){
        std::unique_lock<std::mutex> g(idle_mu_);
        idle_cv_.wait(g,[this]{return pending_.load(std::memory_order_acquire)==0;});
    }
    void shutdown(){
        if(status_.fetch_or(STOP,std::memory_order_acq_rel)&STOP)return;
        while((status_.load(std::memory_order_acquire)&~STOP)!=0)std::this_thread::yield();
        tokens_.release(static_cast<std::ptrdiff_t>(workers_.size()));
        for(auto& t:workers_)if(t.joinable())t.join();
    }
    std::size_t completed()const{return completed_.load();}
    std::size_t failed()const{return failed_.load();}
private:
    void run(){for(;;){
        tokens_.acquire();
        std::function<void()> fn;
        while(!queue_.try_pop(fn)){
            // A ring producer may have claimed a slot but not yet published
            // its sequence. Do not lose a semaphore work token on transient
            // empty/contended observations.
            if(status_.load(std::memory_order_acquire)&STOP)return;
            std::this_thread::yield();
        }
        try{fn();}catch(...){failed_.fetch_add(1);}
        completed_.fetch_add(1);
        if(pending_.fetch_sub(1,std::memory_order_acq_rel)==1)idle_cv_.notify_all();
    }}
};
}
