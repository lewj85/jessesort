#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
mkdir -p deps results

if [[ ! -d deps/jessesort/.git ]]; then
  git clone https://github.com/lewj85/jessesort.git deps/jessesort
else
  git -C deps/jessesort fetch --all --prune
  git -C deps/jessesort checkout main
  git -C deps/jessesort pull --ff-only
fi

if [[ ! -d deps/sort-research-rs/.git ]]; then
  git clone https://github.com/Voultapher/sort-research-rs.git deps/sort-research-rs
else
  git -C deps/sort-research-rs fetch --all --prune
  git -C deps/sort-research-rs checkout main
  git -C deps/sort-research-rs pull --ff-only
fi

echo "JesseSort commit: $(git -C deps/jessesort rev-parse HEAD)"
echo "sort-research-rs commit: $(git -C deps/sort-research-rs rev-parse HEAD)"

rustup toolchain install nightly --profile minimal
echo
echo "Setup complete. Run: ./run.sh 10000 500"
