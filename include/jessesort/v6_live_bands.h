#ifndef JESSESORT_SIMULATED_EARLY_FREEZE_LIVE_HPP
#define JESSESORT_SIMULATED_EARLY_FREEZE_LIVE_HPP

#include <jessesort/v5_deferred_bands.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_early_freeze_live {

// V6 shares V5's early-freeze base structure but keeps each overflow band
// sorted as values arrive, trading insertion work for less deferred sorting.


namespace detail {

struct TimSortMergeNode {
    std::size_t begin = 0;
    std::size_t mid = 0;
    std::size_t end = 0;
    std::size_t left = 0;
    std::size_t right = 0;
    bool leaf = true;
};

inline std::size_t timsortNodeLength(const TimSortMergeNode& node) {
    return node.end - node.begin;
}

inline std::size_t makeTimSortMergedNode(
    std::vector<TimSortMergeNode>& nodes,
    std::size_t left,
    std::size_t right
) {
    assert(nodes[left].end == nodes[right].begin);
    const std::size_t index = nodes.size();
    nodes.push_back(TimSortMergeNode{
        nodes[left].begin,
        nodes[left].end,
        nodes[right].end,
        left,
        right,
        false
    });
    return index;
}

inline void timsortMergeAt(
    std::vector<std::size_t>& stack,
    std::vector<TimSortMergeNode>& nodes,
    std::size_t position
) {
    assert(position + 1 < stack.size());
    stack[position] = makeTimSortMergedNode(
        nodes, stack[position], stack[position + 1]);
    stack.erase(stack.begin() + static_cast<std::ptrdiff_t>(position + 1));
}

inline void timsortCollapse(
    std::vector<std::size_t>& stack,
    std::vector<TimSortMergeNode>& nodes
) {
    while (stack.size() > 1) {
        std::size_t n = stack.size() - 2;
        const auto length = [&](std::size_t stackIndex) {
            return timsortNodeLength(nodes[stack[stackIndex]]);
        };

        if ((n > 0 && length(n - 1) <= length(n) + length(n + 1)) ||
            (n > 1 && length(n - 2) <= length(n - 1) + length(n))) {
            if (n > 0 && length(n - 1) < length(n + 1)) --n;
            timsortMergeAt(stack, nodes, n);
        } else if (length(n) <= length(n + 1)) {
            timsortMergeAt(stack, nodes, n);
        } else {
            break;
        }
    }
}

inline void timsortForceCollapse(
    std::vector<std::size_t>& stack,
    std::vector<TimSortMergeNode>& nodes
) {
    while (stack.size() > 1) {
        std::size_t n = stack.size() - 2;
        const auto length = [&](std::size_t stackIndex) {
            return timsortNodeLength(nodes[stack[stackIndex]]);
        };
        if (n > 0 && length(n - 1) < length(n + 1)) --n;
        timsortMergeAt(stack, nodes, n);
    }
}

inline std::size_t timsortLeafCopyCost(
    const std::vector<TimSortMergeNode>& nodes,
    std::size_t nodeIndex,
    bool outputToArr
) {
    const TimSortMergeNode& node = nodes[nodeIndex];
    if (node.leaf) return outputToArr ? timsortNodeLength(node) : 0;
    return timsortLeafCopyCost(nodes, node.left, !outputToArr) +
           timsortLeafCopyCost(nodes, node.right, !outputToArr);
}

template <typename T, typename Less>
void materializeTimSortTree(
    const std::vector<TimSortMergeNode>& nodes,
    std::size_t nodeIndex,
    bool outputToArr,
    std::vector<T>& tmp,
    std::vector<T>& arr,
    Less less
) {
    const TimSortMergeNode& node = nodes[nodeIndex];
    if (node.leaf) {
        if (outputToArr) {
            std::move(tmp.begin() + static_cast<std::ptrdiff_t>(node.begin),
                      tmp.begin() + static_cast<std::ptrdiff_t>(node.end),
                      arr.begin() + static_cast<std::ptrdiff_t>(node.begin));
        }
        return;
    }

    materializeTimSortTree(nodes, node.left, !outputToArr, tmp, arr, less);
    materializeTimSortTree(nodes, node.right, !outputToArr, tmp, arr, less);

    std::vector<T>& src = outputToArr ? tmp : arr;
    std::vector<T>& dst = outputToArr ? arr : tmp;
    simulated::mergeTwoAdjacentRunsToDest(
        src, dst, node.begin, node.mid, node.end, less);
}

inline bool shouldUseTimSortMerge(
    const std::vector<std::size_t>& runStart,
    std::size_t n
) {
    const std::size_t runCount = runStart.size() - 1;
    if (runCount < 3) return false;

    std::size_t maxLength = 0;
    for (std::size_t r = 0; r < runCount; ++r) {
        maxLength = std::max(
            maxLength, runStart[r + 1] - runStart[r]);
    }

    // The TimSort-style tree is selected only when one run is at least eight
    // times the mean run length. Balanced run sets remain on the simpler,
    // sequential adjacent-pair merge.
    return static_cast<long double>(maxLength) *
               static_cast<long double>(runCount) >=
           8.0L * static_cast<long double>(n);
}

template <typename T, typename Less>
void mergeRunsTimSortStyle(
    std::vector<T>& tmp,
    std::vector<T>& arr,
    std::vector<std::size_t> runStart,
    Less less
) {
    const std::size_t n = tmp.size();
    if (n == 0) {
        arr.clear();
        return;
    }

    // Match the production adjacent scheduler's first ordered-boundary probe.
    if (runStart.size() > 2) {
        std::size_t write = 1;
        for (std::size_t r = 1; r + 1 < runStart.size(); ++r) {
            const std::size_t boundary = runStart[r];
            if (less(tmp[boundary], tmp[boundary - 1]))
                runStart[write++] = boundary;
        }
        runStart[write++] = n;
        runStart.resize(write);
    }

    const std::size_t runCount = runStart.size() - 1;
    if (runCount == 0) {
        arr.clear();
        return;
    }
    if (runCount == 1) {
        arr.swap(tmp);
        return;
    }

    std::vector<TimSortMergeNode> nodes;
    nodes.reserve(runCount * 2 - 1);
    std::vector<std::size_t> stack;
    stack.reserve(96);

    for (std::size_t r = 0; r < runCount; ++r) {
        const std::size_t nodeIndex = nodes.size();
        nodes.push_back(TimSortMergeNode{
            runStart[r], runStart[r], runStart[r + 1], 0, 0, true});
        stack.push_back(nodeIndex);
        timsortCollapse(stack, nodes);
    }
    timsortForceCollapse(stack, nodes);
    assert(stack.size() == 1);

    const std::size_t root = stack.front();
    const std::size_t copyToArr = timsortLeafCopyCost(nodes, root, true);
    const std::size_t copyToTmp = timsortLeafCopyCost(nodes, root, false);
    const bool outputToArr = copyToArr <= copyToTmp;
    materializeTimSortTree(nodes, root, outputToArr, tmp, arr, less);
    if (!outputToArr) arr.swap(tmp);
}

} // namespace detail

// True live-overflow variation: overflow values are inserted immediately into
// their current fixed-size sorted band during blueprint reconstruction.
// Band size 32 was restored by the random-input audit after the selected
// power-of-two freeze policy eliminated the organ-pipe overflow regime that
// had previously favored 8-element bands.
template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithLiveOverflowBands(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
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
    const std::size_t overflowRuns = (overflowCount + BandSize - 1) / BandSize;

    std::vector<std::size_t> runStart;
    runStart.reserve(numNormalRuns + overflowRuns + 1);
    runStart.resize(numNormalRuns + 1, 0);

    std::size_t out = 0;
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        runStart[p] = out;
        out += ascCounts[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t r = numAscPiles + p;
        runStart[r] = out;
        out += descCounts[p];
    }
    runStart[numNormalRuns] = out;
    assert(out == normalCount);

    if constexpr (std::is_default_constructible_v<T>) tmp.resize(n);
    else tmp = arr;

    for (std::size_t p = 0; p < numAscPiles; ++p) ascCounts[p] = runStart[p];
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        descCounts[p] = runStart[numAscPiles + p + 1];
    }

    std::size_t overflowSeen = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == simulated::OVERFLOW_TAG) {
            const std::size_t bandStart = normalCount + (overflowSeen / BandSize) * BandSize;
            std::size_t pos = normalCount + overflowSeen;
            while (pos > bandStart && less(arr[i], tmp[pos - 1])) {
                tmp[pos] = std::move(tmp[pos - 1]);
                --pos;
            }
            tmp[pos] = arr[i];
            ++overflowSeen;
            continue;
        }

        const bool desc = simulated::isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(simulated::localPileId(tag));
        if (desc) tmp[--descCounts[local]] = arr[i];
        else tmp[ascCounts[local]++] = arr[i];
    }
    assert(overflowSeen == overflowCount);

    for (std::size_t end = normalCount + std::min(BandSize, overflowCount);
         end <= n && end > normalCount;
         end += BandSize) {
        runStart.push_back(end);
        if (end == n) break;
        if (n - end < BandSize) end = n - BandSize;
    }
    return runStart;
}

// Public V6 entry point: early freeze with live-sorted overflow bands.
template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_early_freeze_live::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_early_freeze_live::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) return;
    auto sim = simulated_early_freeze::simulatePatienceInsertionBlueprintEarlyFreeze(
        arr, less, simulated_early_freeze::FreezePolicy::power2_broad);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }
    bool useRandomBranchlessMerge = false;
    if constexpr (std::is_trivially_copyable_v<T> &&
                  sizeof(T) <= 2 * sizeof(void*)) {
        useRandomBranchlessMerge =
            simulated_early_freeze::frozenInsertionLooksRandomLike(
                sim, arr.size());
    }

    std::vector<T> tmp;
    auto runStart = reconstructFrozenBlueprintWithLiveOverflowBands(
        arr, sim.blueprint, std::move(sim.ascCounts), std::move(sim.descCounts),
        sim.overflowCount, tmp, less);
    if (useRandomBranchlessMerge) {
        simulated::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less,
            simulated::MergeSchedule::adjacent_pairs, true);
    } else if (detail::shouldUseTimSortMerge(runStart, arr.size())) {
        detail::mergeRunsTimSortStyle(
            tmp, arr, std::move(runStart), less);
    } else {
        simulated::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less);
    }
}

} // namespace jessesort::simulated_early_freeze_live
#endif
