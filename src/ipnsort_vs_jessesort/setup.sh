#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JESSESORT_ROOT="$(cd "$ROOT/../.." && pwd)"
cd "$ROOT"
mkdir -p deps results

if [[ ! -f "$JESSESORT_ROOT/include/jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h" ]]; then
  echo "Expected current tagged JesseSort headers were not found in: $JESSESORT_ROOT" >&2
  exit 1
fi

if [[ ! -d deps/sort-research-rs/.git ]]; then
  git clone https://github.com/Voultapher/sort-research-rs.git deps/sort-research-rs
else
  git -C deps/sort-research-rs fetch --all --prune
  git -C deps/sort-research-rs checkout main
  git -C deps/sort-research-rs pull --ff-only
fi

if git -C "$JESSESORT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "JesseSort commit: $(git -C "$JESSESORT_ROOT" rev-parse HEAD)"
else
  echo "JesseSort source: local enclosing repository tree (no git metadata)"
fi
echo "sort-research-rs commit: $(git -C deps/sort-research-rs rev-parse HEAD)"

rustup toolchain install nightly --profile minimal
echo
echo "Setup complete. Run: ./run.sh 10000 500"
