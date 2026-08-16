#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

N="${1:-10000}"
TRIALS="${2:-500}"
WARMUPS="${3:-2}"

if [[ ! -d deps/jessesort/.git || ! -d deps/sort-research-rs/.git ]]; then
  echo "Dependencies missing. Run ./setup.sh first." >&2
  exit 1
fi

mkdir -p results

{
  echo "generated_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "n=$N"
  echo "trials=$TRIALS"
  echo "warmups=$WARMUPS"
  echo "jessesort_commit=$(git -C deps/jessesort rev-parse HEAD)"
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
