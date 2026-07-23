# JesseSort

[src/viz/simulated/jessesort_simulated_video.mp4](https://github.com/user-attachments/assets/2c3b6781-8f99-4318-b0f7-0450ee4f45a4)

[src/viz/simulated/jessesort_simulated_freezing_video.mp4](https://github.com/user-attachments/assets/a97c5e20-7e8d-4ab8-873f-342bf3a3bc43)

JesseSort is a new sorting algorithm that introduces dual Patience--one game with descending piles and one game with ascending piles--as a core insertion strategy. Variations and optimizations are actively being explored, including experiments in adaptivity to input type, possible fallback strategies, optimal merge methods, game simulation, improved branch prediction, AVX2/AVX512 usage, etc. Even in its current unoptimized state, JesseSort is one of the fastest sorting algorithms to date.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n + k      No          Yes
```

## Speed Test

JesseSort beats std::sort() on various structured inputs. Values below are median (not mean) ratios of `JesseSort / std::sort` over 30 trials (after 3 unmeasured warmups). A value of 0.5 means JesseSort takes half as much time (2x faster), while a value of 2.0 means JesseSort takes twice as much time (2x slower).

**Values less than 1 mean JesseSort is faster.**

With a virtualized Linux container and 5 allocated CPU cores from an AMD EPYC 9V74 80-Core Processor, compiled with GCC 14.2.0 (libstdc++):

```
Simulated JesseSort versus std::sort (JesseSort / std::sort)
Elements              1000            10000           100000          1000000
--------------------------------------------------------------------------------------
Random                1.236           1.097           1.028           1.027
Sorted                0.558           0.381           0.279           0.335
Reverse               0.736           0.577           0.427           0.546
Sorted+Noise(3%)      0.652           0.600           0.608           0.777
Random%100            1.210           1.050           0.831           0.878
Jitter                0.511           0.351           0.637           0.858
Alternating           0.632           0.403           0.169           0.239
Sawtooth              1.521           0.729           0.253           0.325
BlockSorted           0.519           0.312           0.242           0.301
OrganPipe             0.309           0.161           0.097           0.131
Rotated               0.321           0.338           0.270           0.350
```

```
Early-freeze JesseSort versus std::sort
Elements              1000            10000           100000          1000000
--------------------------------------------------------------------------------------
Random                1.293           1.119           1.062           1.060
Sorted                0.619           0.409           0.357           0.405
Reverse               0.939           0.641           0.542           0.650
Sorted+Noise(3%)      0.705           0.660           0.612           0.746
Random%100            1.263           1.089           0.837           0.884
Jitter                0.750           0.450           0.652           0.877
Alternating           0.746           0.485           0.197           0.263
Sawtooth              1.710           0.782           0.288           0.363
BlockSorted           0.540           0.324           0.267           0.347
OrganPipe             0.356           0.182           0.107           0.140
Rotated               0.357           0.397           0.297           0.366
```

```
Actual-pile JesseSort versus std::sort
Elements              1000            10000           100000          1000000
--------------------------------------------------------------------------------------
Random                1.952           1.600           1.495           1.506
Sorted                0.824           0.535           0.434           0.385
Reverse               1.099           0.673           0.558           0.514
Sorted+Noise(3%)      0.995           0.934           0.966           1.342
Random%100            1.927           1.747           1.531           1.542
Jitter                0.821           0.527           0.745           1.214
Alternating           0.899           0.551           0.226           0.352
Sawtooth              1.848           0.917           0.399           0.489
BlockSorted           0.764           0.490           0.341           0.444
OrganPipe             0.400           0.202           0.112           0.168
Rotated               0.499           0.486           0.367           0.565
```

## Algorithm Overview

JesseSort consists of two main phases:

1. Insertion Phase

Play two games of Patience, one with descending piles and one with ascending piles. Send values to the optimal game based on the order of the current natural run in the input array. This avoids a commonly encountered worst case and capitalizes on natural runs to bring runtime closer to O(n).

2. Merge Phase

Merge all piles until one remains. The current code naively merges adjacent runs without additional optimization. Testing is underway, so a faster merge policy may replace this.

### Variants

This repository keeps three JesseSort implementations so their storage strategies can be compared directly. Additional implementations are planned.

| Header / Entry point | Description |
|---|---|
| `include/jessesort/simulated.hpp` / `jessesort::simulated::sort` | Simulates insertion with base arrays and a blueprint, then reconstructs flat runs before merging. |
| `include/jessesort/simulated_early_freeze.hpp` / `jessesort::simulated_early_freeze::sort` | Simulated insertion with base array length freezing @36.8% (1/e) of the input, plus live insertion sorting for 32-element overflow piles. |
| `include/jessesort/actual_piles.hpp` / `jessesort::actual_piles::sort` | Physically creates piles during the insertion phase and uses base array copies for search. |

All variants sort a `std::vector<T>` in place and accept an optional comparator:

```cpp
#include <jessesort/simulated_early_freeze.hpp>

std::vector<int> values{4, 1, 3, 2};
jessesort::simulated_early_freeze::sort(values);
```

## Build and benchmark

The benchmark compares all three variants with `std::sort` over 30 trials and different numbers of elements.

```bash
make
make run-all
```

### Clang with libc++

Install libc++ and libc++abi development packages, then run:

```bash
make clean
make clang-libc++
make run-all
```

Note that the code here was developed primarily with GCC and libstdc++, not clang and libc++. Timing ratios are currently suboptimal with the latter (some inputs are still faster). We provide make targets for clang support, but no testing has been done to actually ensure the code is compiling as expected (e.g., branchless binary search).

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
