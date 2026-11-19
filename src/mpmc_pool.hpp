#pragma once
#include "mpmc_ring.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>
namespace wg {
// Integrates the bounded MPMC ring into a real executor, not just a queue demo.
// Admission/stop synchronization remains mutex-protected intentionally;
// only queue transfer is lock-free, so entire executor is NOT lock-free.
class MPMCPool {
    inline static thread_local MPMCPool* executing_pool_=nullptr;
    MPMCRing<std::function<void()>> queue_;
    std::counting_semaphore<65536> tokens_{0};
    std::mutex admit_mu_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> pending_{0},completed_{0},failed_{0};
    bool stopping_=false;
#ifdef WAKEGUARD_TEST_HOOKS
    std::function<void()> before_idle_park_, after_completion_notify_;
#endif
public:
    MPMCPool(std::size_t n,std::size_t cap):queue_(cap){
        if(n==0||n>512||cap>4096)throw std::invalid_argument("n/cap bounds");
        for(std::size_t j=0;j<n;++j)workers_.emplace_back([this]{loop();});
    }
    ~MPMCPool(){shutdown();}
    MPMCPool(const MPMCPool&)=delete;MPMCPool& operator=(const MPMCPool&)=delete;
    bool submit(std::function<void()> f){
        std::lock_guard<std::mutex> g(admit_mu_);
        if(stopping_)return false;
        if(!queue_.try_push(std::move(f)))return false;
        pending_.fetch_add(1,std::memory_order_release);
        tokens_.release();
        return true;
    }
    void wait_idle(){
        if(executing_pool_==this)
            throw std::logic_error("worker cannot wait for its own completion");
        // Wait on the state itself. A completion between this load and wait
        // cannot be lost: atomic::wait compares the value before blocking.
        // No concurrent submit barrier is promised; callers stop producers
        // before using this method as a drain operation.
        auto remaining=pending_.load(std::memory_order_acquire);
        while(remaining!=0){
#ifdef WAKEGUARD_TEST_HOOKS
            if(before_idle_park_)before_idle_park_();
#endif
            pending_.wait(remaining,std::memory_order_acquire);
            remaining=pending_.load(std::memory_order_acquire);
        }
    }
#ifdef WAKEGUARD_TEST_HOOKS
    // Tests install these before submitting work. No hooks exist in release builds.
    void idle_test_hooks(std::function<void()> before, std::function<void()> after){
        before_idle_park_=std::move(before);after_completion_notify_=std::move(after);
    }
#endif
    // Workers may request stop; joining belongs to a single external owner.
    void request_stop(){
        {std::lock_guard<std::mutex> g(admit_mu_);
         if(stopping_)return;
         stopping_=true;
         tokens_.release(static_cast<std::ptrdiff_t>(workers_.size()));}
    }
    void shutdown(){
        if(executing_pool_==this)
            throw std::logic_error("worker must request_stop; external owner joins");
        request_stop();
        for(auto& w:workers_)if(w.joinable())w.join();
    }
    std::size_t completed()const{return completed_.load();}
    std::size_t failed()const{return failed_.load();}
private:
    void loop(){
        executing_pool_=this;
        for(;;){
            tokens_.acquire();
            std::function<void()> f;
            while(!queue_.try_pop(f)){
                {std::lock_guard<std::mutex> g(admit_mu_);
                 if(stopping_)return;}
                std::this_thread::yield();
            }
            try{f();}catch(...){failed_.fetch_add(1);}
            completed_.fetch_add(1);
            if(pending_.fetch_sub(1,std::memory_order_acq_rel)==1){
                pending_.notify_all();
#ifdef WAKEGUARD_TEST_HOOKS
                if(after_completion_notify_)after_completion_notify_();
#endif
            }
        }
    }
};
}
