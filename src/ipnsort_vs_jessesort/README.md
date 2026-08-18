# ipnsort vs layer-tagged JesseSort — E229 update

This retains the established ipnsort comparison kit's **12 E189 canonical input families** and updates only the JesseSort side to the E229 layer-tagged variation bank.

The compared sort type remains **u64** so results remain comparable to the prior ipnsort-vs-JesseSort runs.


## JesseSort variations compared

The E229 comparison intentionally uses a compact architecture-spanning set:

1. **`physical`** — preserved pre-E225 physical-pile baseline.
2. **`simulated`** — preserved pre-E225 simulated-blueprint champion/control.
3. **`simulated-direct`** — current high-performance allocating descendant with the E225 direct natural-run merge router; this ensures the fastest JesseSort family tested so far is represented.
4. **`indexed`** — index-tail architecture, included because its non-copyable path supports move-only values.
5. **`noalloc`** — allocation-free bounded/fallback architecture.
6. **`noalloc-low-run`** — allocation-free descendant with selective low-run in-place merging and bounded run reclamation.

The comparison itself still sorts `u64`, so the move-only capability of `indexed` is an architectural property rather than something exercised by this particular benchmark. Likewise, `noalloc` means the JesseSort call performs no heap allocation internally; the bridge's per-trial input/output vectors are created outside the timed sort region, as in the existing kit. None of these variations is literally "no-move"—sorting necessarily moves or swaps values.

## Inputs, in E189 canonical order

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

The copied source files used as the authority are in `reference/`.

## Exact E189 input topology while retaining u64

E189 generates `std::vector<int>` using C++ `std::mt19937` and the standard
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
and all E189 input comparison topology is preserved while ipnsort and JesseSort
still benchmark `u64`.

## Seed fidelity

The kit uses E189's canonical base seed:

```text
0x8A5CD789
```

and the exact `trial_seed(base_seed, n, input_ordinal, trial)` mixing constants.

The important enum ordinals are also preserved, including:

- Sorted+Noise(10%) = 12
- MixedDirectionRuns = 13

so trial seeds match E189 for the same `(n, input, trial)`.

## Benchmark methodology

- Default: 500 measured trials per input.
- Default size: 10k.
- 2 warmups per input.
- One generated source per paired trial; ipnsort and all six selected JesseSort variations receive it.
- Input copying is outside the timed sort region.
- Validation is outside the timed region against Rust stable `sort()`, not ipnsort.
- Seven-algorithm execution order (ipnsort + six JesseSort variations) rotates and reverses across trials.
- No shared cold-like preconditioner.
- Rust: `-C target-cpu=native`.
- C++: `-O3 -march=native -DNDEBUG`.

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

Summary format:

```text
| Pattern | ipnsort µs | physical µs | simulated µs | simulated-direct µs | indexed µs | noalloc µs | noalloc-low-run µs | simulated/ipnsort | simulated-direct/ipnsort | best Jesse/ipnsort |
```

The kit now compiles JesseSort from the **enclosing E229 repository tree**, ensuring the layer-tagged files being tested are exactly the files in this checkpoint. `setup.sh` only refreshes the external `sort-research-rs`/ipnsort dependency. `run.sh` records the local JesseSort commit when git metadata is available, otherwise it records that the enclosing E229 tree was used.
