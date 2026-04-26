#ifndef JESSESORT_HPP  // Include guard
#define JESSESORT_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>
#include <type_traits>
#include <utility>
#include <climits>

// =========================================================
// Simulated Jessesort patience insertion + reconstruction + merge
// =========================================================
//
// Pipeline:
//   1. Simulate dual patience insertion using only tail/base arrays.
//   2. Store one packed blueprint entry per original element:
//        high bit = game ID, low bits = local pile ID.
//   3. Store per-game pile counts during simulation.
//   4. Reconstruct physically ascending flat runs into tmp.
//        - ascending-game piles preserve encounter order.
//        - descending-game piles reverse encounter order.
//   5. Return run boundaries for the reconstructed runs in tmp.
//   6. Merge adjacent run pairs bottom-up.
//        - first merge pass reads tmp and writes arr.
//        - later passes ping-pong between arr and tmp.
//        - odd leftover runs are copied/moved forward unchanged.
//        - already-ordered adjacent runs use a fast whole-span move.
//        - reverse-disjoint adjacent runs use a right-then-left fast path.
//   7. Ensure arr owns the final sorted result before returning.
//
// Important convention:
//   Ascending-game piles are ascending in encounter order, but their base/tail
//   array is descending.
//
//   Descending-game piles are descending in encounter order, but their base/tail
//   array is ascending.
//
// Default reconstruction:
//   ascending-game runs: preserve encounter order
//   descending-game runs: reverse encounter order
//
// Merge strategy:
//   The merge phase uses adjacent bottom-up pairwise merging rather than a heap,
//   loser tree, or best-two scheduler. This keeps the merge pass simple,
//   sequential, and cache-friendly. For random-like input, adjacent reconstructed
//   runs are expected to be similar enough in size that avoiding pairing
//   overhead is likely better than optimizing the merge tree.
//
// Stability:
//   The ordinary element-by-element merge path is stable, but the
//   reverse-disjoint fast path is not stable across equal keys. The overall sort
//   should therefore be treated as unstable.
//
// For floating-point types, NaNs are not supported by this logic because they
// break strict weak ordering.

// =========================================================
// Packed blueprint tags
// =========================================================
//
// high bit:
//   0 = ascending game
//   1 = descending game
//
// lower 31 bits:
//   local pile ID inside that game

static constexpr uint32_t DESC_BIT  = 0x80000000u;
static constexpr uint32_t PILE_MASK = 0x7fffffffu;

inline uint32_t makeAscTag(uint32_t localPile) {
    assert((localPile & DESC_BIT) == 0);
    return localPile;
}

inline uint32_t makeDescTag(uint32_t localPile) {
    assert((localPile & DESC_BIT) == 0);
    return DESC_BIT | localPile;
}

inline bool isDescTag(uint32_t tag) {
    return (tag & DESC_BIT) != 0;
}

inline bool isAscTag(uint32_t tag) {
    return (tag & DESC_BIT) == 0;
}

inline uint32_t localPileId(uint32_t tag) {
    return tag & PILE_MASK;
}

inline std::size_t globalRunIdFromTag(
    uint32_t tag,
    std::size_t numAscPiles
) {
    const std::size_t local = static_cast<std::size_t>(localPileId(tag));
    return isDescTag(tag) ? (numAscPiles + local) : local;
}

inline int highestPowerOfTwoLE(int n) {
    assert(n > 0);
    const unsigned un = static_cast<unsigned>(n);
    return 1 << (31 - __builtin_clz(un));
}

// The old branchless search can probe up to just under 2 * bit_floor(n).
// So allocate at least 2 * bit_floor(n) entries.
// Since n <= 2 * bit_floor(n) - 1, this always leaves sentinel room.
inline std::size_t requiredBranchlessBaseSize(std::size_t realPileCount) {
    if (realPileCount == 0) {
        return 1;
    }

    assert(realPileCount <= static_cast<std::size_t>(INT_MAX));

    const int n = static_cast<int>(realPileCount);
    const int step = highestPowerOfTwoLE(n);

    return static_cast<std::size_t>(2 * step);
}

template <typename T>
inline void ensureBranchlessPadding(
    std::vector<T>& baseArray,
    std::size_t realPileCount,
    const T& sentinel
) {
    const std::size_t requiredSize =
        requiredBranchlessBaseSize(realPileCount);

    if (baseArray.size() < requiredSize) {
        baseArray.resize(requiredSize, sentinel);
    }

    // At minimum, the first padding slot must be sentinel.
    // Existing padding beyond this point should already be sentinel because
    // resize fills with sentinel and real tails only occupy [0, realPileCount).
    assert(baseArray.size() >= realPileCount + 1 || realPileCount == 0);
}

// =========================================================
// Result structs
// =========================================================

template <typename T>
struct SimulatedInsertionResult {
    std::vector<uint32_t> blueprint;

    // Counts are split while simulating because numAscPiles is not final until
    // insertion finishes. Reconstruction later treats global run order as:
    //   [asc pile 0 ... asc pile A-1][desc pile 0 ... desc pile D-1]
    std::vector<std::size_t> ascCounts;
    std::vector<std::size_t> descCounts;

    bool alreadySortedAscending = true;
    bool reverseSortedDescending = true;

    // Diagnostic/search state. These are not needed after reconstruction.
    std::vector<T> ascBaseArray;   // descending tails + low sentinel
    std::vector<T> descBaseArray;  // ascending tails + high sentinel
};

struct ReconstructedRuns {
    std::vector<std::size_t> runStart;
    bool sourceIsTmp = true;
};

// =========================================================
// Small helpers
// =========================================================

inline std::size_t estimatePileReserve(std::size_t n) {
    if (n == 0) return 1;

    const double root = std::sqrt(static_cast<double>(n));

    // Conservative per game. Over-reserving O(sqrt(n)) metadata is small
    // compared to n-sized value/blueprint arrays.
    return std::max<std::size_t>(
        32,
        static_cast<std::size_t>(2.0 * root) + 16
    );
}

// =========================================================
// Branchless-ish pile searches
// =========================================================
//
// Less must implement a strict weak ordering compatible with all values and
// sentinels.

// Descending-game piles:
//   pile values descend in encounter order.
//   baseArray/tails are ascending.
//   find first tail >= value.
template <typename T, typename Less = std::less<T>>
inline int findDescendingPileWithBaseArray(
    const std::vector<T>& baseArray,
    int& mid,
    const T& value,
    Less less = Less{}
) {
    const int n = static_cast<int>(baseArray.size()) - 1; // sentinel at baseArray[n]
    assert(n > 0);
    assert(mid >= 0 && mid < n);

    // Descending-game piles:
    //   pile values descend in encounter order.
    //   baseArray/tails are ascending.
    //
    // Find first tail >= value.
    //
    // tail >= value  <=>  !less(tail, value)
    // prev < value   <=>   less(prev, value)

    if (!less(baseArray[mid], value) &&
        (mid == 0 || less(baseArray[mid - 1], value))) {
        return mid;
    }

    // Safe bit-walk lower_bound.
    //
    // idx tracks the last index where baseArray[idx] < value.
    // Return idx + 1.
    int idx = -1;

    const unsigned un = static_cast<unsigned>(n);
    int step = 1 << (31 - __builtin_clz(un));

    for (; step != 0; step >>= 1) {
        const int next = idx + step;

        if (next < n && less(baseArray[next], value)) {
            idx = next;
        }
    }

    mid = idx + 1;
    return mid;
}

// Ascending-game piles:
//   pile values ascend in encounter order.
//   baseArray/tails are descending.
//   find first tail <= value.
template <typename T, typename Less = std::less<T>>
inline int findAscendingPileWithBaseArray(
    const std::vector<T>& baseArray,
    int& mid,
    const T& value,
    Less less = Less{}
) {
    const int n = static_cast<int>(baseArray.size()) - 1; // sentinel at baseArray[n]
    assert(n > 0);
    assert(mid >= 0 && mid < n);

    // Ascending-game piles:
    //   pile values ascend in encounter order.
    //   baseArray/tails are descending.
    //
    // Find first tail <= value.
    //
    // tail <= value  <=>  !less(value, tail)
    // prev > value   <=>   less(value, prev)

    if (!less(value, baseArray[mid]) &&
        (mid == 0 || less(value, baseArray[mid - 1]))) {
        return mid;
    }

    // Safe bit-walk lower_bound over a descending array.
    //
    // idx tracks the last index where baseArray[idx] > value.
    // Return idx + 1.
    int idx = -1;

    const unsigned un = static_cast<unsigned>(n);
    int step = 1 << (31 - __builtin_clz(un));

    for (; step != 0; step >>= 1) {
        const int next = idx + step;

        if (next < n && less(value, baseArray[next])) {
            idx = next;
        }
    }

    mid = idx + 1;
    return mid;
}

// =========================================================
// Simulated insertion into one game
// =========================================================
//
// These functions do not build pile vectors. They only update tails/base arrays,
// write the packed blueprint tag, and increment per-pile counts.

// Descending game:
//   baseArray is ascending, with highSentinel at the end.
template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueDescendingPiles(
    std::vector<T>& descendingBaseArray,
    int& lastPileIndexDescending,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& descCounts,
    const T& highSentinel,
    Less less = Less{}
) {
    assert(originalIndex < blueprint.size());
    assert(descendingBaseArray.size() == descCounts.size() + 1);

    int pileIndex = 0;

    if (!descCounts.empty()) {
        pileIndex = findDescendingPileWithBaseArray(
            descendingBaseArray,
            lastPileIndexDescending,
            value,
            less
        );
    }

    const int sentinelIndex =
        static_cast<int>(descendingBaseArray.size()) - 1;

    if (pileIndex < sentinelIndex) {
        // Existing descending pile.
        descendingBaseArray[pileIndex] = value;
        lastPileIndexDescending = pileIndex;

        blueprint[originalIndex] =
            makeDescTag(static_cast<uint32_t>(pileIndex));

        ++descCounts[static_cast<std::size_t>(pileIndex)];
    } else {
        // New descending pile: replace old sentinel with value, then append a
        // new high sentinel.
        const std::size_t newPileIndex = descCounts.size();

        descendingBaseArray.back() = value;
        descendingBaseArray.push_back(highSentinel);

        descCounts.push_back(1);

        lastPileIndexDescending = static_cast<int>(newPileIndex);

        blueprint[originalIndex] =
            makeDescTag(static_cast<uint32_t>(newPileIndex));
    }
}

// Ascending game:
//   baseArray is descending, with lowSentinel at the end.
template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueAscendingPiles(
    std::vector<T>& ascendingBaseArray,
    int& lastPileIndexAscending,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& ascCounts,
    const T& lowSentinel,
    Less less = Less{}
) {
    assert(originalIndex < blueprint.size());
    assert(ascendingBaseArray.size() == ascCounts.size() + 1);

    int pileIndex = 0;

    if (!ascCounts.empty()) {
        pileIndex = findAscendingPileWithBaseArray(
            ascendingBaseArray,
            lastPileIndexAscending,
            value,
            less
        );
    }

    const int sentinelIndex =
        static_cast<int>(ascendingBaseArray.size()) - 1;

    if (pileIndex < sentinelIndex) {
        // Existing ascending pile.
        ascendingBaseArray[pileIndex] = value;
        lastPileIndexAscending = pileIndex;

        blueprint[originalIndex] =
            makeAscTag(static_cast<uint32_t>(pileIndex));

        ++ascCounts[static_cast<std::size_t>(pileIndex)];
    } else {
        // New ascending pile: replace old sentinel with value, then append a
        // new low sentinel.
        const std::size_t newPileIndex = ascCounts.size();

        ascendingBaseArray.back() = value;
        ascendingBaseArray.push_back(lowSentinel);

        ascCounts.push_back(1);

        lastPileIndexAscending = static_cast<int>(newPileIndex);

        blueprint[originalIndex] =
            makeAscTag(static_cast<uint32_t>(newPileIndex));
    }
}

// =========================================================
// Full simulated insertion pass
// =========================================================
//
// Explicit-sentinel version. Use this for custom types, or when you want direct
// control over sentinels.
//
// Requirements:
//   lowSentinel compares <= every input value.
//   highSentinel compares >= every input value.

template <typename T, typename Less = std::less<T>>
SimulatedInsertionResult<T> simulatePatienceInsertionBlueprint(
    const std::vector<T>& arr,
    const T& lowSentinel,
    const T& highSentinel,
    Less less = Less{}
) {
    const std::size_t n = arr.size();

    SimulatedInsertionResult<T> result;
    result.blueprint.resize(n);

    const std::size_t reservePiles = estimatePileReserve(n);

    result.ascCounts.reserve(reservePiles);
    result.descCounts.reserve(reservePiles);

    result.ascBaseArray.reserve(reservePiles + 1);
    result.descBaseArray.reserve(reservePiles + 1);

    // Ascending-game base array is descending, with low sentinel.
    // Descending-game base array is ascending, with high sentinel.
    result.ascBaseArray.push_back(lowSentinel);
    result.descBaseArray.push_back(highSentinel);

    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;

    bool descendingMode = false;

    if (n == 0) {
        return result;
    }

    T lastValueProcessed = arr[0];

    bool alreadySortedAscending = true;
    bool reverseSortedDescending = true;

    for (std::size_t i = 0; i < n; ++i) {
        const T& value = arr[i];

        if (i != 0) {
            if (less(lastValueProcessed, value)) {
                // value > lastValueProcessed
                descendingMode = false;
                reverseSortedDescending = false;
            } else if (less(value, lastValueProcessed)) {
                // value < lastValueProcessed
                descendingMode = true;
                alreadySortedAscending = false;
            }
            // Equal values keep the previous mode and preserve both monotone flags.
        }

        if (descendingMode) {
            simulateInsertValueDescendingPiles(
                result.descBaseArray,
                lastPileIndexDescending,
                value,
                i,
                result.blueprint,
                result.descCounts,
                highSentinel,
                less
            );
        } else {
            simulateInsertValueAscendingPiles(
                result.ascBaseArray,
                lastPileIndexAscending,
                value,
                i,
                result.blueprint,
                result.ascCounts,
                lowSentinel,
                less
            );
        }

        lastValueProcessed = value;
    }

    result.alreadySortedAscending = alreadySortedAscending;
    result.reverseSortedDescending = reverseSortedDescending;

    assert(result.ascBaseArray.size() == result.ascCounts.size() + 1);
    assert(result.descBaseArray.size() == result.descCounts.size() + 1);

    return result;
}

// Numeric convenience version.
template <
    typename T,
    typename Less = std::less<T>,
    typename = std::enable_if_t<std::numeric_limits<T>::is_specialized>
>
SimulatedInsertionResult<T> simulatePatienceInsertionBlueprint(
    const std::vector<T>& arr,
    Less less = Less{}
) {
    return simulatePatienceInsertionBlueprint(
        arr,
        std::numeric_limits<T>::lowest(),
        std::numeric_limits<T>::max(),
        less
    );
}

// =========================================================
// Reconstruction to temp
// =========================================================
//
// Input:
//   arr       = original values, still unmoved
//   blueprint = packed gameID + local pileID per original index
//   ascCounts / descCounts = counts produced during simulation
//
// Output:
//   tmp      = physically materialized flat runs
//   return   = run boundaries into tmp
//
// Global run order in tmp:
//   [asc pile 0 ... asc pile A-1][desc pile 0 ... desc pile D-1]
//
// By default:
//   ascending-game runs preserve encounter order.
//   descending-game runs reverse encounter order so they become ascending.

template <typename T>
std::vector<std::size_t> reconstructTaggedBlueprintToTemp_WithSplitCounts(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    const std::vector<std::size_t>& ascCounts,
    const std::vector<std::size_t>& descCounts,
    std::vector<T>& tmp,
    bool reverseAscRuns = false,
    bool reverseDescRuns = true
) {
    const std::size_t n = arr.size();

    assert(blueprint.size() == n);

    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numRuns = numAscPiles + numDescPiles;

    // 1. Prefix-sum split counts into global run starts.
    std::vector<std::size_t> start(numRuns + 1, 0);

    std::size_t out = 0;

    for (std::size_t p = 0; p < numAscPiles; ++p) {
        start[p] = out;
        out += ascCounts[p];
    }

    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t r = numAscPiles + p;
        start[r] = out;
        out += descCounts[p];
    }

    start[numRuns] = out;
    assert(start[numRuns] == n);

    // 2. Allocate output buffer.
    tmp.resize(n);

    // 3. Mutable cursors.
    std::vector<std::size_t> leftCursor(numRuns);
    std::vector<std::size_t> rightCursor(numRuns);

    for (std::size_t r = 0; r < numRuns; ++r) {
        leftCursor[r] = start[r];
        rightCursor[r] = start[r + 1];
    }

    // 4. One forward scan over original input.
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        const bool desc = isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(localPileId(tag));

        std::size_t r;
        bool reverseThisRun;

        if (desc) {
            assert(local < numDescPiles);
            r = numAscPiles + local;
            reverseThisRun = reverseDescRuns;
        } else {
            assert(local < numAscPiles);
            r = local;
            reverseThisRun = reverseAscRuns;
        }

        if (reverseThisRun) {
            tmp[--rightCursor[r]] = arr[i];
        } else {
            tmp[leftCursor[r]++] = arr[i];
        }
    }

    // 5. Debug cursor validation.
    #ifndef NDEBUG
    for (std::size_t r = 0; r < numRuns; ++r) {
        const bool descRun = (r >= numAscPiles);
        const bool reverseThisRun = descRun ? reverseDescRuns : reverseAscRuns;

        if (reverseThisRun) {
            assert(rightCursor[r] == start[r]);
        } else {
            assert(leftCursor[r] == start[r + 1]);
        }
    }
    #endif

    return start;
}

// =========================================================
// Validation helpers
// =========================================================

template <typename T, typename Less = std::less<T>>
bool validateRunsAscending(
    const std::vector<T>& data,
    const std::vector<std::size_t>& runStart,
    Less less = Less{}
) {
    if (runStart.empty()) return data.empty();

    const std::size_t numRuns = runStart.size() - 1;

    for (std::size_t r = 0; r < numRuns; ++r) {
        const std::size_t begin = runStart[r];
        const std::size_t end = runStart[r + 1];

        assert(begin <= end);
        assert(end <= data.size());

        for (std::size_t i = begin + 1; i < end; ++i) {
            if (less(data[i], data[i - 1])) {
                return false;
            }
        }
    }

    return true;
}

// Optional final sorted check for testing after your merge is added.
template <typename T, typename Less = std::less<T>>
bool validateSorted(
    const std::vector<T>& data,
    Less less = Less{}
) {
    for (std::size_t i = 1; i < data.size(); ++i) {
        if (less(data[i], data[i - 1])) {
            return false;
        }
    }
    return true;
}

// =========================================================
// Merge logic
// =========================================================
//
// Contract:
//
//   Before merge:
//     tmp contains flat ascending runs.
//     arr still contains the original input.
//     runStart gives run boundaries inside tmp.
//
//   After merge:
//     arr contains the fully sorted result.
//
// Implementation:
//   Adjacent bottom-up pairwise merge.
//   Ping-pongs between tmp and arr.
//   First pass reads tmp and writes arr.
//   If the final sorted data ends in tmp, arr.swap(tmp) makes arr the result.
//
// Notes:
//   - This merge is stable in the ordinary element-by-element path.
//   - The reverse-disjoint fast path is unstable for equal elements across runs,
//     but still sorted. Remove that fast path if stability ever matters.

template <typename T, typename Less = std::less<T>>
void mergeTwoAdjacentRunsToDest(
    std::vector<T>& src,
    std::vector<T>& dst,
    std::size_t begin,
    std::size_t mid,
    std::size_t end,
    Less less = Less{}
) {
    assert(begin <= mid);
    assert(mid <= end);
    assert(end <= src.size());
    assert(dst.size() >= src.size());

    if (begin == end) {
        return;
    }

    if (begin == mid) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(mid),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    if (mid == end) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    // Fast path 1:
    // left.max <= right.min, so the two runs are already globally ordered.
    //
    // src[mid - 1] <= src[mid]
    if (!less(src[mid], src[mid - 1])) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    // Fast path 2:
    // right.max <= left.min, so the merged output is simply right then left.
    //
    // src[end - 1] <= src[begin]
    //
    // This is sorted but unstable across equal keys.
    if (!less(src[begin], src[end - 1])) {
        const std::size_t rightLen = end - mid;

        std::move(src.begin() + static_cast<std::ptrdiff_t>(mid),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));

        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin + rightLen));

        return;
    }

    std::size_t i = begin;
    std::size_t j = mid;
    std::size_t out = begin;

    while (i < mid && j < end) {
        if (less(src[j], src[i])) {
            dst[out++] = std::move(src[j++]);
        } else {
            dst[out++] = std::move(src[i++]);
        }
    }

    if (i < mid) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(i),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(out));
    } else if (j < end) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(j),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(out));
    }
}

template <typename T, typename Less = std::less<T>>
void mergeRunsFromTmpToArr(
    std::vector<T>& tmp,
    std::vector<T>& arr,
    const std::vector<std::size_t>& runStart,
    Less less = Less{}
) {
    assert(tmp.size() == arr.size());

    const std::size_t n = tmp.size();

    if (n == 0) {
        arr.clear();
        return;
    }

    assert(!runStart.empty());
    assert(runStart.front() == 0);
    assert(runStart.back() == n);

    const std::size_t initialRuns = runStart.size() - 1;

    if (initialRuns == 0) {
        arr.clear();
        return;
    }

    // If there is only one run, tmp is already sorted.
    // Make arr the owner of the sorted data in O(1).
    if (initialRuns == 1) {
        arr.swap(tmp);
        return;
    }

    std::vector<std::size_t> currentStarts = runStart;
    std::vector<std::size_t> nextStarts;
    nextStarts.reserve((initialRuns + 1) / 2 + 1);

    // First pass reads tmp and writes arr.
    std::vector<T>* src = &tmp;
    std::vector<T>* dst = &arr;

    while (currentStarts.size() > 2) {
        const std::size_t numRuns = currentStarts.size() - 1;

        dst->resize(n);

        nextStarts.clear();
        nextStarts.push_back(currentStarts[0]);

        for (std::size_t r = 0; r < numRuns; r += 2) {
            const std::size_t begin = currentStarts[r];

            if (r + 1 == numRuns) {
                // Odd run out: copy/move it through unchanged.
                const std::size_t end = currentStarts[r + 1];

                std::move(src->begin() + static_cast<std::ptrdiff_t>(begin),
                          src->begin() + static_cast<std::ptrdiff_t>(end),
                          dst->begin() + static_cast<std::ptrdiff_t>(begin));

                nextStarts.push_back(end);
            } else {
                const std::size_t mid = currentStarts[r + 1];
                const std::size_t end = currentStarts[r + 2];

                mergeTwoAdjacentRunsToDest(
                    *src,
                    *dst,
                    begin,
                    mid,
                    end,
                    less
                );

                nextStarts.push_back(end);
            }
        }

        currentStarts.swap(nextStarts);
        std::swap(src, dst);
    }

    // After the loop, src points to the buffer containing the fully sorted run.
    // Ensure arr is the final output buffer.
    if (src != &arr) {
        arr.swap(tmp);
    }

    #ifndef NDEBUG
    assert(validateSorted(arr, less));
    #endif
}

// =========================================================
// Combined simulation + reconstruction wrapper
// =========================================================
//
// After this returns:
//   tmp[runStart[r] ... runStart[r + 1]) is one ascending run.
//
// The first merge pass should read from tmp and write to arr.

// Explicit-sentinel version.
template <typename T, typename Less = std::less<T>>
ReconstructedRuns simulateAndReconstructRunsToTemp(
    const std::vector<T>& arr,
    std::vector<T>& tmp,
    const T& lowSentinel,
    const T& highSentinel,
    Less less = Less{}
) {
    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(
            arr,
            lowSentinel,
            highSentinel,
            less
        );

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintToTemp_WithSplitCounts(
            arr,
            sim.blueprint,
            sim.ascCounts,
            sim.descCounts,
            tmp,
            false, // ascending-game piles preserve encounter order
            true   // descending-game piles reverse to become ascending
        );

    #ifndef NDEBUG
    assert(validateRunsAscending(tmp, runStart, less));
    #endif

    return ReconstructedRuns{
        std::move(runStart),
        true
    };
}

// Numeric convenience version.
template <typename T, typename Less = std::less<T>>
ReconstructedRuns simulateAndReconstructRunsToTemp(
    const std::vector<T>& arr,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(arr, less);

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintToTemp_WithSplitCounts(
            arr,
            sim.blueprint,
            sim.ascCounts,
            sim.descCounts,
            tmp,
            false,
            true
        );

    #ifndef NDEBUG
    assert(validateRunsAscending(tmp, runStart, less));
    #endif

    return ReconstructedRuns{
        std::move(runStart),
        true
    };
}

// =========================================================
// Public jesseSort wrapper
// =========================================================
//
// This version mutates arr into sorted order and also returns a copy.

template <typename T, typename Less = std::less<T>>
void jesseSort(std::vector<T>& arr, Less less = Less{}) {
    if (arr.size() < 2) {
        return;
    }

    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(arr, less);

    if (sim.alreadySortedAscending) {
        return;
    }

    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    std::vector<T> tmp;

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintToTemp_WithSplitCounts(
            arr,
            sim.blueprint,
            sim.ascCounts,
            sim.descCounts,
            tmp,
            false, // ascending-game piles preserve encounter order
            true   // descending-game piles reverse to become ascending
        );

    #ifndef NDEBUG
    assert(validateRunsAscending(tmp, runStart, less));
    #endif

    mergeRunsFromTmpToArr(
        tmp,
        arr,
        runStart,
        less
    );
}

// template <typename T, typename Less = std::less<T>>
// std::vector<T> jesseSort(std::vector<T> arr, Less less = Less{}) {
//     jesseSortInPlace(arr, less);
//     return arr;
// }

#endif // JESSESORT_HPP

////////////////////////////////////////////////
// TODO
////////////////////////////////////////////////
// - Refactor less naive merge methods to work with the new flat array. Implement
//       Huffman merging, smallest 2 piles iteratively. Overhead may be slower
//       than naive methods because of cache misses vs 2 adjacent runs already in cache.
//       As such, the best 2 of 3 adjacent may be better: (X + Y) + Z vs X + (Y + Z).
// - Early freezing at 1.213*sqrtn piles, reached at 36.8% (1/e) of n input values,
//       derived by S(a) = n(1 - a + a ln a). 36.8% still get inserted, 26.4% go to
//       overflow. Values that would make new bands instead get sorted separately. This
//       prevents many new small bands at the suboptimal ends, which may speed up the
//       merge phase because we're not doing so many memory lookups/trying to merge
//       many small arrays. Could stream overflow to std::sort.
// - Start new games from scratch when base arrays/piles exceed cache size.
//       Similar to freezing but instead of setting anything aside, you're just
//       building piles again, but bound to fit in cache. Overhead worth it?
// - Try in-place simulated pile flattening via O(n) swaps. Track pile sizes for dest, then:
//       for i in range(n):
//           while dest[i] != i:
//               j = dest[i]
//               arr[i], arr[j] = arr[j], arr[i]
//               dest[i], dest[j] = dest[j], dest[i]
// - AVX2 and AVX512 mask+count can turn the insertion phase binary search into near
//       O(n), even faster than branchless. This only works for k=16/32 piles so this
//       would rely on either: 1) chunking into many small games of limited piles
//       (which may slow down merge phase too much), or 2) force early merging
//       during the insertion phase to keep pile counts low. A balance between these
//       (some early merging to greatly increase chunk size) may be optimal here.
// - May be worth keeping base array in eytzinger order for faster binary search:
//       https://en.algorithmica.org/hpc/data-structures/binary-search/
//       S+ trees may also speed this up:
//       https://curiouscoding.nl/posts/static-search-tree/
