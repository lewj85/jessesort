# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

## Description

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience insertion**. Patience is a card game similar to Solitaire, where the tails of each pile are also kept in sorted order. This additional restriction allows new values to quickly find their correct pile via binary search over just these sorted pile tails.

Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs, which generate n 1-element piles--by routing inputs to two simultaneous games based on current run order. One game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles _k_ typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push _k_ lower towards `O(n)` linear work. Maintained variations are deterministic and unstable.

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     n*          No          Yes
```

* Regarding memory: most simulated implementations use `O(n)` auxiliary storage; the maintained `noalloc` and `noalloc-low-run-merge` architectures preserve the experimental allocation-free branch, using only fixed bounded local metadata and no heap allocation inside the sort.

## Speed tests

Jessesort is up to 46x faster than `std::sort` with GCC/libstdc++, up to 14x faster than `std::sort` with Clang/libc++, and up to 13x faster than `ipnsort` in Rust. See below for speed comparisons.

### std::sort with GCC (libstdc++)

Cells are **median paired ratio vs `std::sort` (median microseconds)** from 500 trials on n=100k elements using an Intel Xeon Platinum 8573C with GCC 14.2.0 (libstdc++) in C++20 with `-O3 -march=native -DNDEBUG`. The full 20-variation x 14-input x four-size baseline is saved in `benchmarks/baselines/e229/e229_all_variations_combined.md`.

**Scores less than 1.0 mean Jessesort is faster.**

| Input | physical | simulated | frozen-single | indexed | noalloc-low-run | simulated-direct |
|---|---:|---:|---:|---:|---:|---:|
| Random | 0.2712 (1590.3) | 0.2622 (1545.7) | 0.2609 (1529.8) | 0.2642 (1543.2) | 0.2595 (1527.2) | 0.2628 (1548.1) |
| Sorted | 0.0383 (28.3) | 0.0382 (28.2) | 0.0448 (33.1) | 0.0385 (28.2) | 0.0384 (28.4) | 0.0383 (28.2) |
| Reverse | 0.0689 (35.1) | 0.0687 (34.8) | 0.0782 (39.7) | 0.0686 (34.8) | 0.0688 (34.9) | 0.0684 (34.8) |
| Sorted+Noise(5%) | 0.8267 (1814.3) | 0.5419 (1193.1) | 0.7031 (1538.9) | 0.6692 (1474.4) | 0.7149 (1551.7) | 0.5364 (1188.1) |
| Sorted+Noise(10%) | 0.8492 (2279.5) | 0.5913 (1629.2) | 0.6836 (1868.5) | 0.7036 (1929.8) | 0.5610 (1493.5) | 0.5940 (1627.0) |
| Random%25 | 0.0466 (111.0) | 0.0456 (108.1) | 0.0453 (107.1) | 0.0455 (107.6) | 0.0462 (108.2) | 0.0455 (107.8) |
| Alternating | 0.3460 (732.1) | 0.2017 (485.4) | 0.2578 (538.1) | 0.2858 (672.1) | 0.9411 (1978.6) | 0.2091 (485.4) |
| Sawtooth | 0.6879 (1625.7) | 0.4466 (1054.8) | 0.6428 (1514.9) | 0.4956 (1176.4) | 0.3911 (911.4) | 0.2858 (668.8) |
| MixedDirectionRuns | 0.2728 (703.0) | 0.1320 (337.0) | 0.2660 (688.3) | 0.2113 (539.8) | 0.5800 (1533.4) | 0.0767 (207.9) |
| BlockSorted | 0.2542 (710.9) | 0.1097 (303.2) | 0.2449 (678.4) | 0.1772 (490.9) | 0.4826 (1355.4) | 0.0322 (90.2) |
| OrganPipe | 0.0449 (277.0) | 0.0350 (216.4) | 0.0510 (324.0) | 0.0347 (217.1) | 0.1185 (740.2) | 0.0249 (153.8) |
| Rotated | 0.1373 (258.5) | 0.0640 (111.9) | 0.1336 (266.5) | 0.0701 (114.3) | 0.2546 (535.4) | **0.0215** (46.3) |
| MixedPhase3 | 0.9986 (4063.6) | 0.6710 (2748.2) | 0.7998 (3269.8) | 0.7333 (2992.6) | 0.4115 (1655.7) | 0.6753 (2730.0) |
| MixedPhase12 | 0.4858 (1567.2) | 0.4812 (1534.7) | 0.4835 (1542.7) | 0.4789 (1519.0) | 0.4709 (1504.6) | 0.4802 (1529.6) |

### std::sort with Clang (libc++)

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless behavior). We show one table of clang timings below for reference, using the simulated-direct Jessesort variation and an AMD Ryzen 7 7445HS CPU.

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 2.5912 (47.6410) | 0.3286 (92.2575) | 0.2917 (1050.1445) | 0.2738 (11868.5975) |
| Sorted | 0.8820 (0.9750) | 0.6880 (6.6680) | 0.6625 (42.3745) | 0.6493 (423.8110) |
| Reverse | 0.8305 (1.3070) | 0.7066 (6.7385) | 0.7043 (63.6520) | 0.7348 (647.4190) |
| Sorted+Noise(5%) | 1.6842 (12.7375) | 0.9132 (96.6380) | 0.9993 (1269.0590) | 1.0213 (14821.2860) |
| Sorted+Noise(10%) | 2.0823 (18.4105) | 1.0636 (139.4150) | 1.1304 (1758.5940) | 1.0612 (18976.3045) |
| Random%25 | 3.4992 (31.1120) | 0.1155 (11.1950) | 0.0802 (87.2535) | 0.0815 (871.7235) |
| Alternating | 1.3219 (7.2705) | 0.7856 (38.1410) | 0.7407 (719.3535) | 0.7182 (8171.6595) |
| Sawtooth | 2.2065 (14.0835) | 0.5389 (50.6010) | 0.6076 (683.7040) | 0.6765 (8039.7430) |
| MixedDirectionRuns | 1.0254 (9.7180) | 0.3949 (36.9855) | 0.7791 (726.1255) | 0.8258 (7440.4760) |
| BlockSorted | 0.9099 (7.1840) | 0.1310 (9.1470) | 0.2816 (214.7035) | 0.4016 (3144.9155) |
| OrganPipe | 0.3989 (3.6560) | 0.1011 (14.6270) | 0.1173 (282.2065) | 0.1185 (3719.3470) |
| Rotated | 0.3855 (2.5120) | **0.0679** (3.5870) | 0.1081 (42.3790) | 0.1363 (510.7580) |
| MixedPhase3 | 2.2245 (24.9490) | 1.2924 (238.5300) | 1.2000 (2648.2210) | 1.2593 (33386.7475) |
| MixedPhase12 | 2.1706 (22.9195) | 0.6190 (91.2225) | 0.6207 (1059.5535) | 0.6305 (11814.3705) |

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

- ordered and reverse-disjoint boundary fast paths;
- bidirectional branchless two-run merging for high-entropy cases, exposing independent front/back dependency chains;
- a merge-only fast path for trivially copyable values through 96 bytes;
- general/galloping fallback paths where branchless merging is not preferred;
- pile-density and run-structure routing;
- adjacent-pair merge scheduling;
- variation-specific merge/overflow policies, including deferred-band handling and live-sorted bands.

The exact routing differs by variation because their insertion and run geometries differ. `docs/experiment_log.txt` is the authoritative record of retained/rejected policies and propagation tests.

## Variations

Many variations of Jessesort are maintained in this repo due to ongoing research. Historically, V-names (V1, V2, etc) were used, but now source files use more descriptive identifying tags:

`jessesort_<decomposition>_<router>_<reconstruction-and-merge>[_<other-policy>...]`

Each maintained variation preserves a specific mechanism even when that mechanism is slower on some workloads; the purpose of the family is to keep those architectural tradeoffs measurable rather than allowing every variation to converge silently onto the same fastest path.

- **physical-pile:** is the baseline Jessesort where Patience piles are represented literally rather than simulated.
- **simulated:** replaces physical piles with value-tail metadata plus a packed per-element blueprint, then reconstructs contiguous runs in auxiliary storage.
- **in-place flattening:** replaces streamed flattening with permutation-cycle flattening; exposes the memory/locality tradeoff of in-place reconstruction.
- **single-overflow freezing:** freezes ordinary pile creation early and sends the remainder to one large deferred overflow run to be sorted later.
- **deferred-band freezing:** keeps the same early-freeze idea but breaks overflow into small deferred 32-element bands that are sorted during reconstruction.
- **live/adaptive overflow:** keeps early freezing but processes overflow with a shape-routed live policy: fixed 32-element bands for weak structure, ascending natural runs for clearly ascending low-pile structure, or bidirectional natural runs for stronger directional structure.
- **SIMD/capped-probe:** caps the initial probe to a small pile count so exactly 8 or 16 `int32` tail searches can use AVX2; probe overflow is temporary, distinguishing it from any general early-freeze variation.
- **linked reconstruction:** keeps Patience reconstruction genuinely linked. E201 routes between compact span links and in-place element links based on blueprint locality, with first-merge fusion; it does not simply fall back to ordinary tag reconstruction when linked layouts are inconvenient.
- **index-tail genericity:** stores source indices in pile-tail metadata instead of copies of `T`. That indirection enables move-only/non-copyable values and makes this the genericity/Rust-oriented allocating branch; explored copyable versions reuse optimized downstream reconstruction after the index-tail blueprint is complete.
- **allocation-free bounded baseline:** removes the global blueprint and `T[n]` scratch from the Jessesort path, uses fixed 63/64 index-tail arrays and <=64 run descriptors, accepts only cheap bounded in-place run geometry, and otherwise falls back to an allocation-free partition/heapsort backend. Its defining result is the no-heap resource model.
- **allocation-free shared-tail + live run reclamation:** keeps the former's no-heap/move-only guarantees, changes the two physical tail arrays into one fixed 127-index pool (still logically 63/64), adds geometry-gated general in-place merging, and extends the fixed 64-run descriptor stack by selectively merging the smallest adjacent completed run pair when descriptor capacity is reached. A first-65 long-run/overlap gate keeps short high-entropy and Sawtooth-like repeated-overlap layouts on fallback.
- **long-natural-run pre-router:** introduced a shortcut in E225, similar to the monotone prechecks, that offers a potentially faster path for large comparator-defined structured inputs. This route still depends on structure discovered via the monotone precheck and/or during a dual Patience probe. It requires a nondecreasing first 64 values, a coarse sampled inversion, and a mutation-free proof that the input contains at most 128 natural runs. Accepted inputs normalize descending runs and go directly to a buffered merge; rejected/high-run-count inputs enter the unchanged underlying pipeline. E225 uses no numeric range/cardinality assumptions and was separately validated with an adversarial long-prefix/random-suffix input plus a non-default-constructible record and custom comparator.

## Build and benchmark

Build with GCC/libstdc++:

```bash
make
make run
```

Routine `make run` benchmarks six representatives: `physical`, `simulated`, `frozen-single`, `indexed`, `noalloc-low-run`, and `simulated-direct`. All maintained variations remain selectable by their short descriptors; `all` runs all 20.

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
make run-clang
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
