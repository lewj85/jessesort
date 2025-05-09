# JesseSort

JesseSort is a hybrid sorting algorithm that uses 2 Patience Sort insertions (one with descending stacks and one with ascending stacks), followed by a merge phase.

The runtime of this sorting algorithm is dependent on the number of piles/bands, k, created by the 2 games of Patience. On purely random inputs, k = sqrt(n), leading to a total runtime of O(n log n) after merging. But on inputs with natural runs, repeated values, broken subsequences, etc (all of which are common in real data), k can get significantly smaller, allowing JesseSort to approach as fast as O(n).

```
Best    Average     Worst       Memory      Stable      Deterministic
n       n log k     n log n     2n          No          Yes
```

## Speed Test

JesseSort beats Python's default sorted() on random-value inputs for n > 22000 (even better than our preprint claim!):

![Speed Test](images/speedtest_updated.png)

It also beats std::sort() on various inputs with sorted subsequences:

```
// Sawtooth input
JesseSort: 0.165182 seconds
std::sort: 0.399023 seconds

// Ascending values with 1% noise (repeated values)
JesseSort: 0.105145 seconds
std::sort: 0.505686 seconds

// Pyramid values (half ascending, half descending)
JesseSort: 0.126481 seconds
std::sort: 0.856716 seconds
```

## Setup

To compile JesseSort, run:

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

Merge all stacks until 1 remains. This currently utilizes either Timsort's "best 2 out of 3" policy or the basic "increasing powers of 2" merge strategy (see the comments in the code). Testing is underway, so a faster merge policy (Huffman?) may replace this.

## Preprint

The full breakdown of the algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

This is under active development, so the preprint and code here differ. We now use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.

## Final Thoughts

My PhD has pulled me away from this project for a bit. I've actually come up with a couple other sorting algorithms that will have to wait as well! I will continue to update this in my free time whenever possible, but progress will be slower than before. Merge phase optimization is the last missing piece. Finally, I want to thank you all for your support and feedback--a special thank you to Sebastian Wild, Kenan Millet, and my beloved wife. Sorting is not my area of expertise, so I appreciate all your *patience*. 😉
