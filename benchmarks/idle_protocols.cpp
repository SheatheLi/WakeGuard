#include "mpmc_pool.hpp"
#include "baselines/mpmc_pool_cv_fixed.hpp"
#include <oneapi/tbb/global_control.h>
#include <oneapi/tbb/task_group.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <numeric>
#include <sys/resource.h>
#include <vector>

using Clock=std::chrono::steady_clock;
double micros(Clock::duration d){return std::chrono::duration<double,std::micro>(d).count();}
double cpu_us(){rusage r{};getrusage(RUSAGE_SELF,&r);return
    r.ru_utime.tv_sec*1e6+r.ru_utime.tv_usec+r.ru_stime.tv_sec*1e6+r.ru_stime.tv_usec;}
double quantile(std::vector<double> v,double p){
    std::sort(v.begin(),v.end());return v[static_cast<std::size_t>(p*(v.size()-1))];
}
template<class Submit,class Drain>
int measure(const char* policy,int busy_us,int trial,Submit submit,Drain drain){
    constexpr int n=2000;
    // Pay lazy scheduler/thread startup before measuring for every executor.
    for(int i=0;i<64;++i){
        if(!submit([]{
            const auto begin=Clock::now();
            while(micros(Clock::now()-begin)<100)std::atomic_signal_fence(std::memory_order_seq_cst);
        }))return 2;
    }
    drain();
    std::vector<double> delays(n);
    std::vector<Clock::time_point> arrivals(n);
    std::atomic<int> completed{0};
    const double start_cpu=cpu_us();const auto start=Clock::now();
    for(int i=0;i<n;++i){
        arrivals[i]=Clock::now();
        const bool accepted=submit([&,i]{
            const auto begin=Clock::now();
            delays[i]=micros(begin-arrivals[i]);
            while(micros(Clock::now()-begin)<busy_us)std::atomic_signal_fence(std::memory_order_seq_cst);
            completed.fetch_add(1,std::memory_order_relaxed);
        });
        if(!accepted)return 2; // No retry hiding a different admission budget.
    }
    drain();const double wall=micros(Clock::now()-start);const double cpu=cpu_us()-start_cpu;
    if(completed.load()!=n)return 3;
    std::cout<<policy<<','<<busy_us<<','<<trial<<','<<n<<','<<completed<<','<<wall<<','
             <<cpu<<','<<quantile(delays,.5)<<','<<quantile(delays,.95)<<','<<quantile(delays,.99)<<'\n';
    return 0;
}
int main(int argc,char**argv){
    if(argc!=4)return 1;
    const std::string policy=argv[1];const int busy=std::stoi(argv[2]),trial=std::stoi(argv[3]);
    if(policy=="atomic"){
        wg::MPMCPool p(4,4096);
        return measure("atomic",busy,trial,[&](auto fn){return p.submit(std::move(fn));},[&]{p.wait_idle();});
    }
    if(policy=="cv_fixed"){
        wg_cv_baseline::MPMCPool p(4,4096);
        return measure("cv_fixed",busy,trial,[&](auto fn){return p.submit(std::move(fn));},[&]{p.wait_idle();});
    }
    if(policy=="oneTBB"){
        oneapi::tbb::global_control limit(oneapi::tbb::global_control::max_allowed_parallelism,4);
        oneapi::tbb::task_group group;
        return measure("oneTBB",busy,trial,[&](auto fn){group.run(std::move(fn));return true;},[&]{group.wait();});
    }
    return 1;
}
