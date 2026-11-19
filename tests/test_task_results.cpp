#include "pool.hpp"
#include "semaphore_pool.hpp"
#include "mpmc_pool.hpp"
#include "mpmc_fast_pool.hpp"
#include "task_results.hpp"
#include <atomic>
#include <cassert>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace std::chrono_literals;

template<class P>
void exercise(){
    P p(2,64);
    auto a=wg::try_submit_result(p,[]{return 41+1;});
    assert(a && a->get()==42);
    auto b=wg::try_submit_result(p,[]()->int{throw std::runtime_error("original fault");});
    assert(b);
    bool observed=false;
    try {(void)b->get();}catch(const std::runtime_error&){observed=true;}
    assert(observed);
    auto c=wg::try_submit_result(p,[thing=std::make_unique<int>(73)]{return *thing;});
    assert(c && c->get()==73); // move-only callable supported by admission layer
    p.wait_idle();
    if constexpr (requires(P& pool){ pool.failed(); }) assert(p.failed()==1);
    if constexpr (requires(P& pool){ pool.snapshot(); }) assert(p.snapshot().failed==1);
    p.shutdown();
    assert(!wg::try_submit_result(p,[]{return 2;}));
}

template<class P>
void exercise_worker_lifecycle(){
    P p(1,16);
    std::atomic<bool> idle_rejected{false}, join_rejected{false};
    auto result=wg::try_submit_result(p,[&]{
        try {p.wait_idle();}catch(const std::logic_error&){idle_rejected=true;}
        try {p.shutdown();}catch(const std::logic_error&){join_rejected=true;}
        p.request_stop();
    });
    assert(result);
    result->get();
    p.shutdown();
    assert(idle_rejected && join_rejected && p.stop_requested());
    assert(!p.submit([]{}));
}

void check_backpressure(){
    wg::Pool p(1,1);
    std::promise<void> started, unpark;
    auto release=unpark.get_future().share();
    auto first=wg::try_submit_result(p,[&]{started.set_value();release.wait();return 1;});
    assert(first);
    started.get_future().wait();
    auto queued=wg::try_submit_result(p,[]{return 2;});
    assert(queued);
    auto rejected=wg::try_submit_result(p,[]{return 3;});
    assert(!rejected);
    unpark.set_value();
    assert(first->get()==1 && queued->get()==2);
    p.wait_idle();p.shutdown();
}
int main(){
    exercise<wg::Pool>();
    exercise<wg::SemaphorePool>();
    exercise<wg::MPMCPool>();
    exercise<wg::MPMCFastPool>();
    exercise_worker_lifecycle<wg::Pool>();
    exercise_worker_lifecycle<wg::SemaphorePool>();
    exercise_worker_lifecycle<wg::MPMCFastPool>();
    check_backpressure();
}
