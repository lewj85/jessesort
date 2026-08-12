# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience insertion**. Patience is a card game similar to Solitaire, but the tails of each pile are also kept in sorted order. Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs--by routing inputs to two simultaneous games based on current run order: one game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles _k_ typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push _k_ lower towards `O(n)` linear work. Maintained variations are deterministic and unstable; the simulated implementations use `O(n)` auxiliary storage.

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n + k      No          Yes
```

## Speed tests

Jessesort can be nearly 2x faster than std::sort() on random inputs, up to 10x faster on various structured inputs, and almost 20x faster on monotone. Benchmarks below were done with a virtualized Linux container and 5 allocated CPU cores from an AMD EPYC 9V74 80-Core Processor, compiled with GCC 14.2.0 (libstdc++).

Values below are median (not mean) ratios of `Jessesort / std::sort` over 500 trials (50 trials for 1M elements). The values in parentheses are microseconds (μs). One deterministic unique seed per trial, paired across all seven variations of the algorithm. Descriptions of the variations are below the timing tables.

A value of 0.5 below means Jessesort takes half as much time (2x faster), while a value of 2.0 means Jessesort takes twice as much time (2x slower). **Values less than 1 mean Jessesort is faster.**

### V1

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.9307 | 0.8739 | 0.7224 | 0.7079 |
| Sorted | 0.1609 | 0.1021 | 0.0818 | 0.0691 |
| Reverse | 0.2381 | 0.1510 | 0.1220 | 0.1100 |
| Sorted+Noise(5%) | 1.3967 | 0.8960 | 0.8911 | 0.8798 |
| Random%100 | 2.0522 | 1.0742 | 0.7022 | 0.6624 |
| Alternating | 1.3258 | 0.8019 | 0.4905 | 0.4164 |
| Sawtooth | 2.5820 | 0.6958 | 0.6667 | 0.6802 |
| BlockSorted | 1.0741 | 0.5728 | 0.4825 | 0.4542 |
| OrganPipe | 0.2697 | 0.1585 | 0.1281 | 0.1290 |
| Rotated | 0.3327 | 0.2736 | 0.3055 | 0.2673 |

### V2

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.1107 | 0.6556 | 0.5913 | 0.6096 |
| Sorted | 0.1247 | 0.0771 | 0.0622 | 0.0537 |
| Reverse | 0.3360 | 0.2190 | 0.1789 | 0.1619 |
| Sorted+Noise(5%) | 0.8920 | 0.6195 | 0.6477 | 0.6566 |
| Random%100 | 1.2118 | 0.7828 | 0.5769 | 0.6151 |
| Alternating | 0.7110 | 0.3585 | 0.2798 | 0.2216 |
| Sawtooth | 2.0364 | 0.4616 | 0.4499 | 0.4080 |
| BlockSorted | 0.5885 | 0.2366 | 0.2411 | 0.1892 |
| OrganPipe | 0.2397 | 0.1134 | 0.1100 | 0.0928 |
| Rotated | 0.2495 | 0.1696 | 0.2060 | 0.1569 |

### V3

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.5362 | 0.8090 | 0.7700 | 0.9019 |
| Sorted | 0.1315 | 0.0774 | 0.0622 | 0.0522 |
| Reverse | 0.3443 | 0.2192 | 0.1790 | 0.1598 |
| Sorted+Noise(5%) | 1.1117 | 0.9164 | 1.0177 | 1.4735 |
| Random%100 | 1.6318 | 1.0729 | 0.9196 | 1.2791 |
| Alternating | 1.1237 | 0.7380 | 0.6215 | 1.0131 |
| Sawtooth | 2.4498 | 0.6812 | 0.6462 | 0.5935 |
| BlockSorted | 0.8170 | 0.4710 | 0.4011 | 0.4045 |
| OrganPipe | 0.3444 | 0.1674 | 0.1162 | 0.1041 |
| Rotated | 0.4038 | 0.2886 | 0.2723 | 0.1948 |

### V4

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.5456 | 0.8429 | 0.7955 | 0.7711 |
| Sorted | 0.1261 | 0.0779 | 0.0620 | 0.0521 |
| Reverse | 0.3394 | 0.2196 | 0.1790 | 0.1616 |
| Sorted+Noise(5%) | 1.1505 | 0.8354 | 0.8462 | 0.8130 |
| Random%100 | 1.6788 | 0.9944 | 0.7562 | 0.7047 |
| Alternating | 1.2051 | 0.5960 | 0.4107 | 0.3425 |
| Sawtooth | 2.4924 | 0.8143 | 0.7639 | 0.7135 |
| BlockSorted | 0.9265 | 0.4848 | 0.4410 | 0.3472 |
| OrganPipe | 0.2282 | 0.1107 | 0.1060 | 0.0927 |
| Rotated | 0.3236 | 0.2285 | 0.2564 | 0.2181 |

### V5

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.3292 | 0.7868 | 0.7402 | 0.7191 |
| Sorted | 0.1251 | 0.0780 | 0.0620 | 0.0532 |
| Reverse | 0.3332 | 0.2197 | 0.1790 | 0.1595 |
| Sorted+Noise(5%) | 1.1377 | 0.8172 | 0.8171 | 0.7955 |
| Random%100 | 1.4812 | 0.9722 | 0.8062 | 0.7759 |
| Alternating | 1.2577 | 0.6367 | 0.4276 | 0.3682 |
| Sawtooth | 2.6686 | 0.7454 | 0.6978 | 0.6607 |
| BlockSorted | 0.9659 | 0.5050 | 0.4401 | 0.3730 |
| OrganPipe | 0.2452 | 0.1187 | 0.1096 | 0.1004 |
| Rotated | 0.7234 | 0.5879 | 0.5364 | 0.3778 |

### V6

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4411 | 0.7862 | 0.7415 | 0.7424 |
| Sorted | 0.1376 | 0.0789 | 0.0622 | 0.0517 |
| Reverse | 0.3493 | 0.2216 | 0.1791 | 0.1602 |
| Sorted+Noise(5%) | 1.2543 | 0.8505 | 0.8272 | 0.7788 |
| Random%100 | 1.5690 | 0.9972 | 0.8112 | 0.7819 |
| Alternating | 1.2751 | 0.6365 | 0.4351 | 0.3715 |
| Sawtooth | 2.4434 | 0.6171 | 0.6019 | 0.5705 |
| BlockSorted | 0.8693 | 0.4407 | 0.3958 | 0.3312 |
| OrganPipe | 0.2461 | 0.1192 | 0.1091 | 0.1006 |
| Rotated | 0.5888 | 0.4694 | 0.4443 | 0.3078 |

### V7

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.6363 | 0.6760 | 0.6047 | 0.6107 |
| Sorted | 0.1328 | 0.0780 | 0.0623 | 0.0520 |
| Reverse | 0.2564 | 0.1529 | 0.1223 | 0.1102 |
| Sorted+Noise(5%) | 1.1077 | 0.6944 | 0.7152 | 0.7068 |
| Random%100 | 1.7149 | 0.8670 | 0.5837 | 0.5674 |
| Alternating | 0.8080 | 0.3347 | 0.2525 | 0.2279 |
| Sawtooth | 2.2113 | 0.4890 | 0.4730 | 0.4596 |
| BlockSorted | 0.7149 | 0.2975 | 0.2765 | 0.2508 |
| OrganPipe | 0.2611 | 0.1203 | 0.1128 | 0.1092 |
| Rotated | 0.3059 | 0.1911 | 0.2336 | 0.1926 |

## Algorithm overview

Jessesort has two broad phases, with several retained adaptive paths layered around them.

### 1. Dual-Patience insertion

Values are routed between an ascending and a descending Patience game. The maintained implementations use early structure probes, specialized hinted/no-hint search continuations, monotone early exits, and gated natural-run batching. The simulated variations record compact pile/run tags rather than physically constructing every pile; V1 keeps actual piles.

V4-V6 additionally use a **50% nominal freeze decision point**. Depending on the retained power-of-two extension policy, actual pile creation can continue beyond 50% before the structure is frozen. Later values are handled through each variation's overflow design.

### 2. Reconstruction and adaptive merging

The merge phase selects a merge strategy based on the shape of the piles created by the insertion phase. Current code includes:

- ordered and reverse-disjoint boundary fast paths;
- **bidirectional branchless two-run merging** for high-entropy cases, exposing independent front/back dependency chains;
- a merge-only fast path for trivially copyable values through **96 bytes**;
- general/galloping fallback paths where branchless merging is not preferred;
- pile-density and run-structure routing;
- adjacent-pair merge scheduling;
- variation-specific merge/overflow policies, including V5's deferred-band handling and V6's live-sorted bands.

The exact routing differs by variation because their insertion and run geometries differ. `experiment_log.txt` is the authoritative record of retained/rejected policies and propagation tests.

## Variations

The numbering is structural and intentional: **V1-V3 do not freeze pile creation; V4-V6 are the early-freeze/overflow family; V7 is a V2-derived SIMD research path.**

| Variation | Core representation | Freezes pile creation? | Overflow behavior | Flatten / reconstruction distinction |
|---|---|---|---|---|
| **V1 — Actual Piles** | Physical Patience piles | **No** | None | Physical piles are flattened into runs |
| **V2 — Simulated Blueprint** | Pile tails + blueprint tags | **No** | None | Blueprint is reconstructed into contiguous runs in auxiliary storage |
| **V3 — In-Place Simulated Flattening** | V2-style simulated blueprint | **No** | None | Blueprint destinations are resolved by an in-place permutation/cycle flattening step |
| **V4 — Single Overflow** | Early-frozen simulated blueprint | **Yes** | One large deferred overflow run | Frozen normal runs + one sorted deferred overflow run |
| **V5 — Deferred Bands** | Early-frozen simulated blueprint | **Yes** | Deferred 32-element overflow bands | Overflow bands are gathered first, then sorted during reconstruction |
| **V6 — Live Bands** | Early-frozen simulated blueprint | **Yes** | Live-sorted 32-element overflow bands | During reconstruction, overflow values are insertion-sorted into their current band |
| **V7 — AVX2 Experimental** | V2-derived simulated blueprint + capped SIMD probe | **No general freeze** | Temporary capped-probe spill only | Normalized V2-style reconstruction with SIMD-oriented probe handling |

## Build and benchmark

Build with GCC/libstdc++:

```bash
make
```

Run the canonical benchmark (500 trials at 1k/10k/100k, 50 at 1m):

```bash
make run
```

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

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. Timing ratios are currently suboptimal with the latter (though many inputs are still faster with Jessesort). We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless binary search). Easy enough to schedule a fallback when random input is detected, so a hybrid may be preferable when using clang. We show one table of clang timings below for reference.

## V2

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 2.2822 (41.1750) | 1.3058 (371.8410) | 1.2455 (4515.6210) | 1.2265 (53218.3945) |
| Sorted | 0.7249 (0.7860) | 0.6618 (4.3670) | 0.6611 (42.0570) | 0.6465 (422.8945) |
| Reverse | 0.7396 (0.8270) | 0.7547 (10.4550) | 0.7680 (68.9920) | 0.7541 (663.8980) |
| Sorted+Noise(5%) | 1.4136 (15.2330) | 0.9328 (99.3600) | 1.0586 (1366.0920) | 1.1423 (16880.1055) |
| Random%100 | 2.5245 (37.7915) | 2.1482 (339.4580) | 2.0038 (3146.0140) | 2.1411 (32202.8865) |
| Alternating | 1.1942 (6.1010) | 0.7985 (36.9530) | 0.7073 (695.6015) | 0.7106 (8024.2090) |
| Sawtooth | 2.7783 (15.5760) | 0.8889 (85.2840) | 0.9354 (1203.8910) | 0.9342 (13548.8650) |
| BlockSorted | 1.0267 (8.5450) | 0.6233 (38.5950) | 1.2668 (692.6525) | 1.5102 (8740.7640) |
| OrganPipe | 0.3505 (3.4630) | 0.1482 (21.1300) | 0.2500 (603.4830) | 0.2338 (7363.3105) |
| Rotated | 0.3694 (2.6270) | 0.2236 (11.6075) | 0.5292 (346.1025) | 0.9940 (5486.1010) |

## Basic use

The algorithms are template-based and implemented in their headers. For example, V2 can be used as:

```cpp
#include <jessesort/v2_simulated.h>

#include <vector>

int main() {
    std::vector<int> values{7, 2, 9, 1, 5};
    jessesort::simulated::sort(values);
}
```

Comparators must provide a strict weak ordering. Move-only values are not currently supported because maintained pile-tail mirrors are copied. Inputs containing NaNs require an explicit total-order comparator if a deterministic total ordering is desired.

## Development status

Jessesort is still experimental. The compact current roadmap and the detailed retained/rejected experiment history are maintained in [`experiment_log.txt`](experiment_log.txt).

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ substantially. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
