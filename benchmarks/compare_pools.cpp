#include "pool.hpp"
#include "semaphore_pool.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <iostream>
using clk=std::chrono::steady_clock;
template <class P> void run(const char*name){
 P p(4,40000);std::vector<double> lat(16000);auto start=clk::now();
 for(size_t i=0;i<lat.size();i++){
  auto at=clk::now();while(!p.submit([&,i,at]{lat[i]=std::chrono::duration<double,std::micro>(clk::now()-at).count();})){}
 }
 p.wait_idle();auto end=clk::now();p.shutdown();std::sort(lat.begin(),lat.end());
 std::cout<<name<<","<<(lat.size()/std::chrono::duration<double>(end-start).count())<<","<<lat[lat.size()*99/100]<<"\n";
}
int main(){std::cout<<"protocol,throughput,p99_queue_us\n";run<wg::Pool>("condition_variable");run<wg::SemaphorePool>("semaphore");}
