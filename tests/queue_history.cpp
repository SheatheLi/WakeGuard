#include "mpmc_ring.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include <string>
struct Record {int id;std::string kind;int value;int begin;int end;};
int main(){
 wg::MPMCRing<int> q(8);
 std::atomic<int> logical{0};std::mutex mu;std::vector<Record> rec;
 std::vector<std::thread> workers;
 for(int i=0;i<3;i++) workers.emplace_back([&,i]{
  int start,end;
  while(true){
   start=++logical;
   if(q.try_push(i+1)){end=++logical;break;}
   std::this_thread::yield();
  }
  std::lock_guard<std::mutex> lock(mu);rec.push_back({i,"PUSH",i+1,start,end});
 });
 for(int i=0;i<3;i++) workers.emplace_back([&,i]{
  int start,end,val;
  while(true){
   start=++logical;
   if(q.try_pop(val)){end=++logical;break;}
   std::this_thread::yield();
  }
  std::lock_guard<std::mutex> lock(mu);rec.push_back({i+3,"POP",val,start,end});
 });
 for(auto& t:workers)t.join();
 std::sort(rec.begin(),rec.end(),[](const auto&a,const auto&b){return a.id<b.id;});
 std::cout<<"id,op,value,begin,end\n";
 for(auto&r:rec)std::cout<<r.id<<','<<r.kind<<','<<r.value<<','<<r.begin<<','<<r.end<<'\n';
}
