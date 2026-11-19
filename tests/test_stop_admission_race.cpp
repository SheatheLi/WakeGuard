// Native concurrency contract: request_stop racing accepted submissions.
// Repetitions search for a narrow interleaving, but are not a proof of all
// executions. Every accepted std::future must become ready before owner join.
#include "pool.hpp"
#include "semaphore_pool.hpp"
#include "mpmc_pool.hpp"
#include "mpmc_fast_pool.hpp"
#include "task_results.hpp"
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include <iostream>
using namespace std::chrono_literals;

template<class P>
void race(std::size_t rounds){
    std::size_t accepted_total=0;
    for(std::size_t round=0; round<rounds; ++round){
        P pool(3,32);
        std::barrier start(3);
        std::vector<std::future<unsigned>> futures;
        std::atomic<unsigned> invoked{0};
        std::thread producer([&]{
            start.arrive_and_wait();
            for(unsigned i=0;i<40;++i){
                auto result=wg::try_submit_result(pool,[&,i]{
                    invoked.fetch_add(1,std::memory_order_relaxed);
                    return i;
                });
                if(result)futures.push_back(std::move(*result));
                if(i%7==0)std::this_thread::yield();
            }
        });
        std::thread stopper([&]{
            start.arrive_and_wait();
            if(round%2==0)std::this_thread::yield();
            pool.request_stop();
        });
        start.arrive_and_wait();
        producer.join();stopper.join();
        pool.shutdown(); // after admitted submitter has finished its publication
        unsigned count=0;
        for(auto &future:futures){
            assert(future.wait_for(1s)==std::future_status::ready);
            (void)future.get();++count;
        }
        assert(invoked.load(std::memory_order_relaxed)==count);
        assert(!pool.submit([]{}));
        accepted_total+=count;
    }
    std::cout<<"rounds="<<rounds<<" accepted="<<accepted_total<<"\n";
}
int main(){
    race<wg::Pool>(32);
    race<wg::SemaphorePool>(32);
    race<wg::MPMCPool>(32);
    race<wg::MPMCFastPool>(32);
}
