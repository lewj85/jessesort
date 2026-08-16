# ipnsort vs current JesseSort HEAD — E189 canonical-input kit

This replaces the earlier comparison kit's ipnsort-specific input families with
the **12 canonical input families from the supplied E189 JesseSort benchmark**.

The compared sort type remains **u64** so results remain comparable to the prior
ipnsort-vs-JesseSort runs.

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
- One generated source per paired trial; ipnsort, V1, V2, and V5 all receive it.
- Input copying is outside the timed sort region.
- Validation is outside the timed region against Rust stable `sort()`, not ipnsort.
- Four-algorithm execution order rotates and reverses across trials.
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
| Pattern | ipnsort µs | V1 µs | V2 µs | V5 µs | V2/ipnsort | best Jesse/ipnsort |
```

`setup.sh` refreshes both repositories and `run.sh` records their exact commit
hashes in `results/system.txt`.
