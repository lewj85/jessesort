# JesseSort

JesseSort is a new sorting algorithm that introduces dual Patience--one game with descending piles and one game with ascending piles--as a core insertion strategy. Variations and optimizations are actively being explored, including experiments in adaptivity to input type, possible fallback strategies, optimal merge methods, game simulation, improved branch prediction, AVX2/AVX512 usage, etc. Even in its current unoptimized state, JesseSort is one of the fastest sorting algorithms to date.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n + k      No          Yes
```

## Speed Test

JesseSort beats std::sort() on various semi-sorted inputs. Values below are median (not mean) ratios of JesseSort / std::sort over 100 trials. A value of 0.5 means JesseSort takes half as much time (2x faster), while a value of 2.0 means JesseSort takes twice as much time (2x slower). **Values <1 mean JesseSort is faster.**

With a AMD Ryzen 7 7445HS CPU, compiled with GCC (libstdc++):

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000
------------------------------------------------------------------------------
Random                1.263         1.203         1.150         1.104
Sorted                0.629         0.415         0.355         0.646
Reverse               0.953         0.660         0.557         0.988
Sorted+Noise(3%)      0.743         0.641         0.671         1.028
Random%100            1.237         1.052         0.813         0.986
Jitter                0.632         0.416         0.630         1.338
Alternating           0.786         0.482         0.209         0.500
Sawtooth              1.757         0.672         0.280         0.533
BlockSorted           0.623         0.363         0.277         0.495
OrganPipe             0.360         0.179         0.108         0.250
Rotated               0.438         0.392         0.272         0.553
Signal                1.251         0.800         0.628         0.548
```

JesseSort also beats Python's default sorted() on random-value inputs for n > 20000 (>10x faster than our preprint claim):  
NOTE: This chart was made with outdated code, JesseSort is faster now.

![Speed Test](images/speedtest_updated.png)

## Setup

To compile and test JesseSort, run:

```
cd src/jessesort
g++ -O3 -march=native -DNDEBUG -std=c++23 main.cpp jesseSort.cpp -lfftw3 -lfftw3_threads -lm -o jesseSortTest
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

I welcome any contributions folks want to make to this project.

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before.

Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
