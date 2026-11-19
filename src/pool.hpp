#pragma once
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace wg {
// Bounded MPMC task pool. Wake and empty-state transitions share one mutex.
// Progress invariant: pending == queued + currently executing tasks.
class Pool {
  inline static thread_local Pool* executing_pool_ = nullptr;
 public:
  explicit Pool(size_t threads, size_t capacity): capacity_(capacity) {
    if (!threads || !capacity) throw std::invalid_argument("positive threads/capacity required");
    for (size_t i=0;i<threads;++i) workers_.emplace_back([this]{run();});
  }
  Pool(const Pool&)=delete;
  Pool& operator=(const Pool&)=delete;
  ~Pool(){ shutdown(); }
  bool submit(std::function<void()> fn) {
    std::lock_guard<std::mutex> g(mu_);
    if (stopping_ || queue_.size()>=capacity_) return false;
    queue_.push_back(std::move(fn)); ++pending_; ++enqueued_; ++epoch_;
    cv_.notify_one();
    return true;
  }
  void wait_idle(){
    if (executing_pool_ == this)
      throw std::logic_error("worker cannot wait for its own task");
    std::unique_lock<std::mutex> g(mu_);
    idle_.wait(g, [this]{return pending_==0;});
  }
  // Calling request_stop from a task is supported. Ownership remains with
  // an external thread, which is the only caller allowed to join/destroy.
  void request_stop(){
    {
      std::lock_guard<std::mutex> g(mu_);
      if (stopping_) return;
      stopping_=true; ++epoch_;
    }
    cv_.notify_all();
  }
  void shutdown(){
    if (executing_pool_ == this)
      throw std::logic_error("worker must request_stop; external owner joins");
    // No concurrent shutdown callers or lifetime-changing destruction allowed.
    request_stop();
    for(auto &t:workers_) if(t.joinable())t.join();
    workers_.clear();
  }
  bool stop_requested() const {
    std::lock_guard<std::mutex> g(mu_);
    return stopping_;
  }
  struct Snapshot {size_t pending,queued,completed,failed;uint64_t epoch;bool stopping;};
  Snapshot snapshot() const {
    std::lock_guard<std::mutex> g(mu_);
    return {pending_,queue_.size(),completed_,failed_,epoch_,stopping_};
  }
 private:
  void run(){
    executing_pool_=this;
    for(;;){
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> g(mu_);
        cv_.wait(g,[this]{return stopping_ || !queue_.empty();});
        if(queue_.empty() && stopping_) {executing_pool_=nullptr; return;}
        job=std::move(queue_.front());queue_.pop_front();
      }
      bool failed=false;
      try{job();}catch(...){failed=true;}
      {
        std::lock_guard<std::mutex> g(mu_);
        --pending_; ++completed_; if(failed)++failed_;
        if(pending_==0)idle_.notify_all();
      }
    }
  }
  const size_t capacity_;
  mutable std::mutex mu_;
  std::condition_variable cv_,idle_;
  std::deque<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  size_t pending_=0,enqueued_=0,completed_=0,failed_=0;
  uint64_t epoch_=0;
  bool stopping_=false;
};
} // namespace wg
