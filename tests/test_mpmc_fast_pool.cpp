#include "mpmc_fast_pool.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>
int main(){
  for(int round=0;round<4;round++){
    constexpr int N=24000;
    wg::MPMCFastPool p(4,256);
    std::vector<std::atomic<int>> seen(N);for(auto &x:seen)x.store(0);
    std::vector<std::thread> producer;
    for(int s=0;s<4;s++)producer.emplace_back([&,s]{
      for(int i=s;i<N;i+=4){while(!p.submit([&,i]{seen[i]++;}))std::this_thread::yield();}
    });
    for(auto& t:producer)t.join();
    p.wait_idle();
    for(auto &x:seen)assert(x.load()==1);
    assert(p.completed()==N);p.shutdown();assert(!p.submit([]{}));
  }
}
