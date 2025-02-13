# JesseSort

JesseSort is a novel sorting algorithm that introduces a new data structure called a Rainbow to efficiently organize and merge elements. It achieves a runtime of O(n log n).

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

1. Insertion Phase - generates a Rainbow data structure

2. Merge Phase - merges its bands until 1 remains

The full breakdown of the algorithm can be seen in JesseSort.pdf. Preprint also available here: https://www.researchgate.net/publication/388955884_JesseSort
