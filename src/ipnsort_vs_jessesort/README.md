# ipnsort vs current JesseSort defaults

This comparison uses the **14 canonical JesseSort benchmark input families** while keeping the JesseSort side synchronized with the eight defaults in `benchmarks/benchmark.cpp`.

The compared sort type remains **u64** so results remain comparable to the prior ipnsort-vs-JesseSort runs.

## JesseSort variations compared

The comparison mirrors the **eight current defaults in `benchmarks/benchmark.cpp`**, in the same order:

1. **`physical`** — literal physical-pile baseline.
2. **`simulated`** — simulated-pile/blueprint baseline.
3. **`frozen-single`** — early-freeze single-overflow representative.
4. **`indexed`** — index-tail architecture.
5. **`noalloc-direct`** — current general allocation-free/direct representative.
6. **`noalloc-low-run`** — specialized allocation-free low-run representative.
7. **`simulated-direct`** — direct-execution simulated descendant.
8. **`simulated-direct-phase-map-mature`** — current mature phase-map default.

The comparison itself sorts `u64`, so move-only capability is an architectural property rather than something exercised by this benchmark. For the no-allocation variants, the bridge's per-trial input/output vectors are created outside the timed sort region.

## Benchmark inputs

1. Random
2. Sorted
3. Reverse
4. Sorted+Noise(5%)
5. Sorted+Noise(10%)
6. Random%25
7. Alternating
8. Sawtooth
9. MixedDirectionRuns
10. BlockSorted
11. OrganPipe
12. Rotated
13. MixedPhase3
14. MixedPhase12

The copied source files used as the authority are in `reference/`.

## Exact benchmark input topology while retaining u64

The JesseSort benchmark generators produce `std::vector<int>` using C++ `std::mt19937` and the standard
library distributions. This kit deliberately generates those values **exactly in
C++ first**.

Each generated signed 32-bit integer is then mapped to u64 as:

```cpp
uint64_t(uint32_t(x) ^ 0x80000000u)
```

This is an order-preserving bijection from signed 32-bit integer order into
unsigned integer order:

- equality is unchanged;
- `a < b` is unchanged;
- `a > b` is unchanged.

Therefore Alternating and MixedDirectionRuns need no special-case approximation,
and the base benchmark input comparison topology is preserved while ipnsort and JesseSort
still benchmark `u64`.

## Seed fidelity

The kit uses the JesseSort benchmark base seed:

```text
0x8A5CD789
```

and the exact `trial_seed(base_seed, n, input_ordinal, trial)` mixing constants.

The important enum ordinals are also preserved, including:

- Sorted+Noise(10%) = 12
- MixedDirectionRuns = 13

so trial seeds match the benchmark generator for the same `(n, input, trial)`.

## Benchmark methodology

- Default: 500 measured trials per input.
- Default size: 10k.
- 2 warmups per input.
- One generated source per paired trial; ipnsort and all eight current JesseSort defaults receive it.
- Input copying is outside the timed sort region.
- Validation is outside the timed region against Rust stable `sort()`, not ipnsort.
- Nine-algorithm execution order (ipnsort + eight JesseSort defaults) rotates and reverses across trials.
- No shared cold-like preconditioner.
- Rust: `-C target-cpu=native`.
- C++: `-O3 -march=native -DNDEBUG`.

## Build

apt-get update
apt-get install -y curl build-essential
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"
rustup --version
cargo --version
rustc --version

## Run

```bash
chmod +x setup.sh run.sh
./setup.sh
./run.sh 10000 500
```

Then 100k:

```bash
./run.sh 100000 500
```

Outputs:

- `results/raw.csv`
- `results/summary.md`
- `results/system.txt`

Summary format matches the main benchmark tables: each JesseSort cell is `median ratio vs ipnsort (median microseconds)`, and the `ipnsort` column is the `1.0000` baseline.

```text
| Input | physical | simulated | frozen-single | indexed | noalloc-direct | noalloc-low-run | simulated-direct | simulated-direct-phase-map-mature | ipnsort |
```

The kit compiles JesseSort from the **enclosing repository tree**, ensuring the tagged implementations being tested are exactly the files in the current checkpoint. `setup.sh` only refreshes the external `sort-research-rs`/ipnsort dependency. `run.sh` records the local JesseSort commit when git metadata is available, otherwise it records that the enclosing repository tree was used.
