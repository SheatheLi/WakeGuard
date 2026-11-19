#include "mpmc_ring.hpp"
#include <cassert>
#include <stdexcept>
int main(){
  for (std::size_t n : {std::size_t(0),std::size_t(1),std::size_t(3),std::size_t(1)<<30}) {
    bool rejected=false;
    try { wg::MPMCRing<int> q(n); } catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
  }
  wg::MPMCRing<int> valid(4); int x=0;
  assert(valid.try_push(23));assert(valid.try_pop(x));assert(x==23);
}
