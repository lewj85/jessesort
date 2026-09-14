#!/usr/bin/env python3
"""Render canonical benchmark Markdown from summary.csv.

E760/E761 canonical summaries use CDE history-conditioned medians.
"""
import argparse,csv
from pathlib import Path

CANON=["Random","Sorted","Reverse","Sorted+Noise(5%)","Random%25","Diverging","Sawtooth","MixedDirectionRuns","OrganPipe","Rotated"]
VARIANTS=["simulated-direct_live-phase","noalloc","strict-noalloc"]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('summary_csv');ap.add_argument('-o','--output');a=ap.parse_args()
    src=Path(a.summary_csv); rows=list(csv.DictReader(src.open())); fields=set(rows[0]) if rows else set()
    required={"variant","history_variant_median_us","history_speedup_std_over_variant"}
    if not required <= fields:
        raise SystemExit('summary.csv is not the E761 median-CDE schema; regenerate it from paired_trials.csv')
    indexed={(r['input'],int(r['n']),r['variant']):r for r in rows}
    md=['# Canonical benchmark summary — CDE history-conditioned median only','',
        'Cells show `CDE median µs (std::sort / JesseSort median speedup)`; lower µs and higher × are better. Positions A (fresh) and B (transition) are intentionally excluded from these Markdown tables. Full ABCDE raw observations remain in `paired_trials.csv`; `summary.csv` preserves median diagnostics for all positions.','']
    for n in sorted({int(r['n']) for r in rows}):
        md += [f'## n={n}','', '| Input | simulated-direct_live-phase | noalloc | strict-noalloc |','|---|---:|---:|---:|']
        for name in CANON:
            vals=[]; cells=[]
            for v in VARIANTS:
                r=indexed.get((name,n,v)); vals.append(None if r is None else float(r['history_variant_median_us']))
            finite=[x for x in vals if x is not None]; best=min(finite) if finite else None
            for v,x in zip(VARIANTS,vals):
                if x is None: cells.append('—'); continue
                r=indexed[(name,n,v)]; text=f"{x:.2f} µs ({float(r['history_speedup_std_over_variant']):.2f}×)"
                if x==best:text=f'**{text}**'
                cells.append(text)
            md.append(f"| {name} | "+' | '.join(cells)+' |')
        md.append('')
    out='\n'.join(md)+'\n'
    if a.output:Path(a.output).write_text(out)
    else:print(out,end='')

if __name__=='__main__':main()
