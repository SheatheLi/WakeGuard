#include "mpmc_fast_pool.hpp"
#include <atomic>
#include <cassert>
#include <stdexcept>
#include <thread>
int main(){
    for(int round=0;round<32;round++){
      wg::MPMCFastPool p(4,2048);
      std::atomic<int> done{0};
      // Schedule one worker-initiated stop request after queued work exists.
      for(int i=0;i<1000;i++){
        bool ok=p.submit([&]{done.fetch_add(1,std::memory_order_relaxed);});
        assert(ok);
      }
      bool ok=p.submit([&]{p.request_stop();done.fetch_add(1,std::memory_order_relaxed);});
      assert(ok);
      p.wait_idle();
      p.shutdown();
      assert(done.load()==1001);
      assert(p.completed()==1001);
      assert(p.stop_requested());
      assert(!p.submit([]{}));
    }
}
