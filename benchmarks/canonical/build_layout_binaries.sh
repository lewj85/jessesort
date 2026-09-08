#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"

BENCH_CONFIG=${BENCH_CONFIG:-benchmarks/config/benchmark_layouts.json}
BENCH_COMPILER=${BENCH_COMPILER:-gcc}
BENCH_CXX=${BENCH_CXX:-g++}
BENCH_CXXFLAGS=${BENCH_CXXFLAGS:--std=c++20 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic}
BENCH_LDFLAGS=${BENCH_LDFLAGS:-}
OUT_ROOT=${BENCH_LAYOUT_BIN_ROOT:-benchmarks/generated/layout_bins/$BENCH_COMPILER}

python3 benchmarks/config/generate_benchmark_layouts.py --config "$BENCH_CONFIG"
# shellcheck disable=SC1091
source benchmarks/generated/benchmark_layouts.sh
mkdir -p "$OUT_ROOT"

for layout in "${ROUTINE_ALGORITHMS[@]}"; do
  safe=${layout//:/_}
  dir="$OUT_ROOT/$safe"
  mkdir -p "$dir"
  python3 benchmarks/config/generate_benchmark_layouts.py \
    --config "$BENCH_CONFIG" --only-layout "$layout" \
    --header "$dir/benchmark_layout.h" \
    --shell "$dir/benchmark_layout.sh" \
    --resolved "$dir/benchmark_layout.resolved.json"
  header=$(cd "$dir" && pwd)/benchmark_layout.h
  macro="-DJESSESORT_BENCH_LAYOUT_HEADER=\"$header\""
  # Intentionally compile each layout from source as its own executable. Do not
  # link unrelated pipeline objects into canonical timing binaries.
  "$BENCH_CXX" $BENCH_CXXFLAGS -Iinclude -Ibenchmarks/infrastructure "$macro" \
    benchmarks/canonical/benchmark.cpp $BENCH_LDFLAGS -o "$dir/canonical"
  "$BENCH_CXX" $BENCH_CXXFLAGS -Iinclude -Ibenchmarks/infrastructure -Ibenchmarks/ood "$macro" \
    benchmarks/ood/benchmark.cpp $BENCH_LDFLAGS -o "$dir/ood"
  printf '%s\t%s\n' "$layout" "$safe"
done > "$OUT_ROOT/layout_manifest.tsv"
