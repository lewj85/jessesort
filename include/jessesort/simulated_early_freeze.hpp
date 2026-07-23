#ifndef JESSESORT_SIMULATED_EARLY_FREEZE_HPP
#define JESSESORT_SIMULATED_EARLY_FREEZE_HPP

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits>
#include <functional>
#include <type_traits>
#include <utility>
#include <climits>


namespace jessesort::simulated_early_freeze {
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
static constexpr uint32_t OVERFLOW_TAG = 0xffffffffu;

inline uint32_t makeAscTag(uint32_t localPile) {
    return localPile;
}

inline uint32_t makeDescTag(uint32_t localPile) {
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
            r = numAscPiles + local;
            reverseThisRun = reverseDescRuns;
        } else {
            r = local;
            reverseThisRun = reverseAscRuns;
        }

        if (reverseThisRun) {
            tmp[--rightCursor[r]] = arr[i];
        } else {
            tmp[leftCursor[r]++] = arr[i];
        }
    }

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

    const std::size_t n = tmp.size();

    if (n == 0) {
        arr.clear();
        return;
    }


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

    // #ifndef NDEBUG
    // #endif
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

    // #ifndef NDEBUG
    // #endif

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

    // #ifndef NDEBUG
    // #endif

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
void sortWithoutFreeze(std::vector<T>& arr, Less less = Less{}) {
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

    // #ifndef NDEBUG
    // #endif

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


// =========================================================
// Early-freezing variant with live 32-element overflow bands
// =========================================================

inline std::size_t calculateEarlyFreezePileLimit(std::size_t n) {
    if (n < 2) return 1;
    return std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(1.213 * std::sqrt(static_cast<double>(n))))
    );
}

template <typename T>
struct FrozenInsertionResult : SimulatedInsertionResult<T> {
    std::size_t overflowCount = 0;
    std::size_t pileLimit = 0;
};

template <typename T, typename Less = std::less<T>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreeze(
    const std::vector<T>& arr,
    const T& lowSentinel,
    const T& highSentinel,
    Less less = Less{}
) {
    const std::size_t n = arr.size();
    FrozenInsertionResult<T> result;
    result.blueprint.resize(n);
    result.pileLimit = calculateEarlyFreezePileLimit(n);

    const std::size_t reservePiles = result.pileLimit + 1;
    result.ascCounts.reserve(reservePiles);
    result.descCounts.reserve(reservePiles);
    result.ascBaseArray.reserve(reservePiles + 1);
    result.descBaseArray.reserve(reservePiles + 1);
    result.ascBaseArray.push_back(lowSentinel);
    result.descBaseArray.push_back(highSentinel);

    if (n == 0) return result;

    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
    bool descendingMode = false;
    T lastValueProcessed = arr[0];
    bool alreadySortedAscending = true;
    bool reverseSortedDescending = true;

    for (std::size_t i = 0; i < n; ++i) {
        const T& value = arr[i];
        if (i != 0) {
            if (less(lastValueProcessed, value)) {
                descendingMode = false;
                reverseSortedDescending = false;
            } else if (less(value, lastValueProcessed)) {
                descendingMode = true;
                alreadySortedAscending = false;
            }
        }

        bool overflow = false;
        if (descendingMode) {
            // Descending-game tails are ascending. Once frozen, a value larger
            // than the last real tail would require a new pile.
            if (result.descCounts.size() >= result.pileLimit &&
                !result.descCounts.empty() &&
                less(result.descBaseArray[result.descCounts.size() - 1], value)) {
                overflow = true;
            }
            if (!overflow) {
                simulateInsertValueDescendingPiles(
                    result.descBaseArray, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, highSentinel, less);
            }
        } else {
            // Ascending-game tails are descending. Once frozen, a value smaller
            // than the last real tail would require a new pile.
            if (result.ascCounts.size() >= result.pileLimit &&
                !result.ascCounts.empty() &&
                less(value, result.ascBaseArray[result.ascCounts.size() - 1])) {
                overflow = true;
            }
            if (!overflow) {
                simulateInsertValueAscendingPiles(
                    result.ascBaseArray, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, lowSentinel, less);
            }
        }

        if (overflow) {
            result.blueprint[i] = OVERFLOW_TAG;
            ++result.overflowCount;
        }
        lastValueProcessed = value;
    }

    result.alreadySortedAscending = alreadySortedAscending;
    result.reverseSortedDescending = reverseSortedDescending;
    return result;
}

template <typename T, typename Less = std::less<T>,
          typename = std::enable_if_t<std::numeric_limits<T>::is_specialized>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreeze(
    const std::vector<T>& arr,
    Less less = Less{}
) {
    return simulatePatienceInsertionBlueprintEarlyFreeze(
        arr, std::numeric_limits<T>::lowest(),
        std::numeric_limits<T>::max(), less);
}

template <typename T, typename Less = std::less<T>>
inline void binaryInsertIntoFlatOverflowBand(
    std::vector<T>& tmp,
    std::size_t bandStart,
    std::size_t bandSize,
    const T& value,
    Less less = Less{}
) {
    std::size_t lo = bandStart;
    std::size_t hi = bandStart + bandSize;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (less(value, tmp[mid])) hi = mid;
        else lo = mid + 1;
    }
    for (std::size_t j = bandStart + bandSize; j > lo; --j) {
        tmp[j] = std::move(tmp[j - 1]);
    }
    tmp[lo] = value;
}

template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithLiveOverflowBands(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    const std::vector<std::size_t>& ascCounts,
    const std::vector<std::size_t>& descCounts,
    std::size_t overflowCount,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    constexpr std::size_t BandSize = 32;
    const std::size_t n = arr.size();
    const std::size_t normalCount = n - overflowCount;
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numNormalRuns = numAscPiles + numDescPiles;

    std::vector<std::size_t> start(numNormalRuns + 1, 0);
    std::size_t out = 0;
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        start[p] = out; out += ascCounts[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t r = numAscPiles + p;
        start[r] = out; out += descCounts[p];
    }
    start[numNormalRuns] = out;
    // assert(out == normalCount);

    tmp.resize(n);
    std::vector<std::size_t> leftCursor(numNormalRuns);
    std::vector<std::size_t> rightCursor(numNormalRuns);
    for (std::size_t r = 0; r < numNormalRuns; ++r) {
        leftCursor[r] = start[r];
        rightCursor[r] = start[r + 1];
    }

    std::size_t overflowSeen = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == OVERFLOW_TAG) {
            const std::size_t band = overflowSeen / BandSize;
            const std::size_t within = overflowSeen % BandSize;
            const std::size_t bandStart = normalCount + band * BandSize;
            binaryInsertIntoFlatOverflowBand(tmp, bandStart, within, arr[i], less);
            ++overflowSeen;
            continue;
        }

        const bool desc = isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(localPileId(tag));
        const std::size_t r = desc ? numAscPiles + local : local;
        if (desc) tmp[--rightCursor[r]] = arr[i];
        else tmp[leftCursor[r]++] = arr[i];
    }
    // assert(overflowSeen == overflowCount);

    std::vector<std::size_t> runStart = start;
    for (std::size_t pos = normalCount; pos < n; pos += BandSize) {
        runStart.push_back(std::min(n, pos + BandSize));
    }
    return runStart;
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (arr.size() < 2) return;
    FrozenInsertionResult<T> sim =
        simulatePatienceInsertionBlueprintEarlyFreeze(arr, less);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }
    std::vector<T> tmp;
    std::vector<std::size_t> runStart =
        reconstructFrozenBlueprintWithLiveOverflowBands(
            arr, sim.blueprint, sim.ascCounts, sim.descCounts,
            sim.overflowCount, tmp, less);
    // assert(validateRunsAscending(tmp, runStart, less));
    mergeRunsFromTmpToArr(tmp, arr, runStart, less);
}

} // namespace jessesort::simulated_early_freeze
#endif
