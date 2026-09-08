#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"
BENCH_CONFIG=${BENCH_CONFIG:-benchmarks/config/benchmark_layouts.json}
BENCH_COMPILER=${BENCH_COMPILER:-gcc}
OOD_N=${OOD_N:-100000}
OOD_TRIALS=${OOD_TRIALS:-500}
OOD_WARMUPS=${OOD_WARMUPS:-2}
BIN_ROOT=${BENCH_LAYOUT_BIN_ROOT:-benchmarks/generated/layout_bins/$BENCH_COMPILER}

if [[ ! -f "$BIN_ROOT/layout_manifest.tsv" || ${BENCH_REBUILD:-1} != 0 ]]; then
  benchmarks/canonical/build_layout_binaries.sh >/dev/null
fi
python3 benchmarks/config/generate_benchmark_layouts.py --config "$BENCH_CONFIG" >/dev/null
# shellcheck disable=SC1091
source benchmarks/generated/benchmark_layouts.sh

if [[ -n ${OOD_RUN_DIR:-} ]]; then
  RUN_DIR=$OOD_RUN_DIR
else
  RUN_DIR="results/ood_$(date -u +%Y%m%dT%H%M%SZ)_${BENCH_COMPILER}"
fi
mkdir -p "$RUN_DIR/cells"
cp benchmarks/generated/benchmark_layouts.resolved.json "$RUN_DIR/benchmark_layouts.resolved.json"
printf 'suite=expanded_ood_47\ncompiler=%s\nn=%s\ntrials=%s\nwarmups=%s\nseed_family=E692/E693-compatible\n' \
  "$BENCH_COMPILER" "$OOD_N" "$OOD_TRIALS" "$OOD_WARMUPS" > "$RUN_DIR/suite_metadata.txt"

first_safe=${ROUTINE_ALGORITHMS[0]//:/_}
mapfile -t families < <("$BIN_ROOT/$first_safe/ood" --list-families)
if [[ ${#families[@]} -ne 47 ]]; then
  echo "expected 47 OOD families, found ${#families[@]}" >&2
  exit 3
fi

for layout in "${ROUTINE_ALGORITHMS[@]}"; do
  safe=${layout//:/_}
  bin="$BIN_ROOT/$safe/ood"
  [[ -x "$bin" ]] || { echo "missing layout binary: $bin" >&2; exit 2; }
  for family in "${families[@]}"; do
    out="$RUN_DIR/cells/${safe}__${family}.csv"
    if [[ -s "$out" ]]; then
      continue
    fi
    echo "[$layout] OOD=$family n=$OOD_N"
    tmp="$out.tmp"
    "$bin" "$family" "$OOD_N" "$OOD_TRIALS" "$OOD_WARMUPS" > "$tmp"
    mv "$tmp" "$out"
  done
done

{
  echo 'family,n,trials,algorithm,median_us'
  find "$RUN_DIR/cells" -name '*.csv' -type f -print0 | sort -z | while IFS= read -r -d '' f; do cat "$f"; done
} > "$RUN_DIR/summary.csv"
echo "OOD benchmark complete: $RUN_DIR/summary.csv"
