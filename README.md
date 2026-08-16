# Jessesort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

Jessesort is an experimental family of comparison sorting algorithms built around **dual Patience insertion**. Patience is a card game similar to Solitaire, where the tails of each pile are also kept in sorted order. This additional restriction allows new values to quickly find their correct pile via binary search over just these sorted pile tails.

Jessesort adapts Patience Sort insertion to avoid its very common worst case--natural ascending runs, which generate n 1-element piles--by routing inputs to two simultaneous games based on current run order. One game forms descending piles and asborbs descending runs while the other game forms ascending piles and absorbs ascending runs. Current implementations are adaptive and use the structure discovered during insertion to choose different search, reconstruction, overflow, and merge strategies rather than committing to one fixed pipeline for every input.

For random-order inputs, the number of Patience piles _k_ typically grows on the order of `sqrt(n)`, with insertion and merging costing `O(n log n)`. Inputs with long monotone structure, repeated values, or other exploitable order can produce much smaller effective run sets that push _k_ lower towards `O(n)` linear work. Maintained variations are deterministic and unstable; the simulated implementations use `O(n)` auxiliary storage.

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n + k      No          Yes
```

## Speed tests

Jessesort is up to 20x faster than `std::sort` on various structured inputs. Benchmarks below were done with a virtualized Linux container and 5 allocated CPU cores from an AMD EPYC 9V74 80-Core Processor, compiled with GCC 14.2.0 (libstdc++). Clang (libc++) timings are further down in the README.

Values below are median (not mean) ratios of `Jessesort / std::sort` over 500 trials (50 trials for 1m elements). The values in parentheses are microseconds (μs). One deterministic unique seed per trial, paired across all seven variations of the algorithm. Descriptions of the variations are below the timing tables.

A value of 0.5 below means Jessesort takes half as much time (2x faster), while a value of 2.0 means Jessesort takes twice as much time (2x slower). **Values less than 1 mean Jessesort is faster.**

### V1

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.8806 (65.0315) | 0.2931 (133.4245) | 0.2677 (1484.4295) | 0.2576 (16776.6790) |
| Sorted | 0.0849 (0.3630) | 0.0479 (2.8995) | 0.0393 (28.0870) | 0.0331 (283.0070) |
| Reverse | 0.1365 (0.3980) | 0.0765 (3.2840) | 0.0688 (34.8725) | 0.0681 (391.9370) |
| Sorted+Noise(5%) | 1.2271 (17.0200) | 0.8274 (152.8905) | 0.7805 (1638.2980) | 0.8226 (19634.8200) |
| Sorted+Noise(10%) | 1.4583 (24.6815) | 0.9788 (213.3900) | 0.8995 (2291.0070) | 0.9305 (26777.4105) |
| Random%25 | 1.8755 (41.4710) | 0.0531 (11.5820) | 0.0479 (106.3765) | 0.0491 (1105.4260) |
| Alternating | 1.1920 (5.6030) | 0.3891 (63.5645) | 0.3937 (803.7935) | 0.3283 (8349.9770) |
| Sawtooth | 2.3584 (12.8275) | 0.6095 (130.4285) | 0.7610 (1722.1945) | 0.7748 (18318.8905) |
| MixedDirectionRuns | 0.7023 (9.5015) | 0.3606 (69.7725) | 0.2894 (725.1525) | 0.3609 (10874.2480) |
| BlockSorted | 0.6929 (8.9470) | 0.3355 (67.8865) | 0.2519 (678.0440) | 0.3506 (11174.0050) |
| OrganPipe | 0.3156 (3.8110) | 0.0625 (26.8360) | 0.0777 (474.2105) | 0.0793 (5315.3995) |
| Rotated | 0.2668 (2.4350) | 0.1196 (15.4310) | 0.2393 (393.6795) | 0.1165 (2737.7565) |

### V2

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.1848 (41.0215) | 0.2774 (127.0365) | 0.2637 (1462.2505) | 0.2557 (16693.3100) |
| Sorted | 0.0827 (0.3550) | 0.0479 (2.8970) | 0.0393 (28.1585) | 0.0330 (282.9970) |
| Reverse | 0.1361 (0.3900) | 0.0764 (3.2810) | 0.0687 (34.8125) | 0.0684 (394.1580) |
| Sorted+Noise(5%) | 0.7601 (10.9210) | 0.5719 (104.2110) | 0.5293 (1110.0185) | 0.5735 (13617.2220) |
| Sorted+Noise(10%) | 0.9121 (15.9575) | 0.7041 (154.2065) | 0.6402 (1646.0955) | 0.6329 (18084.4925) |
| Random%25 | 1.0833 (23.7670) | 0.0489 (10.6305) | 0.0489 (108.8730) | 0.0490 (1118.2955) |
| Alternating | 0.7482 (3.5265) | 0.1864 (30.4785) | 0.2400 (484.8035) | 0.2006 (5107.9065) |
| Sawtooth | 1.4136 (7.6955) | 0.3852 (82.0895) | 0.4677 (1055.0785) | 0.4727 (11292.4400) |
| MixedDirectionRuns | 0.3811 (4.9805) | 0.1731 (33.5205) | 0.1357 (328.4205) | 0.2118 (6384.3985) |
| BlockSorted | 0.3331 (4.2010) | 0.1361 (27.8045) | 0.1025 (273.9455) | 0.1837 (5859.9195) |
| OrganPipe | 0.2202 (2.4710) | 0.0470 (20.1010) | 0.0649 (400.5525) | 0.0655 (4417.3355) |
| Rotated | 0.1402 (1.2710) | 0.0695 (9.5905) | 0.1496 (282.0720) | 0.0954 (1978.4555) |

### V3

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4517 (52.5805) | 0.2630 (119.8305) | 0.2625 (1455.6340) | 0.2568 (16681.8180) |
| Sorted | 0.0814 (0.3470) | 0.0480 (2.9010) | 0.0393 (28.1850) | 0.0331 (283.1865) |
| Reverse | 0.1358 (0.3900) | 0.0764 (3.2830) | 0.0689 (34.8340) | 0.0678 (390.2265) |
| Sorted+Noise(5%) | 1.0050 (14.3720) | 0.7567 (141.1110) | 0.8286 (1734.8555) | 1.8041 (42863.2595) |
| Sorted+Noise(10%) | 1.1080 (19.4190) | 0.8426 (185.2360) | 0.8532 (2183.8960) | 1.6400 (46920.2250) |
| Random%25 | 1.4558 (32.4865) | 0.0478 (10.3810) | 0.0490 (108.9075) | 0.0499 (1114.7460) |
| Alternating | 1.2812 (6.0405) | 0.4421 (71.8580) | 0.5216 (1063.2940) | 1.4176 (36021.0620) |
| Sawtooth | 2.2304 (12.1565) | 0.6075 (130.0705) | 0.6819 (1541.5685) | 1.4702 (34741.0095) |
| MixedDirectionRuns | 0.6863 (9.2530) | 0.3828 (74.3130) | 0.4156 (1045.2650) | 1.1446 (34516.6000) |
| BlockSorted | 0.6210 (7.8005) | 0.3330 (67.6870) | 0.3639 (976.2665) | 1.0977 (34993.1910) |
| OrganPipe | 0.4098 (4.7485) | 0.0906 (39.0040) | 0.0764 (460.7855) | 0.0767 (5132.0445) |
| Rotated | 0.2718 (2.6180) | 0.1431 (20.2245) | 0.1744 (299.6505) | 0.1002 (2445.8935) |

### V4

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4444 (52.0095) | 0.2571 (116.6025) | 0.2621 (1453.0670) | 0.2575 (16756.9165) |
| Sorted | 0.0979 (0.4190) | 0.0563 (3.4255) | 0.0458 (32.8650) | 0.0384 (328.4725) |
| Reverse | 0.1594 (0.4590) | 0.0885 (3.8090) | 0.0778 (39.4830) | 0.0762 (437.6645) |
| Sorted+Noise(5%) | 0.9367 (13.4300) | 0.7062 (131.2310) | 0.6766 (1442.4230) | 0.6707 (15848.9360) |
| Sorted+Noise(10%) | 1.0456 (18.4545) | 0.7688 (172.1960) | 0.7466 (1923.6935) | 0.6988 (20276.4950) |
| Random%25 | 1.4356 (32.1700) | 0.0476 (10.2725) | 0.0487 (108.1300) | 0.0491 (1117.2685) |
| Alternating | 1.1780 (5.5220) | 0.3210 (52.0985) | 0.3510 (714.0125) | 0.2920 (7508.2830) |
| Sawtooth | 2.0120 (10.9700) | 0.7859 (168.5020) | 0.6877 (1561.9310) | 0.7026 (16669.4615) |
| MixedDirectionRuns | 0.6343 (8.2545) | 0.4027 (73.1680) | 0.2700 (671.5785) | 0.3057 (9237.8745) |
| BlockSorted | 0.5998 (7.1705) | 0.3418 (65.0700) | 0.2315 (619.9915) | 0.2683 (8594.6500) |
| OrganPipe | 0.3261 (3.8695) | 0.0724 (31.4760) | 0.0737 (445.7030) | 0.0740 (5011.3345) |
| Rotated | 0.2577 (2.4670) | 0.1366 (22.3625) | 0.1862 (331.6360) | 0.0927 (2035.0390) |

### V5

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4017 (48.0870) | 0.2574 (117.6720) | 0.2630 (1456.4040) | 0.2574 (16701.4740) |
| Sorted | 0.0960 (0.4120) | 0.0564 (3.4130) | 0.0458 (32.7810) | 0.0386 (329.0720) |
| Reverse | 0.1621 (0.4660) | 0.0884 (3.7920) | 0.0775 (39.4270) | 0.0760 (436.8715) |
| Sorted+Noise(5%) | 0.9213 (12.8795) | 0.6952 (127.9025) | 0.6478 (1389.2555) | 0.7097 (16703.8000) |
| Sorted+Noise(10%) | 1.0358 (17.3660) | 0.7802 (169.7100) | 0.7173 (1850.7635) | 0.7168 (20643.8075) |
| Random%25 | 1.2405 (27.2160) | 0.0475 (10.2740) | 0.0490 (108.7990) | 0.0494 (1120.2230) |
| Alternating | 1.1919 (5.5770) | 0.3233 (52.6315) | 0.3447 (698.0855) | 0.3037 (7737.1255) |
| Sawtooth | 2.4709 (13.4500) | 0.6690 (142.8150) | 0.7001 (1581.2885) | 0.6954 (16459.0480) |
| MixedDirectionRuns | 0.6382 (8.3140) | 0.4099 (79.8020) | 0.2885 (715.5890) | 0.3417 (10118.2150) |
| BlockSorted | 0.5876 (7.1950) | 0.3491 (70.3600) | 0.2465 (658.2145) | 0.3072 (9722.4610) |
| OrganPipe | 0.3616 (4.0215) | 0.0864 (37.0650) | 0.0823 (499.2955) | 0.0860 (5788.9335) |
| Rotated | 0.3987 (4.6095) | 0.2798 (44.5540) | 0.3296 (543.8475) | 0.1476 (2737.3180) |

### V6

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.4682 (53.2120) | 0.2659 (121.0495) | 0.2633 (1457.4370) | 0.2569 (16706.3075) |
| Sorted | 0.0969 (0.4140) | 0.0564 (3.4240) | 0.0458 (32.8360) | 0.0385 (328.6745) |
| Reverse | 0.1609 (0.4630) | 0.0889 (3.8240) | 0.0781 (39.4935) | 0.0759 (437.2510) |
| Sorted+Noise(5%) | 1.0433 (14.8570) | 0.7293 (134.9215) | 0.6644 (1425.4570) | 0.6702 (15885.7820) |
| Sorted+Noise(10%) | 1.1702 (19.7765) | 0.8143 (177.8700) | 0.7400 (1895.8070) | 0.7037 (20092.5550) |
| Random%25 | 1.4883 (33.5180) | 0.0484 (10.5420) | 0.0487 (108.3875) | 0.0497 (1112.9290) |
| Alternating | 1.3060 (6.1445) | 0.3562 (58.2660) | 0.3806 (775.3885) | 0.3240 (8280.0320) |
| Sawtooth | 1.9770 (10.7350) | 0.5280 (113.1715) | 0.6191 (1396.9070) | 0.6263 (14814.4325) |
| MixedDirectionRuns | 0.6329 (8.1305) | 0.3737 (71.8270) | 0.2941 (724.6160) | 0.3370 (9993.9890) |
| BlockSorted | 0.5855 (7.4885) | 0.3208 (66.3945) | 0.2609 (695.2915) | 0.3048 (9572.8505) |
| OrganPipe | 0.3846 (4.3815) | 0.0894 (39.0930) | 0.0772 (470.6930) | 0.0807 (5441.3355) |
| Rotated | 0.4663 (5.0440) | 0.2988 (45.5550) | 0.3315 (583.7910) | 0.1486 (2593.4310) |

### V7

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 1.5059 (52.0275) | 0.2780 (127.9690) | 0.2641 (1468.5720) | 0.2564 (16716.0405) |
| Sorted | 0.0987 (0.4240) | 0.0561 (3.3910) | 0.0457 (32.7775) | 0.0385 (328.5620) |
| Reverse | 0.1583 (0.4560) | 0.0874 (3.7520) | 0.0776 (39.4290) | 0.0761 (436.8990) |
| Sorted+Noise(5%) | 0.9120 (12.8040) | 0.5883 (107.5740) | 0.5519 (1183.4400) | 0.5458 (12644.6465) |
| Sorted+Noise(10%) | 1.0961 (18.3365) | 0.7349 (162.9455) | 0.7013 (1821.3285) | 0.7421 (21587.4020) |
| Random%25 | 1.4412 (32.2565) | 0.0482 (10.5385) | 0.0487 (108.4440) | 0.0500 (1131.4795) |
| Alternating | 0.7859 (3.6995) | 0.1915 (31.3120) | 0.2430 (498.3520) | 0.2066 (5230.3335) |
| Sawtooth | 1.4577 (7.9120) | 0.3938 (84.1825) | 0.4644 (1050.9190) | 0.4840 (11441.4170) |
| MixedDirectionRuns | 0.5082 (6.5940) | 0.1983 (37.8915) | 0.1510 (369.1385) | 0.2203 (6658.4140) |
| BlockSorted | 0.4910 (5.8715) | 0.1615 (32.0735) | 0.1140 (303.8740) | 0.1797 (5723.8445) |
| OrganPipe | 0.2613 (2.9205) | 0.0539 (23.1250) | 0.0647 (393.2210) | 0.0708 (4733.5685) |
| Rotated | 0.1752 (1.6170) | 0.0875 (12.4940) | 0.1900 (321.0510) | 0.0928 (1903.5840) |

## Algorithm overview

Jessesort has two broad phases, with several retained adaptive paths layered around them.

### 1. Dual-Patience insertion

Values are routed between an ascending and a descending Patience game. The maintained implementations use early structure probes, specialized hinted/no-hint search continuations, monotone early exits, and gated natural-run batching. The simulated variations record compact pile/run tags rather than physically constructing every pile; V1 keeps actual piles. High-entropy inputs are currently routed to ipnsort-inspired partitioning rather than pure Patience.

V4-V6 additionally use a 50% nominal freeze decision point. Depending on balanced merge tree extension policy, actual pile creation can continue beyond 50% before the structure is frozen. Later values are handled through each variation's overflow design.

### 2. Reconstruction and adaptive merging

The merge phase selects a merge strategy based on the shape of the piles created by the insertion phase. Current code includes:

- ordered and reverse-disjoint boundary fast paths;
- bidirectional branchless two-run merging for high-entropy cases, exposing independent front/back dependency chains;
- a merge-only fast path for trivially copyable values through 96 bytes;
- general/galloping fallback paths where branchless merging is not preferred;
- pile-density and run-structure routing;
- adjacent-pair merge scheduling;
- variation-specific merge/overflow policies, including V5's deferred-band handling and V6's live-sorted bands.

The exact routing differs by variation because their insertion and run geometries differ. `experiment_log.txt` is the authoritative record of retained/rejected policies and propagation tests.

## Variations

The numbering is structural and intentional: V1-V3 do not freeze pile creation; V4-V6 are the early-freeze/overflow family; V7 is a V2-derived SIMD research path.

| Variation | Core representation | Freezes pile creation? | Overflow behavior | Flatten / reconstruction distinction |
|---|---|---|---|---|
| **V1 — Actual Piles** | Physical Patience piles | **No** | None | Physical piles are flattened into runs |
| **V2 — Simulated Blueprint** | Pile tails + blueprint tags | **No** | None | Blueprint is reconstructed into contiguous runs in auxiliary storage |
| **V3 — In-Place Simulated Flattening** | V2-style simulated blueprint | **No** | None | Blueprint destinations are resolved by an in-place permutation/cycle flattening step |
| **V4 — Single Overflow** | Early-frozen simulated blueprint | **Yes** | One large deferred overflow run | Frozen normal runs + one sorted deferred overflow run |
| **V5 — Deferred Bands** | Early-frozen simulated blueprint | **Yes** | Deferred 32-element overflow bands | Overflow bands are gathered first, then sorted during reconstruction |
| **V6 — Live Bands** | Early-frozen simulated blueprint | **Yes** | Shape-routed live overflow | During reconstruction, overflow values are sorted as they are encountered; pile shape at freeze point determines method |
| **V7 — AVX2 Experimental** | V2-derived simulated blueprint + capped SIMD probe | **No general freeze** | Temporary capped-probe spill only | Normalized V2-style reconstruction with SIMD-oriented probe handling |

## Build and benchmark

Build with GCC/libstdc++:

```bash
make
```

Run the canonical benchmark (12 permanent input families; 500 trials at 1k/10k/100k, 50 at 1m).
The canonical rows are Random, Sorted, Reverse, Sorted+Noise(5%), Sorted+Noise(10%), Random%25,
Alternating, Sawtooth, MixedDirectionRuns, BlockSorted, OrganPipe, and Rotated. Sawtooth uses an
n^(2/3)-scaled regular ramp period; BlockSorted and MixedDirectionRuns use seeded variable structural
lengths centered on n^(2/3), so both run count and run length grow with input size without the overly
fragmented sqrt(n) regime:

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

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless behavior). We show one table of clang timings below for reference.

## V2

| Input | 1k | 10k | 100k | 1m |
|---|---:|---:|---:|---:|
| Random | 2.2046 (40.4025) | 0.2707 (76.8865) | 0.2893 (1041.2475) | 0.2732 (11856.3930) |
| Sorted | 0.7718 (0.5900) | 0.6676 (6.4960) | 0.6613 (42.3635) | 0.6585 (433.4475) |
| Reverse | 0.7655 (0.8110) | 0.6900 (9.7300) | 0.7035 (63.6210) | 0.7360 (648.0050) |
| Sorted+Noise(5%) | 1.4804 (11.7170) | 0.8404 (89.4920) | 0.9422 (1204.0130) | 0.9379 (13625.0570) |
| Sorted+Noise(10%) | 1.7899 (15.2260) | 0.9954 (131.2250) | 1.0681 (1646.3965) | 1.0444 (18685.9320) |
| Random%25 | 3.0556 (27.3890) | 0.0945 (9.1190) | 0.0785 (85.4125) | 0.0811 (866.3065) |
| Alternating | 1.1769 (6.4820) | 0.8021 (37.9675) | 0.7235 (701.7880) | 0.7139 (8095.0230) |
| Sawtooth | 1.8056 (8.6195) | 0.8243 (77.1255) | 0.9800 (1118.0415) | 1.1400 (13619.2005) |
| MixedDirectionRuns | 0.8487 (7.6680) | 0.5249 (40.7205) | 0.4441 (396.4380) | 1.0083 (9356.5005) |
| BlockSorted | 0.7562 (6.3270) | 0.5331 (36.9160) | 0.4769 (364.7570) | 0.5194 (4080.0710) |
| OrganPipe | 0.3099 (3.3735) | 0.1207 (17.2605) | 0.0691 (166.4340) | 0.0621 (1942.2495) |
| Rotated | 0.3045 (2.0820) | 0.1665 (8.7190) | 0.1560 (81.5455) | 0.2224 (1079.9715) |

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
