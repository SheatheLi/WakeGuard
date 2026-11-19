"""Explicitly unsafe baseline is run ONLY in an isolated subprocess.

The old worker->shutdown() process is EXPECTED to abort. Never execute it
in the caller process or production worker. Requires g++ on Linux.
"""
from pathlib import Path
import json,subprocess,tempfile,resource

ROOT=Path(__file__).resolve().parents[1]
TEMPLATE=r'''
#include "mpmc_fast_pool.hpp"
#include <atomic>
int main(){std::atomic<int> called{0};{
wg::MPMCFastPool p(2,16);
p.submit([&]{called++;p.%s();});
p.wait_idle();
}return called.load()!=1;}
'''

def run():
    results={}
    with tempfile.TemporaryDirectory() as temp:
        p=Path(temp)
        for kind in ('old','new'):
            include_dir=p/kind;include_dir.mkdir()
            if kind=='old':
                (include_dir/'mpmc_fast_pool.hpp').write_bytes((ROOT/'benchmarks/baselines/mpmc_fast_pool_before.hpp').read_bytes())
            else:(include_dir/'mpmc_fast_pool.hpp').write_bytes((ROOT/'src/mpmc_fast_pool.hpp').read_bytes())
            (include_dir/'mpmc_ring.hpp').write_bytes((ROOT/'src/mpmc_ring.hpp').read_bytes())
            program=p/(kind+'.cpp');program.write_text(TEMPLATE%('shutdown' if kind=='old' else 'request_stop'))
            exe=p/(kind+'.bin')
            subprocess.run(['g++','-std=c++20','-O2','-pthread','-I',str(include_dir),str(program),'-o',str(exe)],check=True,timeout=45)
            def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
            result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10,preexec_fn=no_core)
            results[kind]={'exit_code':result.returncode,'stderr_tail':result.stderr[-160:]}
    return {'results':results,'scope':'Exact prior local header vs current worker-safe request_stop; not Folly upstream issue.'}
if __name__=='__main__':print(json.dumps(run(),indent=2))
