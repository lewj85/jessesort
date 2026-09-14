# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

## Description

### **tl;dr Jessesort plays two games of Solitaire, one game with descending piles and one with ascending piles, then merges the piles together. Jessesort is up to 58x faster than `std::sort` with GCC/libstdc++, up to 11x faster than `std::sort` with Clang/libc++, and up to 22x faster than ipnsort in Rust.**

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience**. Patience is a card game similar to Solitaire, where the cards don't have to be sequential, but the tails of each pile have to be kept in sorted order. This additional restriction allows new values to quickly find their correct pile via binary search over just these sorted pile tails.

Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs, which generate n 1-element piles--by routing inputs to two simultaneous games based on the current run order. One game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles `k` typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push `k` lower towards `O(n)` linear work. Maintained variations are deterministic and unstable.

```
Best    Average     Worst       Memory‡                   Stable    Deterministic
n       n log k     n log n     n + k (allocating)        No        Yes
                                log n (non-allocating)
```

‡ Regarding memory: Several `noalloc` implementations are maintained in the repo that are allocation-free/move-only. They use fixed bounded metadata (≤8 KiB), no heap allocation inside the sort, direct structural routes for several low-entropy geometries, and allocation-free fallback behavior. Simulated variations do allocate though, with `n` 32-bit auxiliary storage plus base array copies of size `k`.

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

## Speed tests

Each benchmark runs 100 independent five-input executions (ABCDE), for 500 timed trials total. Within each execution, the first two timed runs (AB) are treated as warmup/transition measurements and excluded from the reported benchmark results. So only 300 of the 500 total (CDE) are kept for the aggregated results below.

Below are speed comparisons. **The numbers below are how many times faster Jessesort is than the competitor.** For example, 3.89 means Jessesort is 3.89x faster.

### std::sort with GCC (libstdc++)

Cells are **median paired ratio vs `std::sort`** from 300 CDE trials on n=100k elements using an Intel Xeon Platinum 8573C with GCC 14.2.0 (libstdc++), CPU0 pinned, in C++20 with `-O3 -march=native -DNDEBUG`.

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 3.78× | **3.89×** | 3.83× |
| Sorted | 27.38× | 27.91× | **28.12×** |
| Reverse | 9.60× | 9.49× | **9.78×** |
| Sorted+Noise(5%) | **2.08×** | 1.48× | 1.40× |
| Random%25 | 5.35× | 6.04× | **6.09×** |
| Diverging | **3.12×** | 1.41× | 1.40× |
| Sawtooth | **6.28×** | 3.75× | 1.30× |
| MixedDirectionRuns | 9.95× | **12.60×** | 8.81× |
| OrganPipe | 54.12× | 53.10× | **57.87×** |
| Rotated | 52.31× | 51.96× | **52.57×** |
| AlternatingDirectionRuns | 9.25× | **12.41×** | 7.58× |
| UnevenRunMosaic | 10.02× | **16.36×** | 15.38× |
| MonotoneBurstNoise | 2.92× | **4.03×** | 3.90× |
| SparseInversionPatches | 6.42× | 21.86× | **23.70×** |
| WindowShuffle | 1.23× | **1.49×** | 1.47× |
| PlateauStaircase | **2.10×** | 1.05× | 1.04× |
| DuplicateRunMosaic | **3.63×** | 2.30× | 2.33× |
| RandomCardinalityK2 | 0.93× | 6.22× | **6.32×** |
| ClusteredDuplicates | 5.14× | 5.55× | **5.58×** |
| InterleavedLanes | 2.93× | 3.03× | **3.07×** |
| AlternatingWithJitter | **1.78×** | 1.72× | 1.71× |
| WarpedBitonic | **40.79×** | 31.69× | 17.51× |
| Reverse+Noise(5%) | **1.35×** | 1.22× | 0.97× |
| MultiTurnAffine | **12.50×** | 11.53× | 4.98× |
| OffsetRotationRamp | 19.89× | **43.69×** | 39.31× |
| AllEqual | 19.16× | **19.34×** | 19.03× |
| DiscontinuousAffinePhases | 25.44× | 38.34× | **38.66×** |
| NoisyAffinePhases | 1.80× | **1.95×** | 1.91× |
| OverlappingSortedBlocks | **6.02×** | 1.94× | 1.61× |
| RandomWalk | 1.69× | **1.73×** | 1.72× |
| StickyRandomWalk | 0.89× | **1.78×** | 1.32× |
| PeriodicPerturbed | 1.38× | 2.96× | **3.03×** |
| ChunkEntropyMixture | 2.96× | **2.98×** | 1.87× |
| LocalizedAlternatingBursts | 4.23× | 5.60× | **5.84×** |
| RandomCardinalityK5 | 2.19× | **5.44×** | 5.29× |
| RandomCardinalityK10 | 5.38× | **6.21×** | 6.21× |
| RandomCardinalityK50 | 5.33× | 5.88× | **5.89×** |
| RandomCardinalityK100 | 5.28× | **5.95×** | 5.75× |
| RandomCardinalityK256 | 5.23× | **5.70×** | 5.57× |
| RandomCardinalityK1024 | 5.04× | 5.23× | **5.33×** |
| RandomCardinalityK4096 | 4.76× | **4.95×** | 4.93× |
| RandomCardinalityK16384 | **3.94×** | 3.91× | 3.83× |
| Converging | 1.89× | **2.01×** | 1.95× |
| BlockShuffle16 | 0.81× | 0.76× | **3.59×** |
| BlockShuffle32 | 1.09× | **3.47×** | 3.46× |
| BlockShuffle64 | 1.16× | 3.36× | **3.41×** |
| BlockShuffle128 | 1.22× | **3.26×** | 3.14× |
| BlockShuffle256 | 1.23× | **2.94×** | 2.90× |
| BlockShuffle512 | 1.37× | **2.79×** | 2.77× |
| BlockShuffle1024 | 1.44× | 2.54× | **2.61×** |
| BlockShuffle4096 | **1.75×** | 1.44× | 0.42× |
| BlockShuffle16384 | **2.10×** | 0.59× | 0.59× |
| SortedNoise1 | **2.68×** | 1.66× | 1.54× |
| SortedNoise2 | **1.62×** | 0.99× | 0.97× |
| SortedNoise10 | 1.62× | 1.29× | **1.79×** |
| SortedNoise20 | 2.00× | **2.10×** | 1.60× |
| SortedNoise30 | 2.33× | **2.42×** | 1.80× |
| MixedPhase3 | 3.01× | **3.19×** | 2.55× |
| BlockSorted | **20.00×** | 18.26× | 18.40× |
| DiversePhaseMix8 | **3.26×** | 2.94× | 1.80× |

CDE median-speedup geometric mean across the maintained 60-input suite: simulated-direct_live-phase **3.83×**, adaptive-noalloc **4.35×**, strict-noalloc **3.98×**.

### std::sort with Clang (libc++)

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. We provide make targets for clang support, but little testing has been done to actually ensure the code is compiling as expected (e.g., branchless behavior). We show one table of clang timings below for reference (300 CDE trials, n=100k), using an AMD Ryzen 7 7445HS CPU.

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 3.41× | **3.48×** | 3.46× |
| Sorted | 1.54× | **1.83×** | 1.82× |
| Reverse | **1.50×** | 1.36× | **1.50×** |
| Sorted+Noise(5%) | **1.71×** | 0.63× | 1.08× |
| Random%25 | 3.66× | 3.75× | **3.76×** |
| Diverging | **1.11×** | 0.88× | 0.88× |
| Sawtooth | **1.17×** | 0.75× | 0.30× |
| MixedDirectionRuns | **1.36×** | 0.82× | 0.75× |
| OrganPipe | **10.81×** | 8.07× | 7.59× |
| Rotated | **3.97×** | 3.72× | 3.91× |
| AllEqual | **2.86×** | 1.52× | 2.85× |
| AlternatingDirectionRuns | **1.19×** | 0.81× | 0.72× |
| AlternatingWithJitter | 1.26× | **1.27×** | 1.25× |
| BlockShuffle1024 | 1.64× | **2.14×** | 2.12× |
| BlockShuffle128 | 1.53× | **2.75×** | 2.73× |
| BlockShuffle16 | 0.86× | 0.62× | **3.22×** |
| BlockShuffle16384 | **2.19×** | 0.38× | 0.38× |
| BlockShuffle256 | 1.56× | **2.56×** | 2.54× |
| BlockShuffle32 | 1.45× | **3.09×** | 3.07× |
| BlockShuffle4096 | **1.79×** | 0.78× | 0.28× |
| BlockShuffle512 | 1.60× | **2.35×** | 2.33× |
| BlockShuffle64 | 1.49× | **2.93×** | 2.92× |
| BlockSorted | **4.57×** | 0.81× | 0.81× |
| ChunkEntropyMixture | **1.65×** | 1.22× | 0.88× |
| ClusteredDuplicates | 3.88× | **4.04×** | 4.03× |
| Converging | 0.88× | **0.90×** | 0.89× |
| DiscontinuousAffinePhases | **3.41×** | 1.54× | 1.50× |
| DiversePhaseMix8 | **1.53×** | 1.13× | 0.74× |
| DuplicateRunMosaic | 0.96× | **1.56×** | 1.54× |
| InterleavedLanes | 2.71× | **2.77×** | 2.74× |
| LocalizedAlternatingBursts | 1.27× | 1.80× | **1.84×** |
| MixedPhase3 | **1.66×** | 1.45× | 1.23× |
| MonotoneBurstNoise | 1.15× | 1.60× | **1.66×** |
| MultiTurnAffine | **3.94×** | 2.07× | 1.42× |
| NoisyAffinePhases | **1.03×** | 1.03× | 0.98× |
| OffsetRotationRamp | **5.10×** | 2.88× | 2.86× |
| OverlappingSortedBlocks | **2.40×** | 0.56× | 0.51× |
| PeriodicPerturbed | 1.66× | **2.58×** | 2.55× |
| PlateauStaircase | 0.36× | 0.52× | **0.52×** |
| RandomCardinalityK10 | 3.37× | 3.43× | **3.44×** |
| RandomCardinalityK100 | 3.82× | **4.00×** | 3.97× |
| RandomCardinalityK1024 | 3.95× | 3.99× | **4.00×** |
| RandomCardinalityK16384 | 3.34× | **3.40×** | 3.37× |
| RandomCardinalityK2 | 0.29× | 2.41× | **2.53×** |
| RandomCardinalityK256 | 3.92× | 4.03× | **4.04×** |
| RandomCardinalityK4096 | 3.89× | 4.06× | **4.06×** |
| RandomCardinalityK5 | 0.79× | 1.16× | **1.19×** |
| RandomCardinalityK50 | 3.77× | **3.91×** | 3.91× |
| RandomWalk | 1.24× | **1.27×** | 1.26× |
| Reverse+Noise(5%) | **0.76×** | 0.53× | 0.43× |
| SortedNoise1 | 0.55× | **0.81×** | 0.79× |
| SortedNoise2 | 0.57× | **0.60×** | 0.58× |
| SortedNoise10 | **1.38×** | 0.65× | 1.33× |
| SortedNoise20 | **1.42×** | 1.34× | 0.99× |
| SortedNoise30 | **1.60×** | 1.56× | 1.17× |
| SparseInversionPatches | 1.72× | 5.40× | **5.82×** |
| StickyRandomWalk | 0.65× | **1.05×** | 0.77× |
| UnevenRunMosaic | **1.03×** | 0.81× | 0.78× |
| WarpedBitonic | **5.50×** | 2.73× | 1.40× |
| WindowShuffle | 0.74× | 0.95× | **0.96×** |

CDE median-speedup geometric mean across the maintained 60-input suite: simulated-direct 1.73×, adaptive-noalloc 1.61×, strict-noalloc 1.57×.

### ipnsort

Results below are direct timing comparisons (300 CDE trials, n=100k) with an AMD Ryzen 7 7445HS CPU. Both algorithms ran in their native language: ipnsort was run in Rust, Jessesort was run in C++. See the `src/ipnsort_vs_jessesort/` folder for more details.

| Input | simulated-direct_live-phase | adaptive-noalloc | strict-noalloc |
|---|---:|---:|---:|
| Random | 0.9360× | **0.9679×** | 0.9658× |
| Sorted | **1.8966×** | 0.9911× | 0.9916× |
| Reverse | **0.9768×** | 0.9747× | 0.9699× |
| Sorted+Noise(5%) | **1.2499×** | 0.8526× | 0.9433× |
| Random%25 | 0.8589× | 0.8908× | **0.8909×** |
| Diverging | 0.6922× | **0.9376×** | 0.9336× |
| Sawtooth | **2.2025×** | 0.9181× | 0.3855× |
| MixedDirectionRuns | 3.6798× | **3.8303×** | 2.8584× |
| OrganPipe | 8.2914× | **8.9416×** | 8.2622× |
| Rotated | 18.9364× | 18.8466× | **18.9603×** |
| AlternatingDirectionRuns | 3.6283× | **3.6893×** | 2.6093× |
| UnevenRunMosaic | 3.4625× | **4.2311×** | 3.8299× |
| MonotoneBurstNoise | 2.6952× | **4.6632×** | 4.3871× |
| SparseInversionPatches | 4.6751× | **22.3360×** | 15.3883× |
| WindowShuffle | 0.8902× | **1.1831×** | 1.1617× |
| PlateauStaircase | **1.6114×** | 0.9103× | 0.8743× |
| DuplicateRunMosaic | **1.4646×** | 0.9577× | 0.9434× |
| RandomCardinalityK2 | 0.0821× | 0.9362× | **0.9377×** |
| ClusteredDuplicates | 0.8812× | **0.9263×** | 0.9131× |
| InterleavedLanes | 0.8653× | 0.8999× | **0.9016×** |
| AlternatingWithJitter | **0.9282×** | 0.9268× | 0.9258× |
| WarpedBitonic | **7.7075×** | 5.7448× | 3.0717× |
| Reverse+Noise(5%) | **0.8320×** | 0.7480× | 0.6086× |
| MultiTurnAffine | **5.9340×** | 4.9185× | 2.9910× |
| OffsetRotationRamp | 7.1005× | 13.3076× | **14.2412×** |
| AllEqual | 1.8715× | 1.9010× | **1.9041×** |
| DiscontinuousAffinePhases | 6.5976× | **8.8209×** | 8.0585× |
| NoisyAffinePhases | 0.8778× | **1.0089×** | 0.9834× |
| OverlappingSortedBlocks | **3.5116×** | 2.1496× | 0.9038× |
| RandomWalk | 0.9042× | 0.9259× | **0.9264×** |
| StickyRandomWalk | 0.3658× | **0.7912×** | 0.6097× |
| PeriodicPerturbed | 0.4618× | 0.9608× | **0.9611×** |
| ChunkEntropyMixture | **0.9225×** | 0.8833× | 0.6094× |
| LocalizedAlternatingBursts | 4.1501× | **7.2516×** | 6.5395× |
| RandomCardinalityK5 | 0.3332× | **0.6738×** | 0.6691× |
| RandomCardinalityK10 | 0.8250× | 0.8528× | **0.8568×** |
| RandomCardinalityK50 | 0.8571× | 0.9008× | **0.9045×** |
| RandomCardinalityK100 | 0.8665× | 0.9030× | **0.9067×** |
| RandomCardinalityK256 | 0.8731× | **0.9071×** | 0.9054× |
| RandomCardinalityK1024 | 0.8781× | **0.9182×** | 0.9142× |
| RandomCardinalityK4096 | 1.0359× | **1.0841×** | 1.0810× |
| RandomCardinalityK16384 | 0.9249× | 0.9511× | **0.9524×** |
| Converging | 0.7886× | **0.8436×** | 0.8427× |
| BlockShuffle16 | 0.2012× | 0.2161× | **0.9644×** |
| BlockShuffle32 | 0.3039× | **0.9696×** | 0.9675× |
| BlockShuffle64 | 0.3318× | 0.9653× | **0.9654×** |
| BlockShuffle128 | 0.3686× | **0.9658×** | 0.9649× |
| BlockShuffle256 | 0.4111× | **0.9655×** | 0.9628× |
| BlockShuffle512 | 0.4731× | **0.9688×** | 0.9602× |
| BlockShuffle1024 | 0.5368× | **0.9574×** | 0.9435× |
| BlockShuffle4096 | **0.7836×** | 0.6077× | 0.1943× |
| BlockShuffle16384 | **1.1367×** | 0.3370× | 0.3363× |
| SortedNoise1 | **1.1944×** | 0.8862× | 0.8755× |
| SortedNoise2 | **0.9178×** | 0.6545× | 0.6254× |
| SortedNoise10 | 0.8503× | 0.7031× | **0.9926×** |
| SortedNoise20 | 0.8562× | **0.8835×** | 0.6865× |
| SortedNoise30 | 0.8391× | **0.8715×** | 0.6609× |
| MixedPhase3 | 1.0393× | **1.0625×** | 0.9116× |
| BlockSorted | **5.5037×** | 4.2858× | 4.3949× |
| DiversePhaseMix8 | **1.2481×** | 1.0870× | 0.6712× |

While ipnsort wins two-thirds of the individual cells, **JesseSort actually wins the geometric mean because its structured victories are often enormous.** The adaptive-noalloc variation of Jessesort is geometrically **about 40% faster than ipnsort** overall.

CDE median-speedup geometric mean across the maintained 60-input suite: simulated-direct **1.21×**, adaptive-noalloc **1.39×**, strict-noalloc **1.27×** versus ipnsort.

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
  -Wall -Wextra -Wpedantic -Iinclude -Ibenchmarks/infrastructure \
  benchmarks/canonical/benchmark.cpp \
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

I need to finish my dissertation.