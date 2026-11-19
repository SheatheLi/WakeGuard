#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <vector>
namespace wg {
// Alternative exact-per-work token protocol for controlled comparison with CV.
class SemaphorePool {
  inline static thread_local SemaphorePool* executing_pool_ = nullptr;
 public:
  SemaphorePool(size_t threads,size_t capacity):capacity_(capacity),workers_count_(threads){
    if(!threads||!capacity||capacity>65536)throw std::invalid_argument("bounds");
    for(size_t i=0;i<threads;i++)workers_.emplace_back([this]{loop();});
  }
  ~SemaphorePool(){shutdown();}
  SemaphorePool(const SemaphorePool&)=delete;
  SemaphorePool& operator=(const SemaphorePool&)=delete;
  bool submit(std::function<void()> f){
    {
      std::lock_guard<std::mutex> g(mu_);
      if(stopping_||queue_.size()>=capacity_)return false;
      queue_.push_back(std::move(f));++pending_;
    }
    tokens_.release();return true;
  }
  void wait_idle(){
    if(executing_pool_==this) throw std::logic_error("worker cannot wait for its own task");
    std::unique_lock<std::mutex> g(mu_);idle_.wait(g,[this]{return pending_==0;});}
  // Worker-safe signal. The external owner subsequently drains and joins.
  void request_stop(){
    {
      std::lock_guard<std::mutex> g(mu_);
      if(stopping_) return;
      stopping_=true;
    }
    tokens_.release(static_cast<std::ptrdiff_t>(workers_count_));
  }
  void shutdown(){
    if(executing_pool_==this)
      throw std::logic_error("worker must request_stop; external owner joins");
    request_stop();
    for(auto &t:workers_) if(t.joinable())t.join();
    workers_.clear();
  }
  bool stop_requested(){
    std::lock_guard<std::mutex> g(mu_);
    return stopping_;
  }
 private:
  void loop(){executing_pool_=this;for(;;){
    tokens_.acquire();std::function<void()> f;
    {std::lock_guard<std::mutex> g(mu_);if(queue_.empty()&&stopping_){executing_pool_=nullptr;return;}
     if(queue_.empty())continue;
     f=std::move(queue_.front());queue_.pop_front();}
    try{f();}catch(...){}
    {std::lock_guard<std::mutex> g(mu_);if(--pending_==0)idle_.notify_all();}
  }}
  size_t capacity_,workers_count_,pending_=0;
  std::mutex mu_;std::condition_variable idle_;
  std::counting_semaphore<65536> tokens_{0};
  std::deque<std::function<void()>> queue_;std::vector<std::thread> workers_;bool stopping_=false;
};
}
