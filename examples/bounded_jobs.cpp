#include "pool.hpp"
#include "task_results.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

int main(){
    wg::Pool executor(4, 16);
    std::vector<std::future<int>> results;
    for(int i=1;i<=100;++i){
        for(;;){
            auto accepted=wg::try_submit_result(executor,[i]{
                if(i==70) throw std::runtime_error("bad job");
                return i*i;
            });
            if(accepted){ results.push_back(std::move(*accepted));break; }
            // Bounded admission applies backpressure; real clients can also
            // choose a deadline, retry budget, or reject instead of spinning.
            std::this_thread::yield();
        }
    }
    long long total=0;
    int failures=0;
    for(auto& future:results){
        try{total+=future.get();}catch(const std::runtime_error&){++failures;}
    }
    executor.wait_idle();
    executor.shutdown();
    const auto metrics=executor.snapshot();
    assert(metrics.pending==0 && metrics.completed==100 && metrics.failed==1);
    assert(failures==1 && total==338350-4900);
    std::cout<<"jobs="<<metrics.completed<<" failed="<<metrics.failed
             <<" sum="<<total<<" stopped="<<metrics.stopping<<'\n';
}
