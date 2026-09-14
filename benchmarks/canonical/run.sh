#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); cd "$ROOT"
args=(--suite canonical --execs "${BENCH_EXECS:-100}" --sizes "${BENCH_SIZES:-10000,100000}" --cpu "${BENCH_CPU:-0}" --compiler "${BENCH_COMPILER:-gcc}")
[[ -n ${BENCH_RUN_DIR:-} ]] && args+=(--run-dir "$BENCH_RUN_DIR")
exec python3 benchmarks/infrastructure/run_e760.py "${args[@]}"
