#include "mpmc_pool.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <vector>

int main(){
    for(int trial=0;trial<100;++trial){
        wg::MPMCPool pool(4,2048);
        std::vector<unsigned> values(1000,0);
        // Each task owns one element; the drain must publish all task writes.
        for(unsigned i=0;i<values.size();++i){
            assert(pool.submit([&,i]{values[i]=i+1;}));
        }
        std::vector<std::thread> waiters;
        std::atomic<int> observed{0};
        for(int w=0;w<8;++w)waiters.emplace_back([&]{
            pool.wait_idle();
            for(unsigned i=0;i<values.size();++i)assert(values[i]==i+1);
            observed.fetch_add(1);
        });
        for(auto& t:waiters)t.join();
        assert(observed==8);
        assert(pool.completed()==1000);
        // Repeated drains and task exceptions must also release every waiter.
        assert(pool.submit([]{throw 7;}));
        pool.wait_idle();
        assert(pool.failed()==1 && pool.completed()==1001);
        pool.shutdown();
        assert(!pool.submit([]{}));
        pool.wait_idle();
    }
}
