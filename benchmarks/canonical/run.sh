#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"
BENCH_CONFIG=${BENCH_CONFIG:-benchmarks/config/benchmark_layouts.json}
BENCH_COMPILER=${BENCH_COMPILER:-gcc}
BENCH_TRIALS=${BENCH_TRIALS:-500}
BENCH_WARMUPS=${BENCH_WARMUPS:-2}
BENCH_SIZES=${BENCH_SIZES:-"10000 100000"}
BIN_ROOT=${BENCH_LAYOUT_BIN_ROOT:-benchmarks/generated/layout_bins/$BENCH_COMPILER}

if [[ ! -f "$BIN_ROOT/layout_manifest.tsv" || ${BENCH_REBUILD:-1} != 0 ]]; then
  benchmarks/canonical/build_layout_binaries.sh >/dev/null
fi
python3 benchmarks/config/generate_benchmark_layouts.py --config "$BENCH_CONFIG" >/dev/null
# shellcheck disable=SC1091
source benchmarks/generated/benchmark_layouts.sh

if [[ -n ${BENCH_RUN_DIR:-} ]]; then
  RUN_DIR=$BENCH_RUN_DIR
else
  RUN_DIR="results/canonical_$(date -u +%Y%m%dT%H%M%SZ)_${BENCH_COMPILER}"
fi
mkdir -p "$RUN_DIR/cells"
cp benchmarks/generated/benchmark_layouts.resolved.json "$RUN_DIR/benchmark_layouts.resolved.json"
printf 'suite=canonical\ncompiler=%s\ntrials=%s\nwarmups=%s\nsizes=%s\n' \
  "$BENCH_COMPILER" "$BENCH_TRIALS" "$BENCH_WARMUPS" "$BENCH_SIZES" > "$RUN_DIR/suite_metadata.txt"

inputs=("Random" "Sorted" "Reverse" "Sorted+Noise(5%)" "Sorted+Noise(10%)" "Random%25" \
        "Alternating" "Sawtooth" "MixedDirectionRuns" "BlockSorted" "OrganPipe" "Rotated" \
        "MixedPhase3" "MixedPhase12")

for layout in "${ROUTINE_ALGORITHMS[@]}"; do
  safe=${layout//:/_}
  bin="$BIN_ROOT/$safe/canonical"
  [[ -x "$bin" ]] || { echo "missing layout binary: $bin" >&2; exit 2; }
  for n in $BENCH_SIZES; do
    for input in "${inputs[@]}"; do
      cell_safe=$(printf '%s' "$input" | tr -cs 'A-Za-z0-9._-' '_')
      cell_dir="$RUN_DIR/cells/$safe/${n}_${cell_safe}"
      mkdir -p "$cell_dir"
      echo "[$layout] n=$n input=$input"
      "$bin" "$BENCH_TRIALS" "$n" "$BENCH_WARMUPS" "$cell_dir" "$layout" "$input"
    done
  done
done

{
  echo 'input,n,seed_scope,trials,algorithm,median_us,p25_us,p75_us,iqr_us,mad_us'
  find "$RUN_DIR/cells" -name benchmark_results.csv -type f -print0 | sort -z | while IFS= read -r -d '' f; do tail -n +2 "$f"; done
} > "$RUN_DIR/summary.csv"
python3 benchmarks/infrastructure/summary_csv_to_md.py \
  "$RUN_DIR/summary.csv" "$RUN_DIR/summary.md"
echo "Canonical benchmark complete: $RUN_DIR/summary.csv and $RUN_DIR/summary.md"
