#include "mpmc_pool.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
int main(){
  constexpr int N=20000;
  std::vector<std::atomic<int>> seen(N);for(auto &v:seen)v.store(0);
  wg::MPMCPool pool(4,128);
  std::vector<std::thread> producers;
  std::atomic<int> accepted{0};
  for(int p=0;p<4;p++)producers.emplace_back([&,p]{
    for(int i=p;i<N;i+=4){
      while(!pool.submit([&,i]{++seen[i];}))std::this_thread::yield();
      ++accepted;
    }
  });
  for(auto &p:producers)p.join();
  pool.wait_idle();
  for(auto &v:seen)assert(v.load()==1);
  assert(pool.completed()==N&&accepted==N&&pool.failed()==0);
  pool.shutdown();
  assert(!pool.submit([]{}));
  std::cout<<"{\"completed\":"<<pool.completed()<<",\"exactly_once\":true,\"shutdown_clean\":true}\n";
}
