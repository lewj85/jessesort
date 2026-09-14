# ipnsort vs maintained JesseSort production/noalloc paths

This benchmark compares Rust **ipnsort** with the three maintained JesseSort public paths that are relevant to production and no-allocation work:

1. **`simulated-direct_live-phase`** — `jessesort::sort`, the current public/default E733->E732 simulated-direct live-phase path.
2. **`adaptive-noalloc`** — `jessesort::noalloc::sort_adaptive`, the performance-oriented maintained no-allocation path.
3. **`strict-noalloc`** — `jessesort::sort_unstable_noalloc`, the bounded-stack, no-heap, worst-case `O(n log n)` public contender intended for eventual direct Rust unstable-sort comparison.
4. **`ipnsort`** — Rust ipnsort from `sort-research-rs`.

Historical physical/simulated/frozen/indexed/direct/phase-map representatives are intentionally excluded. They belong in the canonical JesseSort benchmark when mechanism isolation is needed, not in the current Rust comparison kit. E691 is likewise excluded for now: this folder is specifically the production/noalloc Rust comparison surface.

## Inputs and type fidelity

The benchmark can run the 10 core JesseSort input families, the maintained
50-input additional/OOD suite, or both (60 total).

The core inputs are Random, Sorted, Reverse, Sorted+Noise(5%), Random%25,
Diverging, Sawtooth, MixedDirectionRuns, OrganPipe, and Rotated. Diverging is
the unchanged former Alternating generator under a topology-accurate name.

Sorted+Noise(10%), MixedPhase3, and BlockSorted moved to the additional suite
without changing their workload semantics. `DiversePhaseMix8` replaces
MixedPhase12 with eight guaranteed regimes in seeded order and moderately varied
lengths: ascending, descending, full-random, cardinality-25, 5%-noise sorted,
alternating, mixed-direction runs, and shuffled sorted blocks. The four rows are
appended after the pre-existing additional inputs to preserve their ordinals and
seed schedules.

Canonical and cross-suite shared generators come from
`benchmarks/infrastructure/input_generators.h`; `cpp/bridge.cpp` uses them with
C++ `std::mt19937` and canonical seed mixing. OOD names, definitions, order,
`std::mt19937_64`, and seed schedule come directly from the enclosing repository's
`benchmarks/ood/ood_families.h`, preventing a second copy of the OOD definitions
from drifting. Both suites first generate signed 32-bit topology and then map
each value to `u64` with:

```cpp
uint64_t(uint32_t(x) ^ 0x80000000u)
```

This is order-preserving, so equality and signed integer ordering are preserved while both Rust and C++ sort the same `u64` data. The base seed remains `0x8A5CD789`.

Canonical `Random%25` is fully random uniform cardinality-25 data. The additional
RandomCardinality sweep omits the redundant `RandomCardinalityK25` row while
retaining K5, K10, K50, K100, K256, K1024, K4096, and K16384. `Converging`
replaces the former K65536 row at the same additional-suite position; its low
lane rises while its high lane falls toward the same midpoint.
The additional suite separately includes `RandomCardinalityK2`, `AllEqual`,
`AlternatingDirectionRuns`, and `Reverse+Noise(5%)` in place of the retired
VariableCardinality, JitteredRotation, RunMosaic, and AsymmetricPipePlateau rows.

## Methodology

- Default size: 10k.
- Default cohort: 100 independent five-input executions per input/algorithm, for 500 timed observations.
- Each execution uses five distinct inputs labeled A/B/C/D/E. All five sources and timed working buffers are constructed before A.
- There are no explicit target-sort warmups. A is the fresh-process observation; B is the transition observation; C/D/E are the canonical history-conditioned observations.
- Each algorithm gets its own fresh process for each ABCDE trajectory, so one algorithm cannot warm or perturb another algorithm's allocator/cache history.
- The same five deterministic timed trial IDs/seeds are used for all four algorithms in a given execution. Across 100 executions, all 500 timed trial IDs are used exactly once per input/algorithm, using the same five-stratum rotating assignment as the E760/E761 benchmark.
- Correctness validation happens only after E so validation work cannot condition C/D/E.
- Published per-input results use only the 300 C/D/E observations: `median(ipnsort CDE) / median(JesseSort CDE)`. Values above 1× mean JesseSort is faster; ipnsort is the 1.000× reference.
- Raw CSV output retains all A/B/C/D/E observations for diagnostics.
- Algorithm process launch order rotates/reverses across executions to reduce temporal order bias.
- Rust: `-C target-cpu=native`.
- C++: `-O3 -march=native -DNDEBUG`.
- The JesseSort implementation is compiled from the enclosing repository tree, not copied snapshots.

This comparison harness now follows the same fresh/transition/history interpretation as the E761 JesseSort benchmark. A and B remain available for diagnosis, but are intentionally excluded from the published comparison table.

## Setup

```bash
chmod +x setup.sh run.sh
./setup.sh
```

`setup.sh` fetches/refreshes `sort-research-rs` and installs the Rust nightly toolchain.

## Run

```bash
./run.sh 100000 100 canonical
./run.sh 100000 100 ood
./run.sh 100000 100 all
```

Arguments are `N`, number of fresh ABCDE executions, and suite. The suite is
`canonical`, `ood`, or `all`; defaults are `10000`, `100`, and `canonical`.
With the default 100 executions, each algorithm gets 500 timed observations per
input, of which the 300 C/D/E observations determine the published median.

Outputs are written to:

- `results/raw.csv` — all A/B/C/D/E timings with execution, position, trial ID, seed, and algorithm.
- `results/summary.md` — CDE-median comparison table.
- `results/system.txt` — selected suite, CPU/compiler/toolchain, process policy, and source provenance.

The summary columns are:

```text
| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc | ipnsort |
```

Each JesseSort cell reports `ipnsort CDE median / JesseSort CDE median` plus the
JesseSort CDE median microseconds; ipnsort is the `1.0000×` reference. Therefore
**values above 1× mean JesseSort is faster**. When `all` is selected, the
Markdown output contains separate Canonical and Expanded OOD tables.
