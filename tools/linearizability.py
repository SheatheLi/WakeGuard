"""Bounded exact queue linearizability checker: enumerates legal linearizations.

A positive result proves ONLY the supplied finite history can be linearized;
not wait-freedom, memory safety, all histories, or generic pool correctness.
"""
from dataclasses import dataclass
import csv
from functools import lru_cache
from pathlib import Path

@dataclass(frozen=True)
class Op:
    ident:int
    kind:str
    value:int
    begin:int
    end:int

def valid_history(ops):
    ids=[x.ident for x in ops]
    return len(set(ids))==len(ids) and all(x.begin<x.end and x.kind in ('PUSH','POP') for x in ops)

def linearizable(ops):
    ops=tuple(ops)
    if len(ops)>18:raise ValueError('exact search supports at most 18 operations')
    if not valid_history(ops):return False
    n=len(ops); required=[]
    for i,op in enumerate(ops):
        mask=0
        for j,other in enumerate(ops):
            if j!=i and other.end<op.begin:mask|=1<<j
        required.append(mask)
    @lru_cache(None)
    def solve(mask,queue):
        if mask==(1<<n)-1:return True
        for i,op in enumerate(ops):
            bit=1<<i
            if mask&bit or required[i]&~mask:continue
            if op.kind=='PUSH': new=queue+(op.value,)
            elif queue and queue[0]==op.value:new=queue[1:]
            else:continue
            if solve(mask|bit,new):return True
        return False
    return solve(0,())

def load(path):
    with open(path,newline='') as f:
        return [Op(int(x['id']),x['op'],int(x['value']),int(x['begin']),int(x['end'])) for x in csv.DictReader(f)]

if __name__=='__main__':
    import sys,json
    op=load(Path(sys.argv[1]));print(json.dumps({'operations':len(op),'linearizable':linearizable(op)}))
