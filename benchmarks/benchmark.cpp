#include "pool.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <vector>
using clk=std::chrono::steady_clock;
int main(){
  wg::Pool pool(4,30000);
  constexpr size_t N=10000;
  std::vector<double> latency(N);auto start=clk::now();
  for(size_t i=0;i<N;i++){
    auto enq=clk::now();
    while(!pool.submit([&,i,enq]{latency[i]=std::chrono::duration<double,std::micro>(clk::now()-enq).count();})){}
  }
  pool.wait_idle();auto end=clk::now();pool.shutdown();
  std::sort(latency.begin(),latency.end());
  double secs=std::chrono::duration<double>(end-start).count();
  std::cout<<std::fixed<<std::setprecision(3)<<"{\"tasks\":"<<N<<",\"elapsed_s\":"<<secs<<",\"tasks_per_s\":"<<(N/secs)<<",\"p50_queue_us\":"<<latency[N/2]<<",\"p95_queue_us\":"<<latency[N*95/100]<<",\"p99_queue_us\":"<<latency[N*99/100]<<"}\n";
}
