# JesseSort

JesseSort is a sorting algorithm that uses 2 Patience Sort insertions (one with descending stacks and one with ascending stacks), followed by a merge phase.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n          No          Yes
```

## Speed Test

JesseSort beats Python's default sorted() on random-value inputs for n > 22000 (even better than our preprint claim!):

![Speed Test](images/speedtest_updated.png)

It also beats std::sort() on various inputs with sorted subsequences. Values below are ratios of JesseSort / std::sort, so a value of 0.5 means JesseSort takes half as much time (2x faster), while a value of 2.0 means JesseSort takes twice as much time (2x slower). 20 trials were run for each cell below and averaged.

```
                      Number of Input Values
Input Type            1000          10000         100000        1000000       10000000
--------------------------------------------------------------------------------------------
Random                2.083         1.795         1.755         1.824         1.853
Sorted                2.074         1.479         1.340         1.132         1.287
Reverse               2.083         1.417         1.300         1.222         1.410
NearlySorted(5%)      2.714         2.139         2.101         2.104         2.605
Duplicates(5%)        1.959         1.924         1.931         1.885         1.944
Alternating           10.067        5.468         3.907         5.365         5.741
Sawtooth              2.111         0.662         0.568         0.515         0.453
BlockSorted           2.016         1.567         1.726         1.723         1.776
OrganPipe             0.558         0.169         0.122         0.119         0.112
Rotated               0.626         0.533         0.426         0.341         0.422
```

## Setup

To compile and test JesseSort, run:

```
cd src/jessesort
g++ -O3 -march=native -DNDEBUG -std=c++17 main.cpp jesseSort.cpp -o jesseSortTest
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

The full breakdown of the algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. We now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before. Merge phase optimization is the last missing piece. Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
