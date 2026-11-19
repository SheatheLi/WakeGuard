#include "mpmc_ring.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <deque>
#include <thread>
#include <vector>
#include <stdexcept>
using namespace std::chrono;

struct MutexRing {
 std::mutex mu;std::deque<int> q;std::size_t cap;
 explicit MutexRing(std::size_t n):cap(n){}
 bool try_push(int i){std::lock_guard<std::mutex> g(mu);if(q.size()>=cap)return false;q.push_back(i);return true;}
 bool try_pop(int& i){std::lock_guard<std::mutex> g(mu);if(q.empty())return false;i=q.front();q.pop_front();return true;}
};

template<class Q> double drive(Q& queue, int count=60000){
 constexpr int P=4,C=4;
 std::vector<std::atomic<int>> seen(count);for(auto& x:seen)x.store(0);
 std::atomic<int> produced{0},consumed{0},duplicates{0};
 auto begin=steady_clock::now();std::vector<std::thread> workers;
 for(int c=0;c<C;++c)workers.emplace_back([&]{int val;
  while(consumed.load(std::memory_order_relaxed)<count){
   if(queue.try_pop(val)){
    if(val<0 || val>=count || seen[val].fetch_add(1)!=0)duplicates++;
    ++consumed;
   }else std::this_thread::yield();
  }
 });
 for(int p=0;p<P;++p)workers.emplace_back([&,p]{
  for(int i=p;i<count;i+=P){while(!queue.try_push(i))std::this_thread::yield();++produced;}
 });
 for(auto& t:workers)t.join();
 for(const auto& v:seen) assert(v.load()==1);
 assert(consumed==count && produced==count && duplicates==0);
 return duration<double,std::milli>(steady_clock::now()-begin).count();
}
int main(){
 wg::MPMCRing<int> ring(128);
 int val=-1;assert(!ring.try_pop(val));
 assert(ring.try_push(23));assert(ring.try_pop(val)&&val==23);
 assert(!ring.try_pop(val));
 double a=drive(ring);
 MutexRing mutex(128);double b=drive(mutex);
 std::cout << "{\"items\":60000,\"producers\":4,\"consumers\":4,\"ring_ms\":"<<a
           <<",\"mutex_ms\":"<<b<<",\"exactly_once\":true}\n";
}
