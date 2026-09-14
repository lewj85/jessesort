#!/usr/bin/env python3
import argparse,csv,hashlib,json,math,os,statistics,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
VARIANTS=['simulated-direct_live-phase','noalloc','strict-noalloc']
STD='std::sort'
CANON=['Random','Sorted','Reverse','Sorted+Noise(5%)','Random%25','Diverging','Sawtooth','MixedDirectionRuns','OrganPipe','Rotated']
POSITIONS='ABCDE'

def safe(s): return s.replace(':','_')
def key(seed,*parts):
    h=hashlib.sha256((str(seed)+'|'+'|'.join(map(str,parts))).encode()).digest()
    return int.from_bytes(h[:8],'little')
def coprime_multiplier(total,preferred=137):
    if total<=1:return 1
    a=preferred%total
    if a==0:a=1
    while math.gcd(a,total)!=1:
        a=(a+2)%total
        if a==0:a=1
    return a
def trial_ids(exec_index,execs):
    # Exact bijection over 0..5*execs-1.  Each exec receives one member from
    # each fifth of the domain; position assignment rotates by exec index, then
    # a fixed affine permutation removes simple numeric ordering.
    total=5*execs; mult=coprime_multiplier(total); off=31%total
    ids=[]
    for p in range(5):
        stratum=(p+exec_index)%5
        x=exec_index + execs*stratum
        ids.append((mult*x+off)%total)
    return ids

def run_five(binroot,cpu,suite,name,n,ids,total,layout):
    b=binroot/safe(layout)/'five_trial'
    cmd=[str(b),suite,name,str(n),str(total)]+[str(x) for x in ids]
    if cpu!='none':cmd=['taskset','-c',str(cpu)]+cmd
    p=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,check=True)
    lines=[x for x in p.stdout.splitlines() if x.strip()]
    if len(lines)!=5:raise RuntimeError(f'bad five-trial output ({layout}): {p.stdout!r} {p.stderr!r}')
    out=[]
    for line in lines:
        f=line.split(',')
        if len(f)!=7:raise RuntimeError(f'bad five-trial row: {line!r}')
        out.append((int(f[0]),int(f[3]),int(f[4]),float(f[6])))
    out.sort()
    return out

def ood_names(binroot):
    b=binroot/safe(VARIANTS[0])/'ood'
    return subprocess.check_output([str(b),'--list-families'],cwd=ROOT,text=True).splitlines()

def round_path(run_dir,rep):
    return run_dir/'rounds'/f'round_{rep:03d}.csv'

def write_round_journal(run_dir,rows,rep):
    fields=['exec','position','position_index','trial_id','seed','suite','input','n','variant','variant_us','std_us','speedup_std_over_variant','pair_order','variant_position']
    rdir=run_dir/'rounds';rdir.mkdir(parents=True,exist_ok=True)
    dst=round_path(run_dir,rep);tmp=dst.with_suffix('.tmp')
    with tmp.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows);f.flush();os.fsync(f.fileno())
    os.replace(tmp,dst)
    dfd=os.open(rdir,os.O_RDONLY)
    try: os.fsync(dfd)
    finally: os.close(dfd)

def durable_rounds(run_dir,execs,expected_rows=None):
    done=[]
    for rep in range(execs):
        p=round_path(run_dir,rep)
        if not p.exists(): continue
        with p.open(newline='') as f:
            rows=list(csv.DictReader(f))
        valid=rows and all(int(r['exec'])==rep for r in rows)
        if expected_rows is not None: valid=valid and len(rows)==expected_rows
        if valid: done.append(rep)
    return done

def rebuild_master(run_dir,execs,expected_rows=None):
    fields=['exec','position','position_index','trial_id','seed','suite','input','n','variant','variant_us','std_us','speedup_std_over_variant','pair_order','variant_position']
    tmp=run_dir/'paired_trials.tmp.csv';dst=run_dir/'paired_trials.csv'
    with tmp.open('w',newline='') as out:
        w=csv.DictWriter(out,fieldnames=fields);w.writeheader()
        for rep in range(execs):
            p=round_path(run_dir,rep)
            if not p.exists(): raise RuntimeError(f'missing durable round {rep}')
            with p.open(newline='') as f:
                rs=list(csv.DictReader(f))
            if expected_rows is not None and len(rs)!=expected_rows: raise RuntimeError(f'round {rep} row count {len(rs)} != {expected_rows}')
            for r in rs: w.writerow(r)
        out.flush();os.fsync(out.fileno())
    os.replace(tmp,dst)

def percentile(xs,p):
    ys=sorted(xs)
    if not ys:return float('nan')
    z=(len(ys)-1)*p;lo=int(math.floor(z));hi=int(math.ceil(z))
    if lo==hi:return ys[lo]
    return ys[lo]+(ys[hi]-ys[lo])*(z-lo)

def avg(xs):return sum(xs)/len(xs)
def gmean(xs):return math.exp(avg([math.log(x) for x in xs]))
def channel_stats(rs,positions):
    s=[r for r in rs if int(r['position_index']) in positions]
    vu=[float(r['variant_us']) for r in s];su=[float(r['std_us']) for r in s];pr=[float(r['speedup_std_over_variant']) for r in s]
    vmed=statistics.median(vu);smed=statistics.median(su)
    return {'n':len(s),'variant_median':vmed,'std_median':smed,'speedup':smed/vmed,'paired_gmean':gmean(pr),'paired_median':statistics.median(pr),'win_rate':sum(x>1 for x in pr)/len(pr)}

def summarize(run_dir):
    rows=list(csv.DictReader((run_dir/'paired_trials.csv').open()));groups={}
    for r in rows:groups.setdefault((r['suite'],r['input'],int(r['n']),r['variant']),[]).append(r)
    out=[]
    for (suite,name,n,var),rs in sorted(groups.items(),key=lambda x:(x[0][2],x[0][1],x[0][3])):
        rs.sort(key=lambda r:(int(r['exec']),int(r['position_index'])))
        fresh=channel_stats(rs,{0});transition=channel_stats(rs,{1});hist=channel_stats(rs,{2,3,4});overall=channel_stats(rs,{0,1,2,3,4})
        vu=[float(r['variant_us']) for r in rs];su=[float(r['std_us']) for r in rs]
        hist_v=[float(r['variant_us']) for r in rs if int(r['position_index']) in {2,3,4}]
        pos=[]
        for p in range(5):
            ps=[r for r in rs if int(r['position_index'])==p]
            pv=statistics.median([float(r['variant_us']) for r in ps]);pu=statistics.median([float(r['std_us']) for r in ps])
            pos.extend([pv,pu,pu/pv])
        sd=statistics.stdev(vu) if len(vu)>1 else 0.;hsd=statistics.stdev(hist_v) if len(hist_v)>1 else 0.
        d=dict(suite=suite,input=name,n=n,variant=var,execs=len({r['exec'] for r in rs}),observations=len(rs),
            fresh_variant_median_us=fresh['variant_median'],fresh_std_median_us=fresh['std_median'],fresh_speedup_std_over_variant=fresh['speedup'],
            transition_B_variant_median_us=transition['variant_median'],transition_B_std_median_us=transition['std_median'],transition_B_speedup_std_over_variant=transition['speedup'],
            history_variant_median_us=hist['variant_median'],history_std_median_us=hist['std_median'],history_speedup_std_over_variant=hist['speedup'],
            overall_variant_median_us=overall['variant_median'],overall_std_median_us=overall['std_median'],overall_speedup_std_over_variant=overall['speedup'],
            variant_p10_us=percentile(vu,.10),variant_p25_us=percentile(vu,.25),variant_p75_us=percentile(vu,.75),variant_p90_us=percentile(vu,.90),
            variant_sd_us=sd,history_variant_sd_us=hsd,
            fresh_paired_gmean_speedup=fresh['paired_gmean'],transition_B_paired_gmean_speedup=transition['paired_gmean'],history_paired_gmean_speedup=hist['paired_gmean'],overall_paired_gmean_speedup=overall['paired_gmean'],
            fresh_win_rate=fresh['win_rate'],transition_B_win_rate=transition['win_rate'],history_win_rate=hist['win_rate'],overall_win_rate=overall['win_rate'])
        for p,label in enumerate(POSITIONS):
            d[f'{label}_variant_median_us']=pos[3*p];d[f'{label}_std_median_us']=pos[3*p+1];d[f'{label}_speedup']=pos[3*p+2]
        out.append(d)
    with (run_dir/'summary.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=out[0].keys());w.writeheader();w.writerows(out)
    ag=[]
    for v in VARIANTS:
        xs=[r for r in out if r['variant']==v]
        if xs:
            ag.append(dict(variant=v,cells=len(xs),fresh_geomean_speedup=gmean([r['fresh_speedup_std_over_variant'] for r in xs]),history_geomean_speedup=gmean([r['history_speedup_std_over_variant'] for r in xs]),overall_geomean_speedup=gmean([r['overall_speedup_std_over_variant'] for r in xs])))
    with (run_dir/'aggregate.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=ag[0].keys());w.writeheader();w.writerows(ag)
    if all(r['suite']=='canonical' for r in out):
        indexed={(r['input'],int(r['n']),r['variant']):r for r in out}
        md=['# Canonical benchmark summary — CDE history-conditioned median only','',
            'Cells show `CDE median µs (std::sort / JesseSort median speedup)`; lower µs and higher × are better. Positions A (fresh) and B (transition) are intentionally excluded from these Markdown tables. Full ABCDE raw observations remain in `paired_trials.csv`; `summary.csv` preserves median diagnostics for all positions.','']
        for n in sorted({int(r['n']) for r in out}):
            md += [f'## n={n}','',
                   '| Input | simulated-direct_live-phase | noalloc | strict-noalloc |',
                   '|---|---:|---:|---:|']
            for name in CANON:
                cells=[];vals=[]
                for v in VARIANTS:
                    r=indexed.get((name,n,v))
                    if r is None: vals.append(None); continue
                    vals.append(float(r['history_variant_median_us']))
                finite=[x for x in vals if x is not None];best=min(finite) if finite else None
                for v,x in zip(VARIANTS,vals):
                    if x is None: cells.append('—'); continue
                    r=indexed[(name,n,v)]
                    text=f"{x:.2f} µs ({float(r['history_speedup_std_over_variant']):.2f}×)"
                    if x==best:text=f'**{text}**'
                    cells.append(text)
                md.append(f"| {name} | "+' | '.join(cells)+' |')
            md.append('')
    else:
        md=['# E760 CDE-history process benchmark','',
            'Primary cell estimates are medians. Fresh = position A; Transition = position B; History = positions C–E. Suite aggregates are geometric means of per-cell median speedups.','',
            '| Input | n | Variant | Fresh JS µs | Fresh std µs | **Fresh ×** | B transition JS µs | **B ×** | History CDE JS µs | History std µs | **History ×** | Overall JS µs | **Overall ×** |',
            '|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
        for r in out:
            md.append(f"| {r['input']} | {r['n']} | {r['variant']} | {r['fresh_variant_median_us']:.2f} | {r['fresh_std_median_us']:.2f} | **{r['fresh_speedup_std_over_variant']:.3f}×** | {r['transition_B_variant_median_us']:.2f} | **{r['transition_B_speedup_std_over_variant']:.3f}×** | {r['history_variant_median_us']:.2f} | {r['history_std_median_us']:.2f} | **{r['history_speedup_std_over_variant']:.3f}×** | {r['overall_variant_median_us']:.2f} | **{r['overall_speedup_std_over_variant']:.3f}×** |")
    (run_dir/'summary.md').write_text('\n'.join(md)+'\n')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--suite',choices=['canonical','ood','all60'],default='canonical');ap.add_argument('--execs',type=int,default=100);ap.add_argument('--sizes',default='10000,100000');ap.add_argument('--n',type=int,default=100000);ap.add_argument('--cpu',default='0');ap.add_argument('--schedule-seed',type=lambda x:int(x,0),default=0xE759);ap.add_argument('--run-dir');ap.add_argument('--compiler',default='gcc');a=ap.parse_args()
    if a.execs<=0:raise SystemExit('--execs must be positive')
    total=5*a.execs;binroot=ROOT/f'benchmarks/generated/layout_bins/{a.compiler}'
    if not all((binroot/safe(x)/'five_trial').exists() for x in VARIANTS+[STD]):
        subprocess.run([str(ROOT/'benchmarks/canonical/build_layout_binaries.sh')],cwd=ROOT,check=True)
    ood=ood_names(binroot);blocks=[]
    if a.suite=='canonical':
        for n in map(int,a.sizes.split(',')):blocks += [('canonical',x,n) for x in CANON]
    elif a.suite=='ood':blocks=[('ood',x,a.n) for x in ood]
    else:blocks=[('canonical',x,a.n) for x in CANON]+[('ood',x,a.n) for x in ood]
    rd=Path(a.run_dir) if a.run_dir else ROOT/f"results/e760_{a.suite}_{time.strftime('%Y%m%dT%H%M%SZ',time.gmtime())}_{a.compiler}"
    rd.mkdir(parents=True,exist_ok=True)
    meta={'method':'E760-ABCDE-CDE-history-median-v3','suite':a.suite,'execs_per_cell':a.execs,'timed_sorts_per_exec':5,'observations_per_cell':total,'blocks':len(blocks),'cpu':a.cpu,'schedule_seed':a.schedule_seed,'compiler':a.compiler,'target_warmups':0,'seed_policy':'all 5*execs timed trial IDs used exactly once; each exec gets one ID from each fifth of domain; position/stratum rotates; fixed affine permutation','process_trajectory':'five distinct preconstructed inputs A,B,C,D,E; A is fresh; B is transitional after one prior sort; C-E are canonical history-conditioned after at least two prior sorts','pairing':'each JesseSort variant gets its own immediately adjacent std::sort process using identical five seed IDs/order; AB/BA balanced by exec','scheduling':'exec round outer; input/size blocks and variant-pair order deterministically SHA256 shuffled each round','primary_cell_statistics':'median runtime; Fresh=A, Transition=B, canonical History=C-E; speedup is ratio of channel medians','suite_aggregate':'geometric mean of per-cell speedups','secondary_statistics':'A-E position medians, p10/p25/p75/p90, SD, paired geometric-mean ratios, win rate','cooldown':'none; continuous round-robin load'}
    (rd/'metadata.json').write_text(json.dumps(meta,indent=2))
    expected_round_rows=len(blocks)*len(VARIANTS)*5
    done=set(durable_rounds(rd,a.execs,expected_round_rows))
    for rep in range(a.execs):
        if rep in done: continue
        ids=trial_ids(rep,a.execs);ordered=sorted(blocks,key=lambda b:key(a.schedule_seed,rep,*b));round_rows=[]
        for suite,name,n in ordered:
            variants=sorted(VARIANTS,key=lambda v:key(a.schedule_seed,rep,suite,name,n,v,'variant_order'))
            for vp,v in enumerate(variants):
                ab=((rep + (key(a.schedule_seed,suite,name,n,v,'ab')&1)) & 1)==0
                if ab:
                    vr=run_five(binroot,a.cpu,suite,name,n,ids,total,v);sr=run_five(binroot,a.cpu,suite,name,n,ids,total,STD);po='A/B'
                else:
                    sr=run_five(binroot,a.cpu,suite,name,n,ids,total,STD);vr=run_five(binroot,a.cpu,suite,name,n,ids,total,v);po='B/A'
                for p,(vv,ss) in enumerate(zip(vr,sr)):
                    pi,seed,tid,vu=vv;pi2,seed2,tid2,su=ss
                    if pi!=pi2 or seed!=seed2 or tid!=tid2:raise RuntimeError('paired trajectory mismatch')
                    round_rows.append(dict(exec=rep,position=POSITIONS[p],position_index=p,trial_id=tid,seed=seed,suite=suite,input=name,n=n,variant=v,variant_us=vu,std_us=su,speedup_std_over_variant=su/vu,pair_order=po,variant_position=vp))
        write_round_journal(rd,round_rows,rep)
        done.add(rep);(rd/'completed_round.txt').write_text(str(len(done)))
        if (rep+1)%10==0 or rep+1==a.execs:print(f'E760 {a.suite}: exec round {rep+1}/{a.execs}',flush=True)
    if len(durable_rounds(rd,a.execs,expected_round_rows))!=a.execs: raise RuntimeError('not all rounds durably journaled')
    rebuild_master(rd,a.execs,expected_round_rows);summarize(rd);print(rd)
if __name__=='__main__':main()
