#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); cd "$ROOT"
args=(--suite ood --execs "${OOD_EXECS:-100}" --n "${OOD_N:-100000}" --cpu "${BENCH_CPU:-0}" --compiler "${BENCH_COMPILER:-gcc}")
[[ -n ${OOD_RUN_DIR:-} ]] && args+=(--run-dir "$OOD_RUN_DIR")
exec python3 benchmarks/infrastructure/run_e760.py "${args[@]}"
