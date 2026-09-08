#!/usr/bin/env python3
import csv, hashlib, json, statistics, sys
from pathlib import Path

def sha256(p):
    h=hashlib.sha256();
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(1<<20),b''): h.update(b)
    return h.hexdigest()

def load_trials(path):
    with open(path,newline='') as f: rows=list(csv.DictReader(f))
    for r in rows:
        r['n']=int(r['n']); r['seed']=int(r['seed']); r['trial']=int(r['trial']); r['time_us']=float(r['time_us'])
    return rows

def med(xs): return statistics.median(xs)

def validate_canonical(root):
    root=Path(root)
    groups={}
    errors=[]
    for d in root.iterdir():
        if not d.is_dir(): continue
        p=d/'benchmark_trials.csv'
        m=d/'benchmark_metadata.txt'
        prog=d/'benchmark_progress.txt'
        if not p.exists(): continue
        if not prog.exists() or 'state=complete' not in prog.read_text(): errors.append(f'incomplete {d}')
        rows=load_trials(p)
        if not rows: errors.append(f'no rows {d}'); continue
        alg=rows[0]['algorithm']; inp=rows[0]['input']; n=rows[0]['n']
        if any(r['algorithm']!=alg or r['input']!=inp or r['n']!=n for r in rows): errors.append(f'mixed cell {d}')
        if len(rows)!=500: errors.append(f'{d}: expected 500 rows, got {len(rows)}')
        trials=[r['trial'] for r in rows]
        if sorted(trials)!=list(range(500)): errors.append(f'{d}: trial ids invalid')
        groups[(alg,inp,n)]=rows
    inputs=sorted({(inp,n) for _,inp,n in groups})
    for inp,n in inputs:
        present=[a for a in ('e359','e362','std::sort') if (a,inp,n) in groups]
        if len(present)!=3: errors.append(f'missing algs {inp} {n}: {present}'); continue
        seedsets=[]
        for a in present:
            seedsets.append([(r['trial'],r['seed']) for r in groups[(a,inp,n)]])
        if not (seedsets[0]==seedsets[1]==seedsets[2]): errors.append(f'seed mismatch {inp} {n}')
    if errors:
        print('\n'.join(errors),file=sys.stderr); raise SystemExit(2)
    return groups

if __name__=='__main__':
    if len(sys.argv)!=2: raise SystemExit('usage: validate_evidence.py CANONICAL_ROOT')
    groups=validate_canonical(sys.argv[1]); print(f'VALID canonical cells={len(groups)}')
