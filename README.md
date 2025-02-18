# JesseSort

JesseSort is a hybrid sorting algorithm that utilizes 2 Patience Sorts and Powersort under the hood.

# Setup

To compile JesseSort, run:

```
pip install cython
python setup.py build_ext --inplace
```

Then you can import it with:

`from jessesort_c import jessesort`


# Algorithm Overview

JesseSort consists of two main phases:

1. Insertion Phase

Play 2 games of Patience, one with descending stacks and one with ascending stacks. Send values to the optimal game based on the order of the current natural run in the input array. This avoids a commonly encountered worst case and capitalizes on natural runs to bring runtime closer to O(n).

2. Merge Phase - Merges all stacks until 1 remains, utilizing Powersort's near-optimal merge tree.

The full breakdown of the algorithm can be seen in JesseSort.pdf. Preprint is also available here: https://www.researchgate.net/publication/388955884_JesseSort

CAVEAT: This is under active development, so the preprint and code differs slightly from the implementation being worked. We will be using 2 half rainbows (Patience Sort's default output structure) instead of 1 split rainbow.
