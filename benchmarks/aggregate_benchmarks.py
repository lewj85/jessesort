#!/usr/bin/env python3
import csv, sys
from pathlib import Path

if len(sys.argv) < 3:
    raise SystemExit('usage: aggregate_benchmarks.py SUITE_DIR algorithm=summary.csv [...]')

suite = Path(sys.argv[1])
entries = []
for item in sys.argv[2:]:
    name, path = item.split('=', 1)
    entries.append((name, Path(path)))

rows = {}
inputs = []
sizes = []
for alg, path in entries:
    with path.open(newline='') as f:
        for row in csv.DictReader(f):
            key = (row['input'], int(row['n']))
            if row['input'] not in inputs:
                inputs.append(row['input'])
            if int(row['n']) not in sizes:
                sizes.append(int(row['n']))
            rows[(alg,) + key] = row

sizes.sort()
if 'std::sort' not in {x[0] for x in entries}:
    raise SystemExit('std::sort result is required for aggregation')

out_csv = suite / 'benchmark_results.csv'
with out_csv.open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['input','n','algorithm','median_us','p25_us','p75_us','iqr_us','mad_us','std_median_us','ratio_of_medians'])
    algs = list(dict.fromkeys(a for a,_ in entries))
    for n in sizes:
        for inp in inputs:
            std = rows.get(('std::sort', inp, n))
            if not std: continue
            sm = float(std['median_us'])
            for alg in algs:
                r = rows.get((alg, inp, n))
                if not r: continue
                m = float(r['median_us'])
                w.writerow([inp,n,alg,r['median_us'],r['p25_us'],r['p75_us'],r['iqr_us'],r['mad_us'],std['median_us'],m/sm])

out_md = suite / 'benchmark_results.md'
with out_md.open('w') as f:
    f.write('# JesseSort isolated-process benchmark results\n\n')
    f.write('Each algorithm/input cell was measured in a separate process using the same deterministic seed policy. ')
    f.write('Ratios are **ratio of independent medians vs std::sort**, not paired per-trial ratios.\n\n')
    for n in sizes:
        f.write(f'## n={n}\n\n')
        algs = list(dict.fromkeys(a for a,_ in entries))
        f.write('| Input | ' + ' | '.join(algs) + ' |\n')
        f.write('|---|' + '|'.join('---:' for _ in algs) + '|\n')
        for inp in inputs:
            std = rows.get(('std::sort', inp, n))
            if not std: continue
            sm = float(std['median_us'])
            cells=[]
            for alg in algs:
                r=rows.get((alg, inp,n))
                if not r:
                    cells.append('pending')
                else:
                    m=float(r['median_us'])
                    cells.append(f'{m/sm:.4f} ({m:.3f} us)' if alg != 'std::sort' else f'1.0000 ({m:.3f} us)')
            f.write('| ' + inp + ' | ' + ' | '.join(cells) + ' |\n')
        f.write('\n')
print(out_md)
print(out_csv)
