# JesseSort

JesseSort is a sorting algorithm that uses 2 Patience Sort insertions (one with descending stacks and one with ascending stacks), followed by a merge phase.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n          No          Yes
```

## Speed Test

JesseSort beats std::sort() on various inputs with sorted subsequences. Values below are ratios of JesseSort / std::sort. A value of 0.5 means JesseSort takes half as much time (2x faster), while a value of 2.0 means JesseSort takes twice as much time (2x slower). 20 trials were run for each cell below and averaged.

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000       10000000
--------------------------------------------------------------------------------------------
Random                1.587         1.477         1.433         1.484         1.596
Sorted                1.554         1.117         1.031         0.897         1.063
Reverse               2.260         1.602         1.416         1.352         1.414
Sorted+Noise(5%)      1.868         1.278         1.357         1.368         1.546
Random+Repeats(50%)   1.443         1.144         1.097         1.085         1.177
Jitter                1.492         1.133         0.980         0.901         1.173
Alternating           2.809         1.066         0.748         0.796         1.062
Sawtooth              1.576         0.439         0.361         0.372         0.376
BlockSorted           0.830         0.355         0.250         0.276         0.285
OrganPipe             0.288         0.164         0.112         0.107         0.106
Rotated               0.498         0.455         0.350         0.277         0.378
Signal                15.093        0.792         0.615         0.619         0.652
```

On a laptop with limited RAM and a different CPU (AMD Ryzen 7 7445HS), JesseSort appears faster on some inputs. The limited memory also leads to a few large time jumps from 100k to 1mil inputs:

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000
-------------------------------------------------------------------------------
Random                1.834         1.442         1.522         1.663
Sorted                1.031         0.672         0.582         1.382?
Reverse               1.452         0.933         0.879         2.047?
Sorted+Noise(5%)      1.947         1.554         1.480         1.470
Random+Repeats(50%)   1.468         1.218         1.171         1.284
Jitter                1.010         0.620         0.605         0.544
Alternating           2.458         1.566         0.958         1.574?
Sawtooth              1.702         0.411         0.384         0.356
BlockSorted           0.818         0.487         0.365         0.326
OrganPipe             0.429         0.228         0.140         0.127
Rotated               0.630         0.485         0.354         0.298
Signal                1.498         0.825         0.624         0.578
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

For use in Python, run:

```
pip install cython
g++ -O3 -shared -std=c++17 -fPIC -o src/jessesort/libjesseSort.so src/jessesort/jesseSort.cpp
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

Merge all stacks until 1 remains. This currently utilizes either a Timsort-inspired "best 2 out of 3" policy or the basic "increasing powers of 2" merge strategy (see the comments in the code). Testing is underway, so a faster merge policy (Huffman?) may replace this.

## Preprint

A breakdown of the original algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. At the time of writing, I had not heard of Patience Sort--a lot has changed since then. We now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before. Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
