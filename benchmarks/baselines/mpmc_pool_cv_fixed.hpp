// Strong simple baseline: original CV protocol with completion synchronized to the wait mutex.
#pragma once
#include "mpmc_ring.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <semaphore>
#include <thread>
#include <vector>
namespace wg_cv_baseline {
// Integrates the bounded MPMC ring into a real executor, not just a queue demo.
// Admission/stop synchronization remains mutex-protected intentionally;
// only queue transfer is lock-free, so entire executor is NOT lock-free.
class MPMCPool {
    wg::MPMCRing<std::function<void()>> queue_;
    std::counting_semaphore<65536> tokens_{0};
    std::mutex admit_mu_,idle_mu_;
    std::condition_variable idle_cv_;
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
        std::unique_lock<std::mutex> g(idle_mu_);
        idle_cv_.wait(g,[this]{
            const bool idle=pending_.load(std::memory_order_acquire)==0;
#ifdef WAKEGUARD_TEST_HOOKS
            if(!idle && before_idle_park_)before_idle_park_();
#endif
            return idle;
        });
    }
#ifdef WAKEGUARD_TEST_HOOKS
    // Tests install these before submitting work. No hooks exist in release builds.
    void idle_test_hooks(std::function<void()> before, std::function<void()> after){
        before_idle_park_=std::move(before);after_completion_notify_=std::move(after);
    }
#endif
    void shutdown(){
        {std::lock_guard<std::mutex> g(admit_mu_);
         if(stopping_)return;
         stopping_=true;
         tokens_.release(static_cast<std::ptrdiff_t>(workers_.size()));}
        for(auto& w:workers_)if(w.joinable())w.join();
    }
    std::size_t completed()const{return completed_.load();}
    std::size_t failed()const{return failed_.load();}
private:
    void loop(){
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
                {std::lock_guard<std::mutex> lock(idle_mu_);}
                idle_cv_.notify_all();
#ifdef WAKEGUARD_TEST_HOOKS
                if(after_completion_notify_)after_completion_notify_();
#endif
            }
        }
    }
};
}
