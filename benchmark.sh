#!/usr/bin/env bash
set -euo pipefail

# Canonical JesseSort benchmark runner.
# - exactly 10k and 100k by default (never 1k/1m implicitly)
# - one size/algorithm/input cell per process
# - one uninterrupted 500-trial measurement per cell after warmups
# - a cell becomes canonical only after the full process completes and validates
# - interrupted/partial cells are discarded and rerun from trial 0 on resume
# - deterministic cell order and seed schedule
# - CPU + binary/source/runner fingerprint lock across resumes
# - 10k checkpoints after each algorithm completes all 14 isolated inputs
# - 100k checkpoints after every committed input cell

TOTAL_TRIALS="${BENCH_TOTAL_TRIALS:-500}"
WARMUPS="${BENCH_WARMUPS:-2}"
COOLDOWN_SECONDS="${BENCH_COOLDOWN_SECONDS:-1}"
SUITE="${BENCH_SUITE_DIR:-results/default_10k_100k}"
# REQUIRE_CPU_SUBSTR="${BENCH_REQUIRE_CPU_SUBSTR-8573C}"
REQUIRE_CPU_SUBSTR="${BENCH_REQUIRE_CPU_SUBSTR-i9}"
BENCH_CPU="${BENCH_CPU:-auto}"
METHOD_VERSION="E466-cell-atomic-grouped-checkpoint-v3"

SIZES=(10000 100000)
ROUTINE_ALGORITHMS=(
  physical
  simulated
  frozen-single
  indexed
  noalloc-direct
  noalloc-low-run
  simulated-direct
  simulated-direct-phase-map-mature
  'std::sort'
)
CANONICAL_INPUTS=(
  'Random' 'Sorted' 'Reverse' 'Sorted+Noise(5%)' 'Sorted+Noise(10%)'
  'Random%25' 'Alternating' 'Sawtooth' 'MixedDirectionRuns' 'BlockSorted'
  'OrganPipe' 'Rotated' 'MixedPhase3' 'MixedPhase12'
)

if (( TOTAL_TRIALS < 1 )); then
  echo "BENCH_TOTAL_TRIALS must be positive" >&2
  exit 2
fi

MODEL="$(lscpu | awk -F: '/Model name/ {sub(/^[[:space:]]+/, "", $2); print $2; exit}')"
if [[ -n "$REQUIRE_CPU_SUBSTR" && "$MODEL" != *"$REQUIRE_CPU_SUBSTR"* ]]; then
  echo "REFUSING TO RUN: required CPU substring '$REQUIRE_CPU_SUBSTR'" >&2
  echo "Detected: ${MODEL:-unknown}" >&2
  exit 64
fi

make benchmark

if [[ "$BENCH_CPU" == "auto" ]]; then
  if command -v taskset >/dev/null 2>&1; then
    affinity="$(taskset -pc $$ | sed -E 's/.*: *//')"
    first="${affinity%%,*}"
    BENCH_CPU="${first%%-*}"
  else
    BENCH_CPU=""
  fi
elif [[ "$BENCH_CPU" == "none" ]]; then
  BENCH_CPU=""
fi

mkdir -p "$SUITE/cells" "$SUITE/checkpoints" "$SUITE/in_progress"
META="$SUITE/suite_metadata.txt"
STATE="$SUITE/progress_state.txt"
ORDER="$SUITE/cell_order.txt"
LATEST="$SUITE/checkpoints/latest.zip"

sha() { sha256sum "$1" | awk '{print $1}'; }
EXE_SHA="$(sha ./benchmark)"
CPP_SHA="$(sha benchmarks/benchmark.cpp)"
SEED_SHA="$(sha benchmarks/benchmark_seed.h)"
RUNNER_SHA="$(sha benchmark.sh)"

meta_get() { sed -n "s/^$1=//p" "$META" | head -1; }
if [[ -f "$META" ]]; then
  [[ "$(meta_get cpu_model)" == "$MODEL" ]] || { echo "REFUSING RESUME: CPU model changed" >&2; exit 65; }
  [[ "$(meta_get benchmark_exe_sha256)" == "$EXE_SHA" ]] || { echo "REFUSING RESUME: benchmark binary changed" >&2; exit 66; }
  [[ "$(meta_get benchmark_cpp_sha256)" == "$CPP_SHA" ]] || { echo "REFUSING RESUME: benchmark.cpp changed" >&2; exit 66; }
  [[ "$(meta_get benchmark_seed_sha256)" == "$SEED_SHA" ]] || { echo "REFUSING RESUME: benchmark seed changed" >&2; exit 66; }
  [[ "$(meta_get method_version)" == "$METHOD_VERSION" ]] || { echo "REFUSING RESUME: runner method changed" >&2; exit 66; }
  [[ "$(meta_get runner_sha256)" == "$RUNNER_SHA" ]] || { echo "REFUSING RESUME: benchmark runner changed" >&2; exit 66; }
  [[ "$(meta_get total_trials)" == "$TOTAL_TRIALS" ]] || { echo "REFUSING RESUME: total trial count changed" >&2; exit 66; }
  [[ "$(meta_get warmups)" == "$WARMUPS" ]] || { echo "REFUSING RESUME: warmup count changed" >&2; exit 66; }
else
  cat > "$META" <<META
method_version=$METHOD_VERSION
cpu_model=$MODEL
sizes=10000,100000
total_trials=$TOTAL_TRIALS
warmups=$WARMUPS
cooldown_seconds=$COOLDOWN_SECONDS
process_policy=one uninterrupted size/algorithm/input cell per process
commit_policy=cell is canonical only after all trials complete and validate; partial cell is discarded
checkpoint_policy=10k: after all 14 inputs for an algorithm; 100k: after each committed input cell
resume_policy=completed cells are skipped; interrupted cell reruns from trial 0; an uncheckpointed completed 10k cell survives only in the live workspace until its 14-input algorithm group checkpoint
benchmark_exe_sha256=$EXE_SHA
benchmark_cpp_sha256=$CPP_SHA
benchmark_seed_sha256=$SEED_SHA
runner_sha256=$RUNNER_SHA
META
fi

sanitize() { printf '%s' "$1" | sed -E 's/[^A-Za-z0-9._-]+/_/g'; }

package_progress() {
  local suite_abs latest_abs tmp
  suite_abs="$(cd "$(dirname "$SUITE")" && pwd)/$(basename "$SUITE")"
  latest_abs="$suite_abs/checkpoints/latest.zip"
  tmp="$suite_abs/checkpoints/.latest.$$.zip"
  rm -f "$tmp"
  (cd "$(dirname "$suite_abs")" && zip -qr "$tmp" "$(basename "$suite_abs")" \
      -x '*/checkpoints/*.zip' '*/in_progress/*') || return 0
  mv -f "$tmp" "$latest_abs"
}

write_state() {
  local status="$1" completed="$2" current="${3:-}"
  cat > "$STATE" <<STATE
state=$status
completed_cells=$completed
total_cells=${#RUN_CELLS[@]}
trials_per_cell=$TOTAL_TRIALS
current_cell=$current
STATE
}

cleanup_partial() {
  rm -rf "$SUITE/in_progress"
  mkdir -p "$SUITE/in_progress"
}

FINALIZED=0
SAFE_TO_PACKAGE=1
on_exit() {
  local rc=$?
  if (( FINALIZED == 0 )); then
    cleanup_partial
    if declare -p RUN_CELLS >/dev/null 2>&1 && [[ -f "$STATE" ]]; then
      write_state ready "${completed:-0}"
    fi
    if (( SAFE_TO_PACKAGE == 1 )); then
      package_progress
    else
      echo "Leaving last packaged checkpoint unchanged: active 10k algorithm group is incomplete." >&2
    fi
  fi
  return "$rc"
}
on_signal() {
  local sig="$1"
  echo "Interrupted by $sig; discarding active partial cell." >&2
  cleanup_partial
  if (( SAFE_TO_PACKAGE == 1 )); then
    package_progress
  else
    echo "Leaving last packaged checkpoint unchanged: active 10k algorithm group is incomplete." >&2
  fi
  FINALIZED=1
  exit 130
}
trap on_exit EXIT
trap 'on_signal INT' INT
trap 'on_signal TERM' TERM

# A stale in_progress directory can only contain an uncommitted cell from a
# terminated process. It must never be resumed or mixed with a new process.
cleanup_partial

if [[ ! -f "$ORDER" ]]; then
  # 10k is grouped by algorithm so checkpoint ZIP creation occurs only after
  # all 14 isolated input processes for that algorithm have committed.  The
  # inputs themselves remain separate processes. 100k keeps randomized
  # per-cell ordering and checkpoints after every committed cell.
  tenk=()
  for algorithm in "${ROUTINE_ALGORITHMS[@]}"; do
    for input in "${CANONICAL_INPUTS[@]}"; do
      tenk+=("10000|$algorithm|$input")
    done
  done
  hundredk=()
  for algorithm in "${ROUTINE_ALGORITHMS[@]}"; do
    for input in "${CANONICAL_INPUTS[@]}"; do
      hundredk+=("100000|$algorithm|$input")
    done
  done
  mapfile -t shuffled_100k < <(python3 - "${hundredk[@]}" <<'PY'
import random, sys
items = sys.argv[1:]
random.Random('E466-default-100k-order-v3').shuffle(items)
print(*items, sep='\n')
PY
  )
  printf '%s\n' "${tenk[@]}" "${shuffled_100k[@]}" > "$ORDER"
fi
mapfile -t RUN_CELLS < "$ORDER"

expected_cells=$(( ${#SIZES[@]} * ${#ROUTINE_ALGORITHMS[@]} * ${#CANONICAL_INPUTS[@]} ))
if (( ${#RUN_CELLS[@]} != expected_cells )); then
  echo "REFUSING RESUME: cell order has ${#RUN_CELLS[@]} entries, expected $expected_cells" >&2
  exit 67
fi

validate_committed_cell() {
  local d="$1" expected_algorithm="$2" expected_input="$3" expected_n="$4"
  python3 - "$d" "$expected_algorithm" "$expected_input" "$expected_n" "$TOTAL_TRIALS" <<'PY'
import csv, sys
from pathlib import Path

d = Path(sys.argv[1]); alg=sys.argv[2]; inp=sys.argv[3]; n=int(sys.argv[4]); expected=int(sys.argv[5])
required = [d/'benchmark_trials.csv', d/'benchmark_results.csv', d/'benchmark_metadata.txt', d/'benchmark_progress.txt']
if not all(p.exists() for p in required):
    raise SystemExit(2)
if 'state=complete' not in (d/'benchmark_progress.txt').read_text():
    raise SystemExit(3)
rows=list(csv.DictReader((d/'benchmark_trials.csv').open()))
if len(rows) != expected:
    raise SystemExit(4)
if any(r['algorithm'] != alg or r['input'] != inp or int(r['n']) != n for r in rows):
    raise SystemExit(5)
trials=[int(r['trial']) for r in rows]
if len(set(trials)) != expected or sorted(trials) != list(range(expected)):
    raise SystemExit(6)
summary=[r for r in csv.DictReader((d/'benchmark_results.csv').open()) if r['algorithm']==alg and r['input']==inp and int(r['n'])==n]
if len(summary) != 1 or int(summary[0]['trials']) != expected:
    raise SystemExit(7)
PY
}

# Validate all already-committed directories before trusting them as resume points.
completed=0
for cell in "${RUN_CELLS[@]}"; do
  IFS='|' read -r n algorithm input <<< "$cell"
  final_dir="$SUITE/cells/${n}__$(sanitize "$algorithm")__$(sanitize "$input")"
  if [[ -d "$final_dir" ]]; then
    if ! validate_committed_cell "$final_dir" "$algorithm" "$input" "$n"; then
      echo "REFUSING RESUME: committed cell is incomplete/corrupt: $final_dir" >&2
      exit 68
    fi
    completed=$((completed+1))
  fi
done

if (( completed == expected_cells )); then
  echo "Canonical benchmark already complete: $completed/$expected_cells cells."
  write_state complete "$completed"
  exit 0
fi

# A live workspace may contain committed 10k cells from an algorithm group that
# had not yet reached its 14-input checkpoint before a session ended. Preserve
# those cells for fast local resume, but do not overwrite the last safe ZIP
# until the group finishes.
for resume_algorithm in "${ROUTINE_ALGORITHMS[@]}"; do
  resume_count=0
  for resume_input in "${CANONICAL_INPUTS[@]}"; do
    resume_dir="$SUITE/cells/10000__$(sanitize "$resume_algorithm")__$(sanitize "$resume_input")"
    [[ -d "$resume_dir" ]] && resume_count=$((resume_count+1))
  done
  if (( resume_count > 0 && resume_count < ${#CANONICAL_INPUTS[@]} )); then
    SAFE_TO_PACKAGE=0
    echo "Resuming incomplete 10k algorithm group: $resume_algorithm ($resume_count/${#CANONICAL_INPUTS[@]} committed locally)"
    break
  fi
done

write_state ready "$completed"
if (( SAFE_TO_PACKAGE == 1 )); then
  package_progress
fi
echo "CPU: $MODEL"
echo "Protocol: $TOTAL_TRIALS uninterrupted trials per cell after $WARMUPS warmups"
echo "Progress: $completed/$expected_cells committed cells"
echo "Cells: $expected_cells = 2 sizes x 9 algorithms x 14 inputs"

affinity_prefix=()
if [[ -n "$BENCH_CPU" ]]; then
  command -v taskset >/dev/null 2>&1 || { echo "BENCH_CPU requested but taskset unavailable" >&2; exit 5; }
  affinity_prefix=(taskset -c "$BENCH_CPU")
fi

for cell in "${RUN_CELLS[@]}"; do
  IFS='|' read -r n algorithm input <<< "$cell"
  cell_name="${n}__$(sanitize "$algorithm")__$(sanitize "$input")"
  final_dir="$SUITE/cells/$cell_name"
  [[ -d "$final_dir" ]] && continue

  work_dir="$SUITE/in_progress/$cell_name"
  rm -rf "$work_dir"
  write_state running "$completed" "$cell"
  echo "==> [$((completed+1))/$expected_cells] n=$n $algorithm / $input : fresh $TOTAL_TRIALS-trial cell"

  # benchmark itself flushes raw rows as it runs, but they stay quarantined in
  # in_progress. A killed process never contributes those rows to canonical data.
  "${affinity_prefix[@]}" ./benchmark "$TOTAL_TRIALS" "$n" "$WARMUPS" "$work_dir" "$algorithm" "$input"

  if ! validate_committed_cell "$work_dir" "$algorithm" "$input" "$n"; then
    echo "CELL VALIDATION FAILED; discarding uncommitted cell: $cell" >&2
    rm -rf "$work_dir"
    write_state ready "$completed" "$cell"
    if (( SAFE_TO_PACKAGE == 1 )); then
      package_progress
    fi
    exit 69
  fi

  mv "$work_dir" "$final_dir"
  completed=$((completed+1))
  write_state committed "$completed"

  if [[ "$n" == "100000" ]]; then
    package_progress
    echo "Committed 100k cell $completed/$expected_cells; checkpoint: $LATEST"
  else
    # 10k: checkpoint only after the 14th input for this algorithm.  This
    # keeps every input isolated while avoiding 14 ZIP rewrites per algorithm.
    tenk_committed_for_algorithm=0
    for group_input in "${CANONICAL_INPUTS[@]}"; do
      group_dir="$SUITE/cells/10000__$(sanitize "$algorithm")__$(sanitize "$group_input")"
      [[ -d "$group_dir" ]] && tenk_committed_for_algorithm=$((tenk_committed_for_algorithm+1))
    done
    if (( tenk_committed_for_algorithm == ${#CANONICAL_INPUTS[@]} )); then
      SAFE_TO_PACKAGE=1
      package_progress
      echo "Committed 10k algorithm group: $algorithm (${#CANONICAL_INPUTS[@]} isolated inputs); checkpoint: $LATEST"
    else
      SAFE_TO_PACKAGE=0
      echo "Committed 10k cell $tenk_committed_for_algorithm/${#CANONICAL_INPUTS[@]} for $algorithm; checkpoint deferred"
    fi
  fi

  [[ "$COOLDOWN_SECONDS" == "0" ]] || sleep "$COOLDOWN_SECONDS"
done

cleanup_partial
python3 benchmarks/validate_benchmark_suite.py "$SUITE"
aggregate_args=()
for n in "${SIZES[@]}"; do
  for algorithm in "${ROUTINE_ALGORITHMS[@]}"; do
    for input in "${CANONICAL_INPUTS[@]}"; do
      aggregate_args+=("$algorithm=$SUITE/cells/${n}__$(sanitize "$algorithm")__$(sanitize "$input")/benchmark_results.csv")
    done
  done
done
python3 benchmarks/aggregate_benchmarks.py "$SUITE" "${aggregate_args[@]}"
write_state complete "$completed"
package_progress
FINALIZED=1
cat "$SUITE/benchmark_results.md"
echo "Completed canonical benchmark: $completed/$expected_cells cells, $TOTAL_TRIALS uninterrupted trials/cell"
echo "Final cumulative checkpoint: $LATEST"
