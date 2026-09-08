# ipnsort vs maintained JesseSort production/noalloc paths

This benchmark compares Rust **ipnsort** with the three maintained JesseSort public paths that are relevant to production and no-allocation work:

1. **`production-e733`** — `jessesort::sort`, the current public/default E733->E732 simulated-direct live-phase path.
2. **`adaptive-noalloc`** — `jessesort::noalloc::sort_adaptive`, the performance-oriented maintained no-allocation path.
3. **`strict-noalloc`** — `jessesort::sort_unstable_noalloc`, the bounded-stack, no-heap, worst-case `O(n log n)` public contender intended for eventual direct Rust unstable-sort comparison.
4. **`ipnsort`** — Rust ipnsort from `sort-research-rs`.

Historical physical/simulated/frozen/indexed/direct/phase-map representatives are intentionally excluded. They belong in the canonical JesseSort benchmark when mechanism isolation is needed, not in the current Rust comparison kit. E691 is likewise excluded for now: this folder is specifically the production/noalloc Rust comparison surface.

## Inputs and type fidelity

The benchmark uses the 14 canonical JesseSort input families:

Random, Sorted, Reverse, Sorted+Noise(5%), Sorted+Noise(10%), Random%25, Alternating, Sawtooth, MixedDirectionRuns, BlockSorted, OrganPipe, Rotated, MixedPhase3, and MixedPhase12.

The JesseSort generators are implemented directly in `cpp/bridge.cpp` using C++ `std::mt19937` and the canonical seed mixing. They first generate the canonical signed 32-bit topology and then map each value to `u64` with:

```cpp
uint64_t(uint32_t(x) ^ 0x80000000u)
```

This is order-preserving, so equality and signed integer ordering are preserved while both Rust and C++ sort the same `u64` data. The base seed remains `0x8A5CD789`.

## Methodology

- Default size: 10k.
- Default measurements: 500 trials per input, 2 warmups.
- One generated source per trial is shared by all four algorithms.
- Input/output allocation and copying are outside the timed sort region.
- Correctness is validated outside the timed region against Rust stable `sort()`.
- Execution order rotates and reverses across trials to reduce fixed order bias.
- Rust: `-C target-cpu=native`.
- C++: `-O3 -march=native -DNDEBUG`.
- The JesseSort implementation is compiled from the enclosing repository tree, not copied snapshots.

Because all four algorithms run in one benchmark process, use this kit for Rust-vs-JesseSort comparison and publication/reference measurements. JesseSort retain/reject experiments should continue to use the repository's isolated A/B protocol.

## Setup

```bash
chmod +x setup.sh run.sh
./setup.sh
```

`setup.sh` fetches/refreshes `sort-research-rs` and installs the Rust nightly toolchain.

## Run

```bash
./run.sh 10000 500 2
./run.sh 100000 500 2
```

Outputs are written to:

- `results/raw.csv`
- `results/summary.md`
- `results/system.txt`

The summary columns are:

```text
| Input | production-e733 | adaptive-noalloc | strict-noalloc | ipnsort |
```

Each JesseSort cell reports `median/ipnsort (median microseconds)`; ipnsort is the `1.0000` reference. `system.txt` records CPU/compiler/toolchain and source provenance for each run.
