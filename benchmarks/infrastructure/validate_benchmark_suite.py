#!/usr/bin/env python3
"""Validate a canonical benchmark.sh suite before aggregation.

Hard failures: incomplete cells, summary/raw disagreement, trial gaps/duplicates,
metadata method mismatch, or seed schedule mismatch across algorithms for a cell.
"""
import csv, statistics, sys
from pathlib import Path

def median(xs): return statistics.median(xs)

def main(root):
    root=Path(root); errors=[]; cells={}; method=None
    for d in sorted((root/'cells').iterdir()):
        if not d.is_dir(): continue
        raw=d/'benchmark_trials.csv'; summary=d/'benchmark_results.csv'; meta=d/'benchmark_metadata.txt'; prog=d/'benchmark_progress.txt'
        if not all(p.exists() for p in (raw,summary,meta,prog)):
            errors.append(f'{d}: missing required artifact'); continue
        if 'state=complete' not in prog.read_text(): errors.append(f'{d}: incomplete')
        md={}
        for line in meta.read_text().splitlines():
            if '=' in line:
                k,v=line.split('=',1); md[k]=v
        m=md.get('benchmark_method_version')
        if method is None: method=m
        elif m!=method: errors.append(f'{d}: method {m} != {method}')
        rr=list(csv.DictReader(raw.open()))
        sr=list(csv.DictReader(summary.open()))
        if not rr or not sr: errors.append(f'{d}: empty raw/summary'); continue
        alg=rr[0]['algorithm']; inp=rr[0]['input']; n=int(rr[0]['n'])
        if any(r['algorithm']!=alg or r['input']!=inp or int(r['n'])!=n for r in rr): errors.append(f'{d}: mixed raw cell')
        trials=[int(r['trial']) for r in rr]
        if len(set(trials))!=len(trials): errors.append(f'{d}: duplicate trial')
        times=[float(r['time_us']) for r in rr]
        # summary has one row for this selected cell
        matches=[r for r in sr if r['algorithm']==alg and r['input']==inp and int(r['n'])==n]
        if len(matches)!=1: errors.append(f'{d}: expected one summary row, got {len(matches)}')
        else:
            reported=float(matches[0]['median_us']); calc=median(times)
            tol=max(1e-9,abs(calc)*1e-12)
            if abs(reported-calc)>tol: errors.append(f'{d}: median mismatch summary={reported} raw={calc}')
        cells[(alg,inp,n)]=[(int(r['trial']),int(r['seed'])) for r in rr]
    for inp,n in sorted({(i,n) for _,i,n in cells}):
        schedules=[v for (a,i,nn),v in cells.items() if i==inp and nn==n]
        if schedules and any(s!=schedules[0] for s in schedules[1:]): errors.append(f'{inp} n={n}: seed schedule mismatch across algorithms')
    if errors:
        print('EVIDENCE VALIDATION FAILED',file=sys.stderr)
        print('\n'.join(errors),file=sys.stderr); return 2
    print(f'EVIDENCE VALIDATION PASSED cells={len(cells)} method={method}')
    return 0
if __name__=='__main__':
    raise SystemExit(main(sys.argv[1]))
