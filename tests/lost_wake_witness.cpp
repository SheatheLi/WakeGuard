#include <condition_variable>
#include <iostream>
#include <mutex>
#include <chrono>
#include <cassert>
int main(){
  std::mutex mu;std::condition_variable cv;bool ready=false;
  // Deterministically move notification before waiter enters wait.
  {std::lock_guard<std::mutex> g(mu);ready=true;cv.notify_one();}
  std::unique_lock<std::mutex> lock(mu);
  auto bad=cv.wait_for(lock,std::chrono::milliseconds(20));
  // The broken wait has no predicate; the earlier notification cannot be retained.
  assert(bad==std::cv_status::timeout);
  // The corrected predicate-based wait returns immediately.
  bool good=cv.wait_for(lock,std::chrono::milliseconds(20),[&]{return ready;});
  assert(good);std::cout<<"{\"unpredicated_wait_timed_out\":true,\"predicate_wait_ready\":true}\n";
}
