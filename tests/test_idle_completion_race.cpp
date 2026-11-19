#include "mpmc_pool.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <semaphore>

// Drives the production wait/completion paths, not a separate boolean model.
// A watchdog exits the *process* so a broken wait cannot hang CTest forever.
int main(){
    using namespace std::chrono_literals;
    wg::MPMCPool pool(1,8);
    std::binary_semaphore at_wait_edge(0),resume_waiter(0),finish_task(0),notified(0);
    std::atomic<bool> once{false};
    pool.idle_test_hooks([&]{
        if(!once.exchange(true)){
            at_wait_edge.release();
            resume_waiter.acquire();
        }
    },[&]{notified.release();});
    if(!pool.submit([&]{finish_task.acquire();}))return 1;
    auto waiter=std::async(std::launch::async,[&]{pool.wait_idle();});
    at_wait_edge.acquire();
    finish_task.release();
    notified.acquire(); // pending=0 and completion notification already happened
    resume_waiter.release();
    if(waiter.wait_for(250ms)!=std::future_status::ready){
        std::cout<<"LOST_COMPLETION_WAKE: pending reached zero before waiter enrolled\n"<<std::flush;
        std::_Exit(42);
    }
    waiter.get();
    pool.shutdown();
    std::cout<<"IDLE_COMPLETION_RACE_OK completed="<<pool.completed()<<"\n";
    return pool.completed()==1?0:1;
}
