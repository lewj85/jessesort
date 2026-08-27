#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JESSESORT_ROOT="$(cd "$ROOT/../.." && pwd)"
cd "$ROOT"

N="${1:-10000}"
TRIALS="${2:-500}"
WARMUPS="${3:-2}"

if [[ ! -d deps/sort-research-rs/.git ]]; then
  echo "ipnsort dependency missing. Run ./setup.sh first." >&2
  exit 1
fi
if [[ ! -f "$JESSESORT_ROOT/include/jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h" ]]; then
  echo "Expected current tagged JesseSort headers missing from enclosing repository." >&2
  exit 1
fi

mkdir -p results

if git -C "$JESSESORT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  JESSESORT_SOURCE="$(git -C "$JESSESORT_ROOT" rev-parse HEAD)"
else
  JESSESORT_SOURCE="local-jessesort-tree-no-git-metadata"
fi

{
  echo "generated_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "n=$N"
  echo "trials=$TRIALS"
  echo "warmups=$WARMUPS"
  echo "jessesort_source=$JESSESORT_SOURCE"
  echo "sort_research_rs_commit=$(git -C deps/sort-research-rs rev-parse HEAD)"
  echo "rustc=$(rustc +nightly --version)"
  echo "g++=$(g++ --version | head -n1)"
  echo "uname=$(uname -a)"
  if [[ -r /proc/cpuinfo ]]; then
    echo "cpu=$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')"
  fi
} > results/system.txt

export RUSTFLAGS="${RUSTFLAGS:-} -C target-cpu=native"
cargo +nightly run --release -- "$N" "$TRIALS" "$WARMUPS"

echo
cat results/summary.md
