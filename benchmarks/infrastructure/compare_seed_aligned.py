#!/usr/bin/env python3
"""Compare two isolated-process benchmark_trials.csv files on the same seeds.
Reports both ratio-of-medians and median seed-paired ratio. A direction/sign
conflict around 1.0 is an audit flag, not a retention result.
"""
import csv, statistics, sys

def load(path):
    out={}
    with open(path,newline='') as f:
        for r in csv.DictReader(f):
            key=(r['input'],int(r['n']),int(r['trial']),int(r['seed']))
            out[key]=float(r['time_us'])
    return out
if len(sys.argv)!=3: raise SystemExit('usage: compare_seed_aligned.py CONTROL_TRIALS.csv CANDIDATE_TRIALS.csv')
a=load(sys.argv[1]); b=load(sys.argv[2])
if set(a)!=set(b):
    missing_a=set(b)-set(a);missing_b=set(a)-set(b)
    raise SystemExit(f'seed/trial mismatch: candidate_only={len(missing_a)} control_only={len(missing_b)}')
va=[a[k] for k in sorted(a)]; vb=[b[k] for k in sorted(b)]
rom=statistics.median(vb)/statistics.median(va)
paired=statistics.median(b[k]/a[k] for k in sorted(a))
conflict=(rom-1)*(paired-1)<0 and abs(rom-1)>0.001 and abs(paired-1)>0.001
print(f'n={len(a)} control_median_us={statistics.median(va):.9g} candidate_median_us={statistics.median(vb):.9g} ratio_of_medians={rom:.9g} median_seed_paired_ratio={paired:.9g} direction_conflict={int(conflict)}')
if conflict: raise SystemExit(3)
