# JesseSort

JesseSort is a novel sorting algorithm that introduces a new data structure called a Rainbow to efficiently organize and merge elements. It achieves a runtime of O(n log(Hₙ)) ≈ O(n log(ln n)), making it competitive with traditional sorting algorithms while leveraging properties of harmonic numbers for improved performance.

# Algorithm Overview

JesseSort consists of two main phases:

1. Generating the Rainbow (O(n log(1.5 Hₙ)))

The input array is structured into a Rainbow, a hierarchical grouping of elements that facilitates efficient merging.

This structuring is performed in O(n log(1.5 Hₙ)) time, leveraging properties of harmonic numbers to optimize the sorting process.

2. Merging the Rainbow Bands (O(n log(1.5 Hₙ)))

The bands of the Rainbow are recursively merged using a multiway merge strategy.

This merging process takes O(n log(1.5 Hₙ)) time, ensuring an efficient recombination of sorted elements into the final sorted output.

# Runtime Analysis

JesseSort's total runtime is the sum of its two phases:

O(n log(1.5 Hₙ)) + O(n log(1.5 Hₙ)) => O(n log(Hₙ))

# Understanding the Harmonic Term (Hₙ)

The harmonic number Hₙ is approximately:

Hₙ ≈ ln(n) + γ

where γ ≈ 0.577 is the Euler-Mascheroni constant. Substituting this into the runtime:

O(n log(1.5 ln(n)))

This shows that JesseSort has a slightly improved complexity over traditional O(n log n) sorting algorithms, particularly for large n.

# Algorithm

JesseSort's efficiency comes from its use of the Rainbow structure, which optimizes the merging process by leveraging harmonic growth properties.

# Usage

To use JesseSort, import the `jessesort()` function:

`from jessesort import jessesort`

Stay tuned for implementation details and optimizations!

# Future Improvements

Conversion to C/C++: Faster implementation for apples-to-apples speed comparisons.

Parallelization: Exploring multi-threaded implementations to further optimize merging.

Cache Efficiency: Improving memory access patterns for better real-world performance.

Experimental Analysis: Benchmarking against traditional sorting algorithms.

JesseSort aims to push the boundaries of sorting efficiency while introducing novel data structures like the Rainbow. 🌈🚀
