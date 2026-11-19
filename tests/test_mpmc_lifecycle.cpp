#include "mpmc_pool.hpp"
#include <atomic>
#include <cassert>
#include <stdexcept>
int main(){
    for(int trial=0;trial<100;++trial){
        wg::MPMCPool pool(2,64);
        std::atomic<int> rejected{0};
        assert(pool.submit([&]{
            try{pool.wait_idle();}catch(const std::logic_error&){++rejected;}
            try{pool.shutdown();}catch(const std::logic_error&){++rejected;}
            pool.request_stop();
        }));
        pool.shutdown(); // joins even if a worker already requested stop
        assert(rejected==2);
        assert(pool.completed()==1 && pool.failed()==0);
        assert(!pool.submit([]{}));
        pool.wait_idle();
        pool.shutdown();
    }
}
