"""Paired multi-workload benchmark with independent subprocesses and correctness gate.

This is an in-repo baseline only. It is not a Folly/oneTBB comparison.
"""
from __future__ import annotations
import argparse, json, random, statistics, subprocess, pathlib, sys, platform

def run(binary, repeats=9):
    scenarios = [('empty',18000,4,4),('busy20',2400,4,4),('sleep100',2000,4,4)]
    impls=('cv','sem','mpmc_mutex','mpmc_atomic')
    rows=[];rng=random.Random(20261014)
    for mode,tasks,p,c in scenarios:
        for rep in range(repeats):
            randomized=list(impls);rng.shuffle(randomized)
            for impl in randomized:
                args=[str(binary),impl,mode,str(tasks),str(p),str(c)]
                try: proc=subprocess.run(args,text=True,capture_output=True,timeout=8)
                except subprocess.TimeoutExpired: raise RuntimeError(f'benchmark timeout: {args}')
                if proc.returncode: raise RuntimeError(f'benchmark failed: {args}: {proc.stderr[-300:]}')
                item=json.loads(proc.stdout.strip().splitlines()[-1]);item.update(impl=impl,scenario=mode,round=rep)
                if item['missing'] or item['duplicates'] or item['accepted']!=tasks:
                    raise RuntimeError(f'correctness failed: {item}')
                rows.append(item)
    summary=[]
    for mode,_,_,_ in scenarios:
        for impl in impls:
            sub=[x for x in rows if x['scenario']==mode and x['impl']==impl]
            summary.append(dict(scenario=mode,impl=impl,replicates=len(sub),
                wall_ms_median=round(statistics.median(x['wall_ms'] for x in sub),4),
                wall_ms_min=round(min(x['wall_ms'] for x in sub),4),
                wall_ms_max=round(max(x['wall_ms'] for x in sub),4),
                p99_us_median=round(statistics.median(x['enqueue_to_start_p99_us'] for x in sub),4)))
    return dict(environment={'platform':platform.platform(),'python':sys.version.split()[0]},
                rules='randomized within scenario/round; sub-process per run; accepted-only enqueue-to-start; correctness gate',
                scenario_specs=[dict(mode=m,tasks=n,producers=p,consumers=c,queue_capacity=256) for m,n,p,c in scenarios],
                raw=rows,summary=summary,
                limitations=['microbenchmark without Folly/oneTBB','no CPU affinity or RT isolation',
                             'thread scheduler noise may dominate short-task results',
                             'different submitted task payloads not covered'])

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--binary',type=pathlib.Path,required=True)
    parser.add_argument('--out',type=pathlib.Path,required=True);parser.add_argument('--repeats',type=int,default=9)
    args=parser.parse_args();res=run(args.binary,args.repeats)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(res,indent=2),encoding='utf-8')
    print(json.dumps(res['summary'],indent=2))
