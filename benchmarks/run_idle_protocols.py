"""Repeat the warmed native comparison in a randomized, recorded order."""
from pathlib import Path
import argparse,csv,json,os,platform,random,subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native-library-path',type=Path)
    parser.add_argument('--repetitions',type=int,default=8)
    args=parser.parse_args()
    if not args.binary.is_file() or args.repetitions<1:
        raise ValueError('built native executable and positive repetitions required')
    if not hasattr(os,'sched_getaffinity'):
        raise RuntimeError('this controlled Linux affinity experiment is NOT RUN on this platform')
    cpus=sorted(os.sched_getaffinity(0))[:4]
    if len(cpus)!=4:raise RuntimeError('four allowed CPUs are required for this protocol')
    os.sched_setaffinity(0,set(cpus))
    cases=[(p,b,t) for p in ('atomic','cv_fixed','oneTBB') for b in (5,50,500)
           for t in range(args.repetitions)]
    random.Random(20261015).shuffle(cases)
    environment=os.environ.copy()
    if args.native_library_path:
        environment['LD_LIBRARY_PATH']=str(args.native_library_path.resolve())+os.pathsep+environment.get('LD_LIBRARY_PATH','')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    lines=['policy,busy_us,trial,submitted,completed,wall_us,cpu_us,p50_schedule_us,p95_schedule_us,p99_schedule_us']
    for policy,busy,trial in cases:
        result=subprocess.run([str(args.binary.resolve()),policy,str(busy),str(trial)],
                              env=environment,text=True,capture_output=True,timeout=15)
        if result.returncode:
            raise RuntimeError(f'native run failed: {policy}/{busy}/{trial}: '+result.stderr)
        row=next(csv.DictReader([lines[0],result.stdout.strip()]))
        if row['submitted']!=row['completed'] or row['completed']!='2000':
            raise RuntimeError('correctness denominator mismatch')
        lines.append(result.stdout.strip())
    args.output.write_text('\n'.join(lines)+'\n')
    args.output.with_suffix('.environment.json').write_text(json.dumps({
        'platform':platform.platform(),'affinity':cpus,'repetitions':args.repetitions,
        'randomized_order':cases,'warmup':'64 tasks x 100us for every executor, excluded',
        'library_path':str(args.native_library_path) if args.native_library_path else 'CMake runtime search path',
        'baseline_version':'record the actual TBB dependency used to build this binary',
        'scope':'2000 independent CPU tasks; four CPUs; not production or general Runtime superiority'},indent=2)+'\n')
    print(f'{len(cases)} correct native comparison rows saved')


if __name__=='__main__':main()
