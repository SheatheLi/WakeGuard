#include "pool.hpp"
#include "semaphore_pool.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
int main(){
  {wg::Pool p(4,2000);std::atomic<int> done=0;std::vector<std::thread> prod;
   for(int k=0;k<4;k++) prod.emplace_back([&]{for(int i=0;i<300;i++){
      while(!p.submit([&]{done.fetch_add(1,std::memory_order_relaxed);})){std::this_thread::yield();}
   }});
   for(auto &t:prod)t.join();p.wait_idle();assert(done==1200);assert(p.snapshot().pending==0);
   assert(p.submit([]{throw std::runtime_error("job failed");}));p.wait_idle();assert(p.snapshot().failed==1);
   p.shutdown();assert(!p.submit([]{}));
  }
  {wg::Pool p(1,2);std::atomic<int> done=0;
    for(int i=0;i<1000;i++){while(!p.submit([&]{++done;}))std::this_thread::yield();p.wait_idle();}
    assert(done==1000);p.shutdown();}
  {wg::Pool p(2,30);for(int i=0;i<30;i++)while(!p.submit([]{std::this_thread::sleep_for(20us);}));
    p.shutdown();auto s=p.snapshot();assert(s.completed==30 && s.pending==0);}
  {wg::SemaphorePool p(3,2000);std::atomic<int> count=0;for(int i=0;i<700;i++)while(!p.submit([&]{count++;})){}p.wait_idle();assert(count==700);p.shutdown();}
  std::cout<<"WakeGuard checks passed\n";
}
