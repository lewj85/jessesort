# JesseSort

JesseSort is a hybrid sorting algorithm that uses Patience Sort and Powersort merging under the hood.

# Speed Test

JesseSort beats Python's default sorted() on random-value inputs for n > 15000:

![Speed Test](images/speedtest_updated.png)

It also beats std::sort() on various inputs with sorted subsequences:

```
// Pyramid input (half ascending, half descending)
JesseSort: 0.266038 seconds
std::sort: 2.01727 seconds

// Ascending values with noise (aka nearly sorted)
JesseSort: 0.256696 seconds
std::sort: 0.750383 seconds
```

# Setup

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

# Algorithm Overview

JesseSort consists of two main phases:

1. Insertion Phase

Play 2 games of Patience, one with descending stacks and one with ascending stacks. Send values to the optimal game based on the order of the current natural run in the input array. This avoids a commonly encountered worst case and capitalizes on natural runs to bring runtime closer to O(n).

2. Merge Phase

Merge all stacks until 1 remains. This utilizes Powersort's near-optimal merge strategy. NOTE: Naive adjacent merging is currently implemented and will be refactored.

The full breakdown of the algorithm can be seen in the Preprint here: https://www.researchgate.net/publication/388955884_JesseSort

# Caveat

This is under active development, so the preprint and code here differ slightly. We use 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow. This is because split rainbows unnecessarily divide the ranges of the Patience Sort inputs and require suboptimal middle-insertions rather than faster tail-end insertions.
