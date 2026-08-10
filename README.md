# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

Jessesort is a new family of sorting algorithms that introduces dual Patience--one game with descending piles and one game with ascending piles--as a core insertion strategy. Variations and optimizations are actively being explored, including experiments in adaptivity to input type, possible fallback strategies, optimal merge methods, game simulation, improved branch prediction, AVX2/AVX512 usage, etc. Even in its current unoptimized state, Jessesort is one of the fastest sorting algorithms to date.

Jessesort runtime is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing Jessesort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n + k      No          Yes
```

## Speed Tests

Jessesort is up to 95% faster than std::sort() on various structured inputs, and up to 20% faster on random inputs. Benchmarks below were done with a virtualized Linux container and 5 allocated CPU cores from an AMD EPYC 9V74 80-Core Processor, compiled with GCC 14.2.0 (libstdc++).

Values below are median (not mean) ratios of `Jessesort / std::sort` over 500 trials (50 trials for 1M elements). The values in parentheses are microseconds (μs). One deterministic unique seed per trial, paired across all seven variations of the algorithm. Descriptions of the variations are below the timing tables.

A value of 0.5 below means Jessesort takes half as much time (2x faster), while a value of 2.0 means Jessesort takes twice as much time (2x slower). **Values less than 1 mean Jessesort is faster.**

### V1 — Actual Piles

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 2.0670 (55.9530) | 1.0938 (429.7930) | 0.9213 (4455.4730) | 0.9070 (52534.0745) |
| Sorted | 0.1617 (0.6000) | 0.1036 (5.4880) | 0.0822 (54.2410) | 0.0696 (542.4310) |
| Reverse | 0.2473 (0.6710) | 0.1507 (5.9090) | 0.1228 (58.1765) | 0.1069 (590.4520) |
| Sorted+Noise(5%) | 1.3733 (15.0625) | 0.8890 (132.8175) | 0.8610 (1506.6580) | 0.8945 (17663.0150) |
| Random%100 | 2.1702 (49.4785) | 1.0976 (279.6705) | 0.6828 (1734.1060) | 0.6367 (16220.5225) |
| Alternating | 0.7591 (3.6550) | 0.4721 (39.8190) | 0.3442 (581.0175) | 0.3176 (6568.2405) |
| Sawtooth | 2.2206 (12.3485) | 0.5889 (104.6405) | 0.5729 (1178.7010) | 0.5623 (12882.0590) |
| BlockSorted | 0.8156 (7.8310) | 0.3443 (44.9820) | 0.3506 (625.8495) | 0.3428 (7417.3815) |
| OrganPipe | 0.3347 (3.8505) | 0.1857 (38.9380) | 0.1532 (539.8015) | 0.1537 (6563.2435) |
| Rotated | 0.4562 (3.6960) | 0.4001 (32.7785) | 0.4054 (473.8690) | 0.3195 (5674.1580) |

### V2 — Simulated Blueprint

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4252 (39.5440) | 0.8730 (341.8680) | 0.7990 (3860.2890) | 0.8113 (47102.6720) |
| Sorted | 0.1617 (0.5910) | 0.1040 (5.5080) | 0.0823 (54.3110) | 0.0696 (543.6575) |
| Reverse | 0.2684 (0.7510) | 0.1828 (7.2010) | 0.1237 (60.1095) | 0.1126 (639.0295) |
| Sorted+Noise(5%) | 0.9228 (10.2500) | 0.6720 (101.4460) | 0.7078 (1250.4870) | 0.6923 (14028.1960) |
| Random%100 | 1.4200 (33.8800) | 0.8485 (217.5830) | 0.5834 (1475.5265) | 0.5540 (14049.1170) |
| Alternating | 0.7406 (3.5955) | 0.3662 (30.9060) | 0.2761 (464.2440) | 0.2460 (5166.9295) |
| Sawtooth | 2.0554 (11.4570) | 0.4706 (83.3385) | 0.4501 (928.3485) | 0.4116 (9428.8300) |
| BlockSorted | 0.5925 (5.6880) | 0.2359 (31.0860) | 0.2377 (424.2605) | 0.1906 (4497.2800) |
| OrganPipe | 0.3038 (3.5350) | 0.1495 (31.3960) | 0.1283 (449.4030) | 0.1149 (4840.7550) |
| Rotated | 0.3366 (2.8140) | 0.2714 (22.4580) | 0.2646 (315.4135) | 0.2045 (3671.9990) |

### V3 — In-Place Simulated

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.5087 (40.7960) | 1.0597 (416.3685) | 0.9946 (4799.3530) | 1.1244 (65251.3335) |
| Sorted | 0.1644 (0.6010) | 0.1041 (5.5135) | 0.0825 (54.3210) | 0.0697 (544.6290) |
| Reverse | 0.2782 (0.7610) | 0.1834 (7.2010) | 0.1237 (58.6820) | 0.1098 (617.7475) |
| Sorted+Noise(5%) | 1.0132 (11.2260) | 0.9016 (136.6430) | 1.0732 (1883.9030) | 1.5086 (30053.1430) |
| Random%100 | 1.5537 (35.6425) | 1.0763 (273.6115) | 0.9158 (2326.0050) | 1.2783 (32355.6130) |
| Alternating | 1.1305 (5.4380) | 0.8142 (69.3030) | 0.6299 (1062.0120) | 1.0162 (21017.1870) |
| Sawtooth | 2.3822 (13.2200) | 0.6747 (119.2470) | 0.6254 (1288.6340) | 0.5734 (12969.6690) |
| BlockSorted | 0.7641 (7.2755) | 0.4514 (59.0525) | 0.3806 (673.5605) | 0.3903 (8604.9610) |
| OrganPipe | 0.3536 (4.0960) | 0.1749 (36.7650) | 0.1172 (409.7535) | 0.1046 (4402.8040) |
| Rotated | 0.5227 (4.2410) | 0.4588 (37.8015) | 0.3728 (432.2620) | 0.2466 (4240.9290) |

### V4 — Single Overflow

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.3558 (38.2820) | 1.0003 (393.0035) | 0.9685 (4689.5260) | 0.9276 (54215.6520) |
| Sorted | 0.1302 (0.4810) | 0.0793 (4.2060) | 0.0625 (40.9500) | 0.0523 (408.5870) |
| Reverse | 0.3545 (0.9710) | 0.2205 (8.6420) | 0.1802 (85.4270) | 0.1567 (880.1220) |
| Sorted+Noise(5%) | 0.7357 (8.0770) | 0.6157 (92.6225) | 0.7012 (1234.3085) | 0.6790 (13715.5520) |
| Random%100 | 1.4570 (34.8710) | 0.9267 (237.4975) | 0.6823 (1725.8230) | 0.6418 (16189.1865) |
| Alternating | 0.6375 (3.0740) | 0.3356 (28.1815) | 0.2495 (419.4985) | 0.2096 (4424.4465) |
| Sawtooth | 2.0872 (11.6270) | 0.6800 (120.4340) | 0.6344 (1307.6370) | 0.5948 (13435.2305) |
| BlockSorted | 0.5777 (5.6385) | 0.2929 (35.0470) | 0.2880 (473.4880) | 0.2242 (5069.6450) |
| OrganPipe | 0.3038 (3.5150) | 0.1546 (32.4480) | 0.1299 (456.7130) | 0.1149 (4862.2375) |
| Rotated | 0.3616 (2.9645) | 0.3048 (26.1990) | 0.3063 (368.3970) | 0.2572 (4228.7860) |

### V5 — Deferred Bands

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.2133 (32.9390) | 0.9837 (386.0230) | 0.9073 (4382.9245) | 0.9102 (52574.0585) |
| Sorted | 0.1319 (0.4810) | 0.0791 (4.1860) | 0.0626 (40.8710) | 0.0543 (437.6405) |
| Reverse | 0.3410 (0.9310) | 0.2205 (8.6330) | 0.1801 (85.3170) | 0.1568 (872.0905) |
| Sorted+Noise(5%) | 0.7389 (8.3020) | 0.6203 (94.6005) | 0.6611 (1155.5765) | 0.6645 (13286.3195) |
| Random%100 | 1.3110 (30.2750) | 0.9228 (234.2980) | 0.6676 (1688.7485) | 0.6431 (16299.1295) |
| Alternating | 0.6536 (3.1650) | 0.3396 (28.6775) | 0.2459 (411.3660) | 0.2257 (4700.4015) |
| Sawtooth | 2.0950 (11.6780) | 0.4972 (88.3360) | 0.4626 (953.5420) | 0.4446 (10032.1055) |
| BlockSorted | 0.5535 (5.4935) | 0.2524 (33.6450) | 0.2426 (433.0985) | 0.2041 (4660.6025) |
| OrganPipe | 0.3019 (3.4655) | 0.1542 (32.2380) | 0.1292 (453.2930) | 0.1159 (4895.3660) |
| Rotated | 0.3311 (2.7195) | 0.2864 (23.9600) | 0.2847 (331.7125) | 0.2153 (3687.9480) |

### V6 — Live Bands

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.3230 (39.0030) | 0.9623 (377.5860) | 0.9084 (4394.6225) | 0.9329 (53978.1645) |
| Sorted | 0.1295 (0.4800) | 0.0794 (4.2060) | 0.0623 (40.9310) | 0.0522 (408.5070) |
| Reverse | 0.3456 (0.9420) | 0.2202 (8.6330) | 0.1802 (85.4270) | 0.1565 (871.6595) |
| Sorted+Noise(5%) | 0.9029 (10.3150) | 0.6722 (102.1170) | 0.6687 (1172.3915) | 0.6542 (13094.6845) |
| Random%100 | 1.3620 (34.3610) | 0.9492 (245.2145) | 0.6705 (1698.8330) | 0.6460 (16480.5935) |
| Alternating | 0.6749 (3.2550) | 0.3500 (29.6940) | 0.2556 (432.3320) | 0.2307 (4803.3345) |
| Sawtooth | 2.0617 (11.5070) | 0.4882 (87.1745) | 0.4748 (979.4300) | 0.4560 (10322.6275) |
| BlockSorted | 0.5599 (5.5380) | 0.2550 (33.6400) | 0.2458 (438.5215) | 0.2147 (4763.7405) |
| OrganPipe | 0.3084 (3.5850) | 0.1567 (32.9490) | 0.1281 (448.9515) | 0.1178 (4996.9470) |
| Rotated | 0.3304 (2.7640) | 0.2861 (24.4715) | 0.2892 (328.5330) | 0.2059 (3523.1330) |

### V7 — AVX2 Experimental

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.6494 (44.9520) | 0.9686 (379.3640) | 0.8531 (4110.7960) | 0.8408 (50350.9790) |
| Sorted | 0.1319 (0.4810) | 0.0790 (4.1860) | 0.0619 (40.8100) | 0.0522 (407.5155) |
| Reverse | 0.2445 (0.6610) | 0.1511 (5.9235) | 0.1228 (58.2060) | 0.1073 (598.2840) |
| Sorted+Noise(5%) | 1.0102 (11.0860) | 0.6687 (100.5845) | 0.6941 (1217.5035) | 0.7082 (14080.3085) |
| Random%100 | 1.7347 (39.8390) | 0.9813 (249.5360) | 0.6639 (1677.6920) | 0.6485 (16338.7780) |
| Alternating | 0.6566 (3.1850) | 0.3098 (26.1790) | 0.2265 (383.7795) | 0.2164 (4535.5365) |
| Sawtooth | 2.1259 (11.8380) | 0.5062 (90.1140) | 0.4804 (988.9740) | 0.4610 (10430.3620) |
| BlockSorted | 0.6162 (5.8590) | 0.2896 (38.2765) | 0.2738 (487.8050) | 0.2426 (5339.5160) |
| OrganPipe | 0.3186 (3.6750) | 0.1563 (32.8335) | 0.1315 (464.6305) | 0.1245 (5323.8025) |
| Rotated | 0.3864 (3.1445) | 0.3147 (25.7330) | 0.3075 (363.5505) | 0.2288 (4132.0680) |

## Algorithm Overview

Jessesort consists of two main phases:

1. Insertion Phase

Play two games of Patience, one with descending piles and one with ascending piles. Send values to the optimal game based on the order of the current natural run in the input array. This avoids a commonly encountered worst case and capitalizes on natural runs to bring runtime closer to O(n).

2. Merge Phase

Merge all piles until one remains. The current code naively merges adjacent runs without additional optimization. Testing is underway, so a faster merge policy may replace this.

### Variations

This repository currently contains seven Jessesort implementations, V1–V7, each exploring a different combination of insertion structure, search strategy, reconstruction, and merging behavior. Rather than treating sorting as a single fixed procedure, Jessesort uses the structure discovered during insertion to guide later decisions. An early probe can route inputs toward more appropriate insertion behavior, while a second routing decision after insertion selects a merge strategy based on the structure that was actually produced. Together, the seven variations examine how much performance can be gained by adapting both the search/comparison kernel and the shape of the work created throughout the sort. Additional variations are planned as this design space continues to be explored.

- **V1 — Actual Piles**  
  The direct physical-pile implementation. Values are inserted into real patience piles during the first phase, then those piles are flattened and merged.

- **V2 — Simulated Blueprint**  
  The main simulated implementation. It tracks pile tails and writes compact pile IDs into a blueprint instead of physically building every pile, then reconstructs ascending runs and selects merge behavior from the observed structure. Large-n reconstruction normalizes packed blueprint tags before the scatter pass (E081), and the Random branchless merge kernel uses a retained contiguous-pointer implementation (E082).

- **V3 — In-Place Simulated**  
  Uses V2's simulated insertion, but converts the blueprint into a permutation and flattens the data in place with cycle swaps. It exists primarily to study the memory/reconstruction tradeoff against ordinary V2.

- **V4 — Single Overflow**  
  An early-freeze design. Pile growth is frozen partway through insertion and all later values are deferred into one large overflow region that is sorted once and merged with the frozen runs.

- **V5 — Deferred Bands**  
  Another early-freeze design. Later values are collected into multiple small unsorted overflow bands, which are sorted after insertion. This variation is the main test bed for future **dynamic merge-tree balancing with overflow**.

- **V6 — Live Bands**  
  Uses the same early-freeze family but keeps each overflow band sorted as values arrive, trading extra insertion work for less deferred sorting.

- **V7 — AVX2 Experimental**  
  A V2-derived SIMD research variation. V7 now deliberately caps each game at eight piles during a 128-element initial probe, spilling would-be extra piles into a temporary probe-overflow run that is routed first, then sorted and married into an existing pile. This keeps the validated single-register AVX2 exact-eight lookup active much more regularly on Random-like inputs. E075 measured the 8x128 design at roughly 0.32% slower than uncapped V7, so it is retained as a meaningful SIMD-use research example rather than a speed claim.

## Build and benchmark

The benchmark compares all seven variations with `std::sort` over 500 trials (50 trials for 1M elements) and different numbers of elements.

```bash
make
```

Equivalent command:

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

### V2

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 2.3562 (44.4255) | 1.4668 (417.8645) | 1.4185 (5132.5210) | 1.4134 (61663.0690) |
| Sorted | 0.7198 (0.7120) | 0.6649 (6.4320) | 0.6609 (42.0830) | 0.6575 (420.6660) |
| Reverse | 0.7512 (1.1150) | 0.7519 (7.0490) | 0.7700 (69.0785) | 0.7710 (674.9470) |
| Sorted+Noise(5%) | 1.4699 (12.5380) | 0.9582 (102.8150) | 1.1399 (1465.7945) | 1.1889 (17253.2160) |
| Random%100 | 2.5644 (40.0495) | 2.1296 (343.7630) | 2.0732 (3212.9060) | 2.1650 (32900.5315) |
| Alternating | 1.1951 (6.4930) | 0.8175 (38.1535) | 0.8191 (787.7895) | 0.7439 (8409.0010) |
| Sawtooth | 2.7174 (11.4480) | 0.8821 (88.3695) | 1.0338 (1328.5485) | 0.9551 (13668.1795) |
| BlockSorted | 1.0481 (8.4995) | 0.6481 (40.0170) | 1.5277 (815.1990) | 1.5414 (9104.2055) |
| OrganPipe | 0.5410 (5.9305) | 0.2502 (36.3690) | 0.3496 (844.1045) | 0.2862 (9070.6390) |
| Rotated | 0.6482 (4.1830) | 0.5388 (25.4250) | 1.0965 (582.0465) | 1.3475 (6672.8450) |

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

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
