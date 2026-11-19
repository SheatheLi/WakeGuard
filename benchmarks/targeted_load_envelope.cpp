// Focused comparative executor benchmark. Correctness gates every reported result.
// Repeated measurements are intentionally reported raw: NO superiority inference
// without variance, machine and load regime. Single process can be contaminated
// by scheduling/thermal effects; randomized order is handled by the driver.
#include "pool.hpp"
#include "semaphore_pool.hpp"
#include "mpmc_pool.hpp"
#include "mpmc_fast_pool.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <string>
#include <cstdlib>
using Clock=std::chrono::steady_clock;
using namespace std::chrono;

static void work(int mode) {
  if(mode==1) std::this_thread::sleep_for(microseconds(100));
  if(mode==2){
    auto start=Clock::now();
    while(duration_cast<microseconds>(Clock::now()-start).count()<20)std::atomic_signal_fence(std::memory_order_seq_cst);
  }
}
template<typename Engine> int run(int mode,int tasks,int producers,int consumers){
  Engine engine(consumers,256);
  std::vector<std::int64_t> wait_ns(tasks,-1);
  std::vector<std::atomic<int>> seen(tasks);
  for(auto &a:seen)a.store(0);
  std::atomic<int> accepted{0};
  auto t0=Clock::now();
  std::vector<std::thread> writers;
  for(int p=0;p<producers;p++)writers.emplace_back([&,p]{
    for(int i=p;i<tasks;i+=producers){
      for(;;){
        const auto mark=Clock::now();
        if(engine.submit([&,i,mark]{
          wait_ns[i]=duration_cast<nanoseconds>(Clock::now()-mark).count();
          seen[i].fetch_add(1,std::memory_order_relaxed);
          work(mode);
        })){accepted.fetch_add(1,std::memory_order_relaxed);break;}
        std::this_thread::yield();
      }
    }
  });
  for(auto &p:writers)p.join();
  engine.wait_idle();
  const auto done=Clock::now();
  engine.shutdown();
  int duplicates=0,missing=0;
  for(int i=0;i<tasks;i++){
    auto cnt=seen[i].load(std::memory_order_relaxed);
    if(cnt==0 || wait_ns[i]<0)missing++;
    if(cnt>1)duplicates++;
  }
  std::sort(wait_ns.begin(),wait_ns.end());
  auto p99=wait_ns[static_cast<size_t>(.99*(tasks-1))]/1000.;
  std::cout<<"{\"mode\":"<<mode<<",\"tasks\":"<<tasks<<",\"producers\":"<<producers
           <<",\"consumers\":"<<consumers<<",\"wall_ms\":"
           <<duration<double,std::milli>(done-t0).count()<<",\"enqueue_to_start_p99_us\":"<<p99
           <<",\"accepted\":"<<accepted.load()<<",\"missing\":"<<missing
           <<",\"duplicates\":"<<duplicates<<"}"<<std::endl;
  return (missing||duplicates||accepted!=tasks) ? 2:0;
}
int main(int argc,char**argv){
  if(argc!=6){std::cerr<<"usage: targeted_load_envelope <cv|sem|mpmc_mutex|mpmc_atomic> <empty|sleep100|busy20> tasks producers consumers\n";return 64;}
  std::string impl=argv[1], mode=argv[2];
  int m=mode=="empty"?0:mode=="sleep100"?1:mode=="busy20"?2:-1;
  int tasks=std::atoi(argv[3]), prod=std::atoi(argv[4]),cons=std::atoi(argv[5]);
  if(m<0 || tasks<100 || tasks>200000 || prod<1 || prod>64 || cons<1 || cons>64)return 64;
  if(impl=="cv")return run<wg::Pool>(m,tasks,prod,cons);
  if(impl=="sem")return run<wg::SemaphorePool>(m,tasks,prod,cons);
  if(impl=="mpmc_mutex")return run<wg::MPMCPool>(m,tasks,prod,cons);
  if(impl=="mpmc_atomic")return run<wg::MPMCFastPool>(m,tasks,prod,cons);
  return 64;
}
