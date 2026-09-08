#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
BIN="${BENCHMARK_BIN:-./benchmark}"
TRIALS="${BENCH_TOTAL_TRIALS:-500}"
WARMUPS="${BENCH_WARMUPS:-2}"
COOLDOWN="${BENCH_COOLDOWN_SECONDS:-1}"
CPU="${BENCH_CPU:-0}"
SUITE="${BENCH_SUITE_DIR:-results/baselines/aggregate_default_10k_100k}"
REQ="${BENCH_REQUIRE_CPU_SUBSTR:-}"
MODEL="$(lscpu | awk -F: '/Model name/ {sub(/^[[:space:]]+/, "", $2); print $2; exit}')"
if [[ -n "$REQ" && "$MODEL" != *"$REQ"* ]]; then echo "REFUSING: required CPU $REQ; detected $MODEL" >&2; exit 64; fi
mkdir -p "$SUITE/cells"
ALGS=(physical simulated frozen-single indexed noalloc-direct noalloc-low-run simulated-direct simulated-direct-phase-map-mature 'std::sort')
INPUTS=('Random' 'Sorted' 'Reverse' 'Sorted+Noise(5%)' 'Sorted+Noise(10%)' 'Random%25' 'Alternating' 'Sawtooth' 'MixedDirectionRuns' 'BlockSorted' 'OrganPipe' 'Rotated' 'MixedPhase3' 'MixedPhase12')
sanitize(){ printf '%s' "$1" | sed -E 's/[^A-Za-z0-9._-]+/_/g'; }
if [[ ! -f "$SUITE/cell_order.txt" ]]; then
  for a in "${ALGS[@]}"; do for i in "${INPUTS[@]}"; do printf '10000|%s|%s\n' "$a" "$i"; done; done > "$SUITE/cell_order.txt"
  tmp=$(mktemp)
  for a in "${ALGS[@]}"; do for i in "${INPUTS[@]}"; do printf '100000|%s|%s\n' "$a" "$i"; done; done > "$tmp"
  python3 - "$tmp" >> "$SUITE/cell_order.txt" <<'PY'
import random,sys
items=open(sys.argv[1]).read().splitlines(); random.Random('E466-default-100k-order-v3').shuffle(items); print(*items,sep='\n')
PY
  rm -f "$tmp"
fi
if [[ ! -f "$SUITE/suite_metadata.txt" ]]; then
  cat > "$SUITE/suite_metadata.txt" <<META
method_version=E586-aggregate-binary-cell-atomic-baseline-v1
binary_policy=aggregate canonical benchmark executable; NOT isolated-layout provenance
cpu_model=$MODEL
pinned_cpu=$CPU
sizes=10000,100000
total_trials=$TRIALS
warmups=$WARMUPS
cooldown_seconds=$COOLDOWN
benchmark_binary_sha256=$(sha256sum "$BIN" | awk '{print $1}')
benchmark_layout_sha256=$(grep -o 'BENCH_LAYOUT_SHA256="[^"]*' benchmarks/generated/benchmark_layouts.sh | cut -d\" -f2)
compiler=$(g++ --version | head -1)
META
fi
completed=0; total=$(wc -l < "$SUITE/cell_order.txt")
while IFS='|' read -r n alg input; do
  sa=$(sanitize "$alg"); si=$(sanitize "$input"); d="$SUITE/cells/${n}__${sa}__${si}"
  if [[ -f "$d/benchmark_progress.txt" ]] && grep -q '^state=complete' "$d/benchmark_progress.txt"; then completed=$((completed+1)); continue; fi
  rm -rf "$d"; mkdir -p "$d"
  echo "[$((completed+1))/$total] n=$n alg=$alg input=$input"
  if command -v taskset >/dev/null 2>&1; then taskset -c "$CPU" "$BIN" "$TRIALS" "$n" "$WARMUPS" "$d" "$alg" "$input" >/dev/null; else "$BIN" "$TRIALS" "$n" "$WARMUPS" "$d" "$alg" "$input" >/dev/null; fi
  completed=$((completed+1)); printf 'state=running\ncompleted_cells=%d\ntotal_cells=%d\n' "$completed" "$total" > "$SUITE/progress_state.txt"
  sleep "$COOLDOWN"
done < "$SUITE/cell_order.txt"
printf 'state=complete\ncompleted_cells=%d\ntotal_cells=%d\n' "$completed" "$total" > "$SUITE/progress_state.txt"
python3 - "$SUITE" <<'PY'
import csv, pathlib, sys
s=pathlib.Path(sys.argv[1]); rows=[]
for p in s.glob('cells/*/benchmark_results.csv'):
    rows += list(csv.DictReader(p.open()))
rows.sort(key=lambda r:(int(r['n']),r['algorithm'],r['input']))
fields=['input','n','seed_scope','trials','algorithm','median_us','p25_us','p75_us','iqr_us','mad_us']
with (s/'summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(rows)
PY
