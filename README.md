# JesseSort

JesseSort is a new sorting algorithm that introduces dual Patience--one game with descending piles and one game with ascending piles--as a core insertion strategy. The exact implementation is still evolving, with ongoing exploration of adaptivity to input type, possible fallback strategies, optimal merge methods, game simulation, improved branch prediction, etc. Even in its current unoptimized state, JesseSort is one of the fastest sorting algorithms to date.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n          No          Yes
```

## Speed Test

JesseSort beats std::sort() on various semi-sorted inputs. Values below are median (not mean) ratios of JesseSort / std::sort over 100 trials. A value of 0.5 means JesseSort takes half as much time (2x faster), while a value of 2.0 means JesseSort takes twice as much time (2x slower). **Values below 1 mean JesseSort is faster.**

With a AMD Ryzen 7 7445HS CPU, compiled with GCC (libstdc++):

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000
------------------------------------------------------------------------------
Random                1.018         1.033         1.022         1.073
Sorted                1.069         0.699         0.590         1.484
Reverse               1.645         1.099         0.900         2.199
Sorted+Noise(3%)      1.049         1.036         1.048         1.201
Random%100            1.033         1.041         1.040         1.141
Jitter                1.072         0.699         0.600         1.489
Alternating           0.834         1.023         0.843         0.973
Sawtooth              1.083         0.954         0.975         1.074
BlockSorted           1.020         0.937         0.934         1.164
OrganPipe             0.453         0.240         0.138         0.270
Rotated               0.529         0.488         0.353         0.709
Signal                1.424         0.831         0.642         0.573
```

Same CPU, compiled with clang (libc++):

                      Number of Input Values
Input Type            1000          10000         100000        1000000
------------------------------------------------------------------------------
Random                1.080         1.078         1.058         1.141
Sorted                1.064         0.595         0.550         1.487
Reverse               1.223         0.894         0.736         1.998
Sorted+Noise(3%)      1.113         1.095         1.133         1.261
Random%100            1.080         1.095         1.120         1.251
Jitter                1.061         0.697         0.555         1.501
Alternating           1.256         1.086         0.921         1.254
Sawtooth              1.523         1.067         1.075         1.160
BlockSorted           1.216         0.992         0.987         1.189
OrganPipe             0.590         0.162         0.117         0.225
Rotated               0.577         0.539         0.420         0.739
Signal                1.289         0.766         0.628         0.566

With an Intel Core i9 13900K CPU:

Note: These ratios are using outdated code. Keeping them to demonstrate how the new input adaptive logic greatly sped up the Random input case but at the cost of faster structured inputs.

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000       10000000
--------------------------------------------------------------------------------------------
Random                1.587         1.477         1.433         1.484         1.596
...
Alternating           2.809         1.066         0.748         0.796         1.062
Sawtooth              1.576         0.439         0.361         0.372         0.376
BlockSorted           0.830         0.355         0.250         0.276         0.285
OrganPipe             0.288         0.164         0.112         0.107         0.106
Rotated               0.498         0.455         0.350         0.277         0.378
```

JesseSort also beats Python's default sorted() on random-value inputs for n > 20000 (>10x faster than our preprint claim):

![Speed Test](images/speedtest_updated.png)

## Setup

To compile and test JesseSort, run:

```
cd src/jessesort
g++ -O3 -march=native -DNDEBUG -std=c++23 main.cpp jesseSort.cpp -lfftw3 -lfftw3_threads -lm -o jesseSortTest
./jesseSortTest
```

Or if you want to use clang and libc++, run:

```
cd src/jessesort
clang++ -O3 -march=native -DNDEBUG main.cpp jesseSort.cpp -lfftw3 -lfftw3_threads -lm -o jesseSortTest
./jesseSortTest
```

For use in Python, run:

```
pip install cython
g++ -O3 -shared -std=c++23 -fPIC -o src/jessesort/libjesseSort.so src/jessesort/jesseSort.cpp
python setup.py build_ext --inplace
```

Then you can import it in a Python file:

```
from jessesort import jessesort
import random
unsorted_array = [random.randint(1,1000) for _ in range(1000)]
sorted_array = jessesort(unsorted_array)
```

## Algorithm Overview

JesseSort consists of two main phases:

1. Insertion Phase

Play 2 games of Patience, one with descending stacks and one with ascending stacks. Send values to the optimal game based on the order of the current natural run in the input array. This avoids a commonly encountered worst case and capitalizes on natural runs to bring runtime closer to O(n).

2. Merge Phase

Merge all stacks until 1 remains. This currently utilizes either a Timsort-inspired "best 2 out of 3" policy or naive adjacent runs. Testing is underway, so a faster merge policy may replace this.

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. I now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before. I welcome any contributions folks want to make to this project.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
