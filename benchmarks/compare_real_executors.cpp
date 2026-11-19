#include "pool.hpp"
#include "semaphore_pool.hpp"
#include "mpmc_pool.hpp"
#include "mpmc_fast_pool.hpp"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <type_traits>
using namespace std::chrono;
using Clock=steady_clock;
template<class Pool> void bench(const char* name){
 constexpr int N=18000;
 Pool pool(4,256);
 std::vector<std::int64_t> latency(N);
 std::atomic<int> accepted{0};
 auto start=Clock::now();std::vector<std::thread> producers;
 for(int p=0;p<4;p++)producers.emplace_back([&,p]{
   for(int i=p;i<N;i+=4){
     auto t=Clock::now();
     // A retry can refresh enqueue timestamp; rejected attempts are NOT counted.
     while(!pool.submit([&,i,t]{latency[i]=duration_cast<nanoseconds>(Clock::now()-t).count();})){
       std::this_thread::yield();t=Clock::now();
     }
     ++accepted;
   }
 });
 for(auto& t:producers)t.join();pool.wait_idle();
 auto ms=duration<double,std::milli>(Clock::now()-start).count();pool.shutdown();
 std::sort(latency.begin(),latency.end());
 auto p99=latency[static_cast<int>(.99*(N-1))]/1000.;
 std::cout << "{\"pool\":\"" << name << "\",\"tasks\":"<<N<<",\"wall_ms\":"<<ms
           <<",\"queue_delay_p99_us\":"<<p99<<",\"accepted\":"<<accepted<<"}\n";
}
int main(){bench<wg::Pool>("cv_mutex");bench<wg::SemaphorePool>("sem_mutex");bench<wg::MPMCPool>("mpmc_mutex_admission");bench<wg::MPMCFastPool>("mpmc_atomic_admission");}
