# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

## Description

**tl;dr Jessesort plays two games of Solitaire, one game with descending piles and one with ascending piles, then merges the piles together.**

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience**. Patience is a card game similar to Solitaire, where the cards don't have to be sequential, but the tails of each pile have to be kept in sorted order. This additional restriction allows new values to quickly find their correct pile via binary search over just these sorted pile tails.

Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs, which generate n 1-element piles--by routing inputs to two simultaneous games based on the current run order. One game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles `k` typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push `k` lower towards `O(n)` linear work. Maintained variations are deterministic and unstable.

```
Best    Average     Worst       Memory‡                   Stable    Deterministic
n       n log k     n log n     n + k (allocating)        No        Yes
                                log n (non-allocating)
```

‡ Regarding memory: Several `noalloc` implementations are maintained in the repo that are allocation-free/move-only. They use fixed bounded metadata (≤8 KiB), no heap allocation inside the sort, direct structural routes for several low-entropy geometries, and allocation-free fallback behavior. Simulated variations do allocate though, with `n` 32-bit auxiliary storage plus base array copies of size `k`.

## Speed tests

**Jessesort is up to 45x faster than `std::sort` with GCC/libstdc++.** Below are speed comparisons. In these tables, a ratio of 5.0 means Jessesort is 5x faster. **Ratios >1 mean Jessesort is faster.**

### std::sort with GCC (libstdc++)

Cells are **median paired ratio vs `std::sort`** from 500 trials on n=100k elements using an Intel Xeon Platinum 8573C with GCC 14.2.0 (libstdc++) in C++20 with `-O3 -march=native -DNDEBUG`.

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | **3.88×** | 3.83× | 3.75× |
| Sorted | 13.78× | 27.37× | **27.37×** |
| Reverse | 10.92× | 15.62× | **15.90×** |
| Sorted+Noise(5%) | **2.23×** | 1.47× | 1.44× |
| Sorted+Noise(10%) | 1.66× | 1.41× | **1.82×** |
| Random%25 | 5.40× | 6.04× | **6.11×** |
| Alternating | **2.85×** | 1.37× | 1.36× |
| Sawtooth | **4.01×** | 3.64× | 1.25× |
| MixedDirectionRuns | 9.72× | **16.64×** | 10.28× |
| BlockSorted | 13.16× | **19.84×** | 19.34× |
| OrganPipe | 28.79× | **45.76×** | 41.24× |
| Rotated | 26.53× | 43.40× | **44.80×** |
| MixedPhase3 | **3.09×** | 3.02× | 2.63× |
| MixedPhase12 | **2.80×** | 2.48× | 1.39× |
| RunMosaic | **7.49×** | 6.23× | 2.35× |
| UnevenRunMosaic | 9.81× | **14.49×** | 13.73× |
| MonotoneBurstNoise | 3.35× | 4.24× | **4.31×** |
| SparseInversionPatches | 6.76× | 21.92× | **23.58×** |
| WindowShuffle | 1.42× | 1.59× | **1.66×** |
| PlateauStaircase | **2.51×** | 1.26× | 1.25× |
| DuplicateRunMosaic | **3.86×** | 2.43× | 2.39× |
| VariableCardinality | 4.87× | 4.94× | **5.28×** |
| ClusteredDuplicates | 5.32× | 5.69× | **5.98×** |
| InterleavedLanes | 3.12× | 1.83× | **3.15×** |
| AlternatingWithJitter | **1.77×** | 0.87× | 1.75× |
| WarpedBitonic | **40.89×** | 26.34× | 14.91× |
| AsymmetricPipePlateau | 8.90× | **11.03×** | 10.85× |
| MultiTurnAffine | 8.56× | **11.31×** | 5.63× |
| OffsetRotationRamp | 11.70× | 37.57× | **38.09×** |
| JitteredRotation | 41.87× | 42.12× | **42.17×** |
| DiscontinuousAffinePhases | 16.06× | 33.13× | **34.55×** |
| NoisyAffinePhases | 1.74× | **2.00×** | 1.91× |
| OverlappingSortedBlocks | **7.84×** | 2.06× | 1.63× |
| RandomWalk | 1.36× | **1.84×** | 1.83× |
| StickyRandomWalk | 0.82× | **1.78×** | 1.35× |
| PeriodicPerturbed | 1.38× | **3.17×** | 3.16× |
| ChunkEntropyMixture | **3.04×** | 3.00× | 1.99× |
| LocalizedAlternatingBursts | 4.35× | 6.04× | **6.29×** |
| RandomCardinalityK5 | 2.20× | 5.85× | **5.85×** |
| RandomCardinalityK10 | 5.71× | **6.69×** | 6.63× |
| RandomCardinalityK25 | 5.62× | **6.43×** | 6.41× |
| RandomCardinalityK50 | 5.55× | **6.39×** | 6.28× |
| RandomCardinalityK100 | 6.33× | **7.04×** | 6.92× |
| RandomCardinalityK256 | 5.39× | **5.97×** | 5.93× |
| RandomCardinalityK1024 | 5.12× | **5.52×** | 5.47× |
| RandomCardinalityK4096 | 4.93× | **5.09×** | 5.09× |
| RandomCardinalityK16384 | 2.92× | 3.92× | **3.94×** |
| RandomCardinalityK65536 | 4.91× | 4.91× | **5.01×** |
| BlockShuffle16 | 0.83× | 0.63× | **4.70×** |
| BlockShuffle32 | 1.39× | **4.77×** | 4.57× |
| BlockShuffle64 | 1.12× | **3.55×** | 3.18× |
| BlockShuffle128 | 1.18× | **3.35×** | 3.32× |
| BlockShuffle256 | 1.19× | 3.13× | **3.16×** |
| BlockShuffle512 | 1.27× | 2.82× | **2.90×** |
| BlockShuffle1024 | 1.42× | 2.72× | **2.74×** |
| BlockShuffle4096 | 1.34× | **1.50×** | 0.44× |
| BlockShuffle16384 | **2.00×** | 0.62× | 0.56× |
| SortedSwap1 | **0.71×** | 0.44× | 0.37× |
| SortedSwap2 | **1.21×** | 0.66× | 0.72× |
| SortedSwap5 | 1.03× | 0.84× | **1.17×** |
| SortedSwap10 | **1.43×** | 1.41× | 1.14× |
| SortedSwap20 | 1.68× | **1.99×** | 1.60× |
| SortedSwap30 | 2.72× | 2.71× | **2.79×** |

Among these three variations, simulated-direct_live-phase is fastest 17×, adaptive-noalloc 22×, and strict-noalloc 24×.

### std::sort with Clang (libc++)

**Jessesort is up to 8x faster than `std::sort` with Clang/libc++.** Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless behavior). We show one table of clang timings below for reference, using an AMD Ryzen 7 7445HS CPU.

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 3.36× | **3.45×** | 3.43× |
| Sorted | 1.84× | **1.85×** | 1.79× |
| Reverse | **1.54×** | 1.35× | 1.53× |
| Sorted+Noise(5%) | **1.71×** | 0.63× | 1.07× |
| Sorted+Noise(10%) | **1.39×** | 0.65× | 1.32× |
| Random%25 | 3.69× | 3.77× | **3.78×** |
| Alternating | **1.02×** | 0.87× | 0.87× |
| Sawtooth | **0.97×** | 0.76× | 0.30× |
| MixedDirectionRuns | **1.06×** | 0.84× | 0.75× |
| BlockSorted | **2.09×** | 0.80× | 0.80× |
| OrganPipe | 5.98× | **7.96×** | 7.53× |
| Rotated | **4.15×** | 3.68× | 3.71× |
| MixedPhase3 | **1.66×** | 1.44× | 1.22× |
| MixedPhase12 | **1.12×** | 0.90× | 0.53× |

### ipnsort

**Jessesort is up to 20x faster than ipnsort in Rust.** Results below are direct timing comparisons with an AMD Ryzen 7 7445HS CPU. Both algorithms ran in their native language: ipnsort was run in Rust, Jessesort was run in C++. See the `src/ipnsort_vs_jessesort/` folder for more details.

- type: u64
- n: 100000
- trials per pattern: 500
- warmups per pattern: 2
- shared cold-like preconditioner: false
- inputs: 14 canonical JesseSort benchmark inputs, order-preserving int->u64 encoding

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 0.94× | 0.96× | **0.96×** |
| Sorted | 0.51× | **0.51×** | 0.51× |
| Reverse | 0.93× | **1.56×** | 1.56× |
| Sorted+Noise(5%) | 0.84× | 0.86× | **0.94×** |
| Sorted+Noise(10%) | 0.65× | 0.72× | **0.98×** |
| Random%25 | 0.87× | **0.90×** | 0.90× |
| Alternating | 0.67× | **0.93×** | 0.93× |
| Sawtooth | **1.01×** | 0.90× | 0.40× |
| MixedDirectionRuns | 1.45× | **3.78×** | 2.96× |
| BlockSorted | 1.50× | **3.97×** | 3.97× |
| OrganPipe | 1.98× | 8.47× | **8.47×** |
| Rotated | **20.24×** | 13.02× | 12.99× |
| MixedPhase3 | 0.73× | **1.07×** | 0.91× |
| MixedPhase12 | 0.79× | **0.93×** | 0.55× |

## Algorithm overview

Jessesort has two broad phases, with several retained adaptive paths layered around them.

### 1. Dual-Patience insertion/deconstruction

Values are routed between an ascending and a descending Patience game based on current run direction. The simulated variations record compact pile/run tags rather than physically constructing every pile.

The maintained implementations use monotone early exits, early structure probes to discover input shape, optimal deconstruction routing, specialized hinted/no-hint search continuations, and gated natural-run batching. Probes are always done using dual Patience to capture underlying structure in both directions. Some variations only probe at the beginning, others re-evaluate after some number of processed elements.

High-entropy inputs are currently routed to ipnsort-inspired partitioning. Similarly, some variations can also switch straight to merging if there is high-monotonicity. Some variations use a 50% nominal freeze decision point, routing values that don't fit on existing piles to an unsorted overflow pile. Depending on balanced merge tree extension policy, actual pile creation can continue beyond 50% before the structure is frozen. Later values are handled through each variation's overflow design.

### 2. Reconstruction and adaptive merging

The merge phase selects a merge strategy based on the shape of the piles created by the insertion phase. Some variations include:

- ordered and reverse-disjoint boundary fast paths
- bidirectional branchless two-run merging for high-entropy cases, exposing independent front/back dependency chains
- a merge-only fast path for trivially copyable values through 96 bytes
- general/galloping fallback paths where branchless merging is not preferred
- pile-density and run-structure routing
- adjacent-pair merge scheduling
- variation-specific merge/overflow policies, including deferred-band handling and live-sorted bands

The exact routing differs by variation because their insertion and run geometries differ. `docs/experiment_log.txt` is the authoritative record of retained/rejected policies and propagation tests.

## Variations

Many variations of Jessesort are maintained in this repo because the project is still an active algorithm-design testbed. Source files use descriptive tags:

`jessesort_<decomposition>_<router>_<reconstruction-and-merge>[_<other-policy>...]`

The names are intended to identify the architectural choices preserved by each implementation. Some variants are routine benchmark representatives; others remain under `research/experimental/` so specific mechanisms can still be measured independently even when they are not currently the fastest overall.

The main maintained families include:

- physical - literal ascending/descending Patience piles; baseline architecture.
- simulated - simulated Patience decomposition using compact pile-tail state and per-element blueprint metadata.
- frozen-single - simulated decomposition with early freezing into a single deferred overflow region.
- in-place - simulated variants that realize the reconstructed pile/run layout inside the source storage rather than through the ordinary full auxiliary flattening path.
- linked - reconstruction represented with element/span links, including fused-first-merge descendants.
- indexed - pile-tail state stored as source indices rather than copies of T, supporting move-only/non-copyable paths.
- noalloc - bounded fixed-metadata Patience variants designed to perform no heap allocation inside the sort.
- noalloc-low-run - allocation-free low-run specialization with shared fixed tail storage and selective in-place merging.
- noalloc-direct - current direct allocation-free branch, combining bounded Patience metadata, direct structured routes, live run reclamation, overlap-aware in-place merging, and allocation-free fallback behavior.
- simulated-direct - simulated Patience front end with direct execution paths that can bypass ordinary full reconstruction when the discovered structure permits it.
- probe-first - variants that attempt to reuse the initial structural probe as useful sorting work rather than discarding it after classification.
- phase-map - meta-routing variants that use sparse Patience observations to divide one input into structural regions and execute different paths across those regions.
- phase-map-mature - production-oriented phase-map descendants using the current mature routing and execution logic.
- capped-probe / SIMD - bounded probe variants designed around small fixed tail sets, including AVX2-oriented searches.
- natural-run routed - variants with a pre-router that recognizes small numbers of long monotone runs and can bypass ordinary Patience decomposition.
- live-phase - route each region live by classifying input into three categories: monotone for direct processing, Patience-unfriendly for quicksort fallback, or Patience-friendly for default dual Patience insertion.

Several of these families have direct, linked, in-place, phase-map, move-only, or no-allocation descendants. Those suffixes identify combinations of the same architectural dimensions rather than entirely separate algorithms.

The section below describes the underlying mechanisms themselves. The variation names above are primarily a map from those mechanisms to the concrete implementations preserved in the repository.

### Distinctive mechanisms explored

Jessesort explores a collection of distinctive sorting mechanisms, including several that I haven't found in prior sorting literature. Some seem genuinely novel, while others are established ideas adapted to Patience-based sorting.

- **Dual-direction Patience decomposition.** Run ascending and descending Patience games simultaneously and route each new value toward the game matching its local run direction, avoiding Patience Sort's pathological treatment of natural runs.
- **Base arrays.** Mirror scattered physical pile tails into a compact contiguous search structure, optionally in Eytzinger order, avoiding pointer/vector traversal during pile selection. This differs from LIS implementations that retain only the tails array; here the array acts as a search mirror for still-materialized piles.
- **Persistent pile-search position.** Start each insertion search near the previous pile rather than restarting from pile zero, so sustained natural runs can stay close to linear-time insertion behavior. Here the locality idea is applied independently to the two Patience games, with a separate search-position tracker for each.
- **Blueprints / simulated piles.** Record pile assignments without physically constructing the corresponding piles, leaving a compact latent description of the decomposition.
- **Packed and bulk-decoded blueprints.** Store compact pile/game tags and reconstruct long same-destination spans in bulk rather than decoding one element at a time.
- **Linked blueprints, simulated merging, and fused reconstruction.** Simulate merge levels over latent pile links, then materialize composite runs directly instead of flattening every pile before merging. Deferred materialization is established, but its use for Patience-pile reconstruction appears distinct.
- **Early freezing.** Stop maintaining an exact Patience decomposition once enough structure has been learned, then handle the remaining suffix with a cheaper representation.
- **Unsorted overflow after freezing.** Deliberately allow post-freeze values to accumulate in one unsorted overflow run or deferred fixed-size unsorted bands, sorting them only when reconstruction makes it worthwhile.
- **Adaptive overflow representation.** After freezing, choose among unsorted bands, ascending natural runs, or bidirectional natural runs from the observed suffix behavior rather than committing to one overflow format.
- **Using overflow for merge-tree balancing.** Pop elements off the overflow pile as needed to help balance the merge-tree. Balance the size of local A/B pairs or plan ahead like Powersort to keep run lengths 2^n-optimal.
- **Deferred/simulated flattening.** Delay converting that latent decomposition into contiguous merge runs until reconstruction is actually required.
- **In-place flattening.** When that reconstruction is required, realize the pile/run layout inside the source storage rather than through a full \(O(n)\) destination buffer.
- **Bounded Patience probes with spill.** Build only a small capped set of probe piles and temporarily spill values that exceed the cap, using the partial decomposition as a cheap structural observation instead of a full sort commitment.
- **Patience decomposition as a sensor.** Use tiny dual-Patience sorts to measure local structure--pile counts, game balance, route state, concentration--even when their main purpose is classification rather than sorting that window.
- **Galloping spatial probes.** When successive Patience observations report a stable phase, exponentially increase the distance to the next probe; when a phase changes, reset to dense sampling and confirm the transition. This applies exponential/galloping spacing to Patience-based structural sampling.
- **Phase maps.** Convert sparse Patience observations into spatial regions and allow different parts of one array to take different execution paths instead of forcing a single global sorting strategy.
- **Economics-gated phase execution.** A structurally valid phase map is still rejected when its predicted execution cost is unattractive; observation and commitment are intentionally separate decisions.
- **Probe-first execution.** Make the initial structural probe useful sorting work whenever possible, so routing/classification does not necessarily become discarded overhead.
- **Fixed-cap no-allocation Patience.** Keep pile tails and run descriptors in bounded local storage, with allocation-free fallback behavior when the input exceeds the geometry the fixed metadata can represent.
- **Live descriptor reclamation.** When a bounded run-descriptor stack fills, merge selected completed adjacent runs during decomposition to free metadata slots and continue without heap allocation.
- **Blueprint quicksort.** Simulate multiple recursive binary partition levels before moving any elements, recording each element's full L/R route as compact bits; only after those decisions are complete is the route realized into destination buckets, deferring the data movement that ordinary quicksort performs at every partition level.
- **Geometry-selected merge trees.** Use the run geometry produced by decomposition to decide when ordinary adjacent pairing is wasteful and selectively switch to a cheap sliding best-2-of-3 merge schedule rather than always building the same tree.
- **Ordered-boundary collapse and overlap trimming.** Before paying for a merge, remove already-ordered run boundaries and trim non-overlapping prefixes/suffixes so only the truly interleaved window is moved. Not new; useful for Patience piles.
- **Bidirectional branchless merging.** Merge from both ends of two runs simultaneously to expose more independent work and reduce dependence on one forward comparison chain. Not new; useful for Patience piles.
- **Cross-game pile reassociation.** Pair ascending- and descending-game piles by rank, endpoints, or stitchability instead of collapsing each game separately. This resembles adaptive merge scheduling but operates across nonadjacent latent runs from opposing Patience decompositions.

A recurring design theme is **delaying irreversible data movement until cheap structural observations have made the next decision more informed**. Blueprints postpone pile construction, freezing stops preserving unnecessary exact structure, overflow postpones organization, and spatial probing postpones global commitment.

## Build and benchmark

Build with GCC/libstdc++:

```bash
make
make bench
```

Routine `make bench` benchmarks three representatives: simulated-direct_live-phase, adaptive-noalloc, and strict-noalloc. Additional research variants are preserved under `src/research/experimental/` and `include/jessesort/research/experimental/`, but are not part of the routine benchmark build.

The canonical rows are Random, Sorted, Reverse, Sorted+Noise(5%), Sorted+Noise(10%), Random%25, Alternating, Sawtooth, MixedDirectionRuns, BlockSorted, OrganPipe, Rotated, MixedPhase3 (Sorted+Noise(5%) -> MixedDirectionRuns -> Random), and MixedPhase12 (all 12 baseline families once each in baseline order) as phase-change router-safety inputs; the original 12 generators and identities remain unchanged. Sawtooth uses an n^(2/3)-scaled regular ramp period; BlockSorted and MixedDirectionRuns use seeded variable structural lengths centered on n^(2/3), so both run count and run length grow with input size.

The benchmark writes an isolated, gitignored `results/run_*` directory containing Markdown/CSV summaries, raw paired trial data, metadata, and progress state. Interrupted runs can be resumed using the existing run directory.

Equivalent direct build:

```bash
g++ -std=c++20 -O3 -march=native -DNDEBUG \
  -Wall -Wextra -Wpedantic -Iinclude \
  benchmarks/benchmark.cpp src/*.cpp \
  -o benchmark
```

### Clang with libc++

Install libc++ and libc++abi development packages, then run:

```bash
make clean
make clang
make bench-clang
```

## Basic use

The algorithms are template-based and implemented in their headers. For example, V2 can be used as:

```cpp
#include <jessesort/jessesort.h>

#include <vector>

int main() {
    std::vector<int> values{7, 2, 9, 1, 5};
    jessesort::sort(values);
}
```

Comparators must provide a strict weak ordering. Value-type requirements depend on the selected layer-tagged variation. The physical, simulated, frozen, AVX2, linked, and their direct-merge descendants retain the legacy allocating implementations and generally require copy construction, copy assignment, move construction, and move assignment because some pile, blueprint, or merge metadata paths store values by copy. The indexed family instead stores pile-tail state by index and supports move-only values on its non-copyable path, requiring move construction and move assignment but not copy construction or copy assignment. The noalloc variations likewise require only movability and are specifically designed to perform no heap allocation inside the sort; the noalloc-low-run variation additionally includes shared fixed tail storage and its selective low-run in-place merge path.

These requirements are properties of the current implementations and variation policies, not known fundamental requirements of the Jessesort algorithm. Floating-point inputs containing NaNs are not supported with the default comparator because ordinary floating-point < does not provide the required strict weak ordering over NaNs; use an explicit NaN-aware comparator, such as a total-order comparator.

## Development status

Jessesort is still experimental. The compact current roadmap and the detailed retained/rejected experiment history are maintained in [`docs/experiment_log.txt`](docs/experiment_log.txt). It's getting long...

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ substantially. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
