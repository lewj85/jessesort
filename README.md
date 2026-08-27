# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

## Description

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience insertion**. Patience is a card game similar to Solitaire, where the tails of each pile are also kept in sorted order. This additional restriction allows new values to quickly find their correct pile via binary search over just these sorted pile tails.

Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs, which generate n 1-element piles--by routing inputs to two simultaneous games based on current run order. One game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles _k_ typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push _k_ lower towards `O(n)` linear work. Maintained variations are deterministic and unstable.

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     n + k       No          Yes
```

Regarding memory: most simulated implementations use `O(n)` auxiliary storage. Many implementations also use base array copies of size _k_. The maintained `noalloc-direct` branch is the current allocation-free/move-only Rust-contract contender: it uses fixed bounded metadata, no heap allocation inside the sort, direct structural routes for several low-entropy geometries, and allocation-free fallback behavior. Older no-allocation architectures remain preserved as research variants. `noalloc-direct` is the general routine no-allocation representative, while `noalloc-low-run` is also in the routine roster as a specialized low-run/high-entropy benchmark descriptor.

## Speed tests

Jessesort is up to 58x faster than `std::sort` with GCC/libstdc++, up to 51x faster than `std::sort` with Clang/libc++, and up to 13x faster than `ipnsort` in Rust. See below for speed comparisons.

### std::sort with GCC (libstdc++)

Cells are **median paired ratio vs `std::sort` (median microseconds)** from 500 trials on n=100k elements using an Intel Xeon Platinum 8573C with GCC 14.2.0 (libstdc++) in C++20 with `-O3 -march=native -DNDEBUG`. The full 20-variation x 14-input x four-size baseline is saved in `benchmarks/baselines/e229/e229_all_variations_combined.md`.

**Scores less than 1.0 mean Jessesort is faster.**

| Input | physical | simulated | frozen-single | indexed | noalloc-direct | noalloc-low-run | simulated-direct | phase-map | std::sort |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Random | 0.2654 (1577.423) | 0.2668 (1585.694) | 0.2636 (1567.194) | 0.2629 (1562.732) | 0.2625 (1560.652) | 0.2612 (1552.638) | **0.2572 (1529.135)** | 0.2615 (1554.269) | 1.0000 (5944.322) |
| Sorted | **0.0383 (29.063)** | 0.0383 (29.114) | 0.0450 (34.180) | 0.0759 (57.622) | 0.0388 (29.458) | 0.0387 (29.360) | 0.0387 (29.375) | 0.0387 (29.405) | 1.0000 (759.536) |
| Reverse | 0.0695 (36.944) | 0.0691 (36.690) | 0.0773 (41.090) | 0.0695 (36.948) | 0.0678 (35.998) | **0.0676 (35.901)** | 0.0678 (35.993) | 0.0678 (36.012) | 1.0000 (531.246) |
| Sorted+Noise(5%) | 0.9047 (2067.896) | 0.5554 (1269.578) | 0.6351 (1451.598) | 0.6912 (1579.987) | 0.6528 (1492.188) | 0.6955 (1589.705) | **0.4814 (1100.343)** | 0.4918 (1124.186) | 1.0000 (2285.794) |
| Sorted+Noise(10%) | 1.0047 (2691.110) | 0.6940 (1858.928) | 0.6961 (1864.534) | 0.7923 (2122.408) | **0.5636 (1509.546)** | 0.5710 (1529.444) | 0.6435 (1723.576) | 0.6464 (1731.440) | 1.0000 (2678.627) |
| Random%25 | **0.0455 (107.790)** | 0.0462 (109.620) | 0.0456 (108.048) | 0.0464 (109.895) | 0.0457 (108.396) | 0.0460 (109.046) | 0.0461 (109.385) | 0.0457 (108.391) | 1.0000 (2370.714) |
| Alternating | 0.4332 (932.994) | 0.2540 (547.042) | 0.3525 (759.280) | 0.3418 (736.155) | **0.0301 (64.889)** | 0.9036 (1946.385) | 0.0742 (159.721) | 0.0756 (162.771) | 1.0000 (2153.963) |
| Sawtooth | 0.8351 (2013.917) | 0.4827 (1163.999) | 0.6989 (1685.394) | 0.5651 (1362.880) | 0.3660 (882.554) | 0.3713 (895.341) | 0.1040 (250.883) | **0.1020 (245.866)** | 1.0000 (2411.554) |
| MixedDirectionRuns | 0.3964 (1112.287) | 0.2152 (603.924) | 0.2923 (820.116) | 0.2927 (821.140) | **0.1423 (399.221)** | 0.5815 (1631.677) | 0.1556 (436.719) | 0.1426 (400.124) | 1.0000 (2805.863) |
| BlockSorted | 0.3409 (1045.224) | 0.1689 (517.772) | 0.2199 (674.330) | 0.1773 (543.636) | **0.0542 (166.332)** | 0.4496 (1378.508) | 0.0618 (189.430) | 0.0611 (187.210) | 1.0000 (3066.346) |
| OrganPipe | 0.0884 (578.328) | 0.0638 (417.263) | 0.0843 (551.038) | 0.0679 (443.934) | **0.0183 (119.806)** | 0.1241 (811.260) | 0.0386 (252.607) | 0.0386 (252.277) | 1.0000 (6538.476) |
| Rotated | 0.2372 (448.260) | 0.1746 (329.916) | 0.2395 (452.654) | 0.1759 (332.361) | **0.0231 (43.659)** | 0.2537 (479.422) | 0.0737 (139.269) | 0.0742 (140.230) | 1.0000 (1889.880) |
| MixedPhase3 | 1.0655 (4346.956) | 0.7288 (2973.396) | 0.9016 (3678.400) | 0.7861 (3207.010) | 0.3971 (1620.120) | 0.3917 (1598.164) | 0.7236 (2952.118) | **0.3193 (1302.604)** | 1.0000 (4079.810) |
| MixedPhase12 | 0.4297 (1463.518) | 0.4226 (1439.075) | 0.4300 (1464.488) | 0.4283 (1458.620) | 0.4222 (1437.902) | 0.4256 (1449.534) | 0.4273 (1455.270) | **0.4056 (1381.382)** | 1.0000 (3405.626) |

### std::sort with Clang (libc++)

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless behavior). We show one table of clang timings below for reference, using the simulated-direct Jessesort variation and an Intel i9-13900K CPU.

| Input | physical | simulated | frozen-single | indexed | noalloc-direct | noalloc-low-run | simulated-direct | simulated-direct-phase-map-mature | std::sort |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Random | 0.5879 (2293.022) | 0.5894 (2298.891) | 0.5907 (2303.964) | 0.5975 (2330.550) | 0.5859 (2285.400) | 0.5939 (2316.530) | **0.5815 (2268.186)** | 0.5881 (2293.841) | 1.0000 (3900.424) |
| Sorted | 0.0477 (18.387) | **0.0474 (18.296)** | 0.0582 (22.452) | 0.0483 (18.619) | 0.0481 (18.539) | 0.0475 (18.330) | 0.0958 (36.959) | 0.0945 (36.457) | 1.0000 (385.793) |
| Reverse | 0.0819 (23.101) | 0.0824 (23.224) | 0.0969 (27.303) | 0.0826 (23.277) | 0.0804 (22.661) | 0.0811 (22.854) | **0.0801 (22.580)** | 0.0821 (23.151) | 1.0000 (281.901) |
| Sorted+Noise(5%) | 1.1250 (1573.132) | 0.8173 (1142.869) | 1.1725 (1639.559) | 0.8951 (1251.549) | 0.6224 (870.270) | 1.7024 (2380.417) | 0.5248 (733.818) | **0.5216 (729.397)** | 1.0000 (1398.293) |
| Sorted+Noise(10%) | 1.2144 (2079.845) | 0.9618 (1647.251) | 1.2339 (2113.338) | 1.0591 (1813.945) | 1.3418 (2298.154) | 1.3543 (2319.466) | **0.6750 (1156.009)** | 0.6803 (1165.101) | 1.0000 (1712.710) |
| Random%25 | 0.1848 (282.707) | 0.1848 (282.680) | 0.1856 (283.943) | 0.1879 (287.409) | 0.1877 (287.200) | **0.1846 (282.392)** | 0.1846 (282.477) | 0.1865 (285.255) | 1.0000 (1529.861) |
| Alternating | 0.6900 (938.976) | 0.6648 (904.735) | 0.8736 (1188.871) | 0.6724 (915.060) | **0.0304 (41.371)** | 1.9774 (2690.944) | 0.0748 (101.839) | 0.0735 (99.998) | 1.0000 (1360.845) |
| Sawtooth | 1.2847 (1930.086) | 0.8865 (1331.851) | 1.3929 (2092.562) | 0.9367 (1407.222) | 1.0888 (1635.783) | 1.0917 (1640.050) | **0.0951 (142.829)** | 0.0962 (144.460) | 1.0000 (1502.330) |
| MixedDirectionRuns | 0.7521 (1152.206) | 0.4922 (754.002) | 0.7360 (1127.488) | 0.4816 (737.800) | **0.1155 (176.957)** | 0.6429 (984.902) | 0.1463 (224.047) | 0.1323 (202.709) | 1.0000 (1531.904) |
| BlockSorted | 0.6647 (1103.289) | 0.3655 (606.621) | 0.6649 (1103.677) | 0.4056 (673.227) | **0.0664 (110.196)** | 0.5245 (870.619) | 0.0732 (121.481) | 0.0727 (120.749) | 1.0000 (1659.823) |
| OrganPipe | 0.1489 (557.467) | 0.1004 (375.935) | 0.1663 (622.595) | 0.0989 (370.490) | **0.0196 (73.433)** | 0.1349 (505.187) | 0.0674 (252.404) | 0.0679 (254.251) | 1.0000 (3744.768) |
| Rotated | 0.4152 (438.861) | 0.1893 (200.044) | 0.4278 (452.212) | 0.1912 (202.094) | **0.0263 (27.835)** | 0.2752 (290.900) | 0.0821 (86.831) | 0.0814 (86.035) | 1.0000 (1057.019) |
| MixedPhase3 | 1.1673 (2940.685) | 1.0098 (2544.124) | 1.2874 (3243.412) | 1.1060 (2786.410) | 0.9426 (2374.769) | 0.9557 (2407.620) | 1.0381 (2615.185) | **0.6025 (1518.006)** | 1.0000 (2519.314) |
| MixedPhase12 | 1.1146 (2216.262) | 1.1201 (2227.361) | 1.1162 (2219.608) | 1.1178 (2222.663) | 1.1064 (2200.035) | 1.1015 (2190.229) | 1.1100 (2207.255) | **0.6636 (1319.612)** | 1.0000 (1988.460) |

### ipnsort

Results below are direct timing comparisons with an AMD Ryzen 7 7445HS CPU. Both algorithms ran in their native language: ipnsort was run in Rust, Jessesort was run in C++. See the `src/ipnsort_vs_jessesort/` folder for more details.

- type: u64
- n: 100000
- trials per pattern: 500
- warmups per pattern: 2
- shared cold-like preconditioner: false
- inputs: Jessesort benchmark inputs, order-preserving int->u64 encoding

| Pattern | ipnsort µs | physical µs | simulated µs | simulated-direct µs | indexed µs | noalloc µs | noalloc-low-run µs | simulated/ipnsort | simulated-direct/ipnsort | best Jessesort/ipnsort |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Random | 932.327 | 1107.630 | 1106.707 | 1121.137 | 1104.991 | 1099.430 | 1105.214 | 1.187 | 1.203 | 1.179 |
| Sorted | 21.227 | 42.284 | 31.861 | 31.901 | 42.374 | 21.277 | 21.348 | 1.501 | 1.503 | 1.002 |
| Reverse | 27.598 | 30.869 | 73.131 | 73.111 | 94.147 | 30.949 | 31.019 | 2.650 | 2.649 | 1.119 |
| Sorted+Noise(5%) | 987.791 | 1898.816 | 1122.736 | 771.332 | 1088.610 | 1160.015 | 1161.144 | 1.137 | 0.781 | 0.781 |
| Sorted+Noise(10%) | 1028.002 | 2346.818 | 1184.158 | 1169.915 | 1474.362 | 1170.976 | 1171.597 | 1.152 | 1.138 | 1.138 |
| Random%25 | 251.660 | 105.907 | 105.819 | 106.343 | 105.841 | 106.037 | 106.102 | 0.420 | 0.423 | 0.420 |
| Alternating | 904.247 | 1282.351 | 366.404 | 351.375 | 615.324 | 1231.776 | 1238.690 | 0.405 | 0.389 | 0.389 |
| Sawtooth | 589.337 | 1963.941 | 688.143 | 494.755 | 1045.841 | 1942.803 | 645.240 | 1.168 | 0.840 | 0.840 |
| MixedDirectionRuns | 892.322 | 1381.436 | 337.160 | 281.571 | 652.214 | 2171.173 | 1347.977 | 0.378 | 0.316 | 0.316 |
| BlockSorted | 821.189 | 1396.120 | 269.755 | 110.150 | 624.697 | 2334.664 | 1316.673 | 0.328 | 0.134 | 0.134 |
| OrganPipe | 1007.441 | 921.839 | 190.435 | 140.377 | 192.601 | 1402.265 | 505.183 | 0.189 | 0.139 | 0.139 |
| Rotated | 758.514 | 857.853 | 179.358 | 54.530 | 151.373 | 128.444 | 129.285 | 0.236 | 0.072 | **0.072** |

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

The names are intended to identify the architectural choices preserved by each implementation. Some variants are routine benchmark representatives; others remain under `experimental/` so specific mechanisms can still be measured independently even when they are not currently the fastest overall.

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

Several of these families have direct, linked, in-place, phase-map, move-only, or no-allocation descendants. Those suffixes identify combinations of the same architectural dimensions rather than entirely separate algorithms.

The section below describes the underlying mechanisms themselves. The variation names above are primarily a map from those mechanisms to the concrete implementations preserved in the repository.

### Distinctive mechanisms explored

Jessesort explores a collection of distinctive sorting mechanisms, including several that I haven't found in prior sorting literature. Some seem genuinely novel, while others are established ideas adapted to Patience-based sorting.

- **Dual-direction Patience decomposition.** Run ascending and descending Patience games simultaneously and route each new value toward the game matching its local run direction, avoiding Patience Sort's pathological treatment of natural runs.
- **Base arrays.** Mirror scattered physical pile tails into a compact contiguous search structure, optionally in Eytzinger order, avoiding pointer/vector traversal during pile selection. This differs from LIS implementations that retain only the tails array; here the array acts as a search mirror for still-materialized piles.
- **Persistent pile-search position.** Start each insertion search near the previous pile rather than restarting from pile zero, so sustained natural runs can stay close to linear-time insertion behavior. Here the locality idea is applied independently to the two Patience games, with a separate search-position tracker for each.
- **Blueprints / simulated piles.** Record pile assignments without physically constructing the corresponding piles, leaving a compact latent description of the decomposition.
- **Packed and bulk-decoded blueprints.** Store compact pile/game tags and reconstruct long same-destination spans in bulk rather than decoding one element at a time.
- **Linked blueprints and fused reconstruction.** Represent reconstruction topology with element/span links, including variants that consume the links directly into the first merge instead of fully materializing every run first.
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

A recurring design theme is **delaying irreversible data movement until cheap structural observations have made the next decision more informed**. Blueprints postpone pile construction, freezing stops preserving unnecessary exact structure, overflow postpones organization, and spatial probing postpones global commitment.

## Build and benchmark

Build with GCC/libstdc++:

```bash
make
make bench
```

Routine make bench benchmarks eight representatives: physical, simulated, frozen-single, indexed, noalloc-direct, noalloc-low-run, simulated-direct, and simulated-direct-phase-map-mature. Additional research variants are preserved under src/experimental/ and include/jessesort/experimental/, but are not part of the routine benchmark build.

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
#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>

#include <vector>

int main() {
    std::vector<int> values{7, 2, 9, 1, 5};
    jessesort::simulated::sort(values);
}
```

Comparators must provide a strict weak ordering. Value-type requirements depend on the selected layer-tagged variation. The physical, simulated, frozen, AVX2, linked, and their direct-merge descendants retain the legacy allocating implementations and generally require copy construction, copy assignment, move construction, and move assignment because some pile, blueprint, or merge metadata paths store values by copy. The indexed family instead stores pile-tail state by index and supports move-only values on its non-copyable path, requiring move construction and move assignment but not copy construction or copy assignment. The noalloc variations likewise require only movability and are specifically designed to perform no heap allocation inside the sort; the noalloc-low-run variation additionally includes shared fixed tail storage and its selective low-run in-place merge path.

These requirements are properties of the current implementations and variation policies, not known fundamental requirements of the Jessesort algorithm. Floating-point inputs containing NaNs are not supported with the default comparator because ordinary floating-point < does not provide the required strict weak ordering over NaNs; use an explicit NaN-aware comparator, such as a total-order comparator.

## Development status

Jessesort is still experimental. The compact current roadmap and the detailed retained/rejected experiment history are maintained in [`docs/experiment_log.txt`](docs/experiment_log.txt). E225 introduced high impact routing behavior, so experiments are planned around these new variations.

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ substantially. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
