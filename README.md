# JesseSort

JesseSort is a novel sorting algorithm that introduces a new data structure called a Rainbow to efficiently organize and merge elements. It achieves a runtime of O(n log n).

# Setup

To compile JesseSort, run:

`python setup.py build_ext --inplace`

Then you can import it with:

`from jessesort_c import jessesort`


# Algorithm Overview

JesseSort consists of two main phases:

1. Generating the Rainbow

The input array is structured into a Rainbow, a hierarchical grouping of elements that facilitates efficient merging.

This structuring is performed in O(n log(1.5 Hₙ)) time, leveraging properties of harmonic numbers to optimize the sorting process.

2. Merging the Rainbow bands

The bands of the Rainbow are recursively merged until only 1 remains.


# Runtime Analysis

JesseSort's total runtime is O(n log n).


# Algorithm

JesseSort's efficiency comes from its use of the Rainbow structure, which optimizes the merging process by leveraging harmonic growth properties.

Stay tuned for optimizations, such as merge phase parallelization and optimal PowerSort merge logic.
