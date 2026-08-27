#ifndef JESSESORT_E229_JESSESORT_SIMULATED_FROZEN_DIRECT_MERGE_PROBE_ROUTED_ADJACENT_TIMSORT_ADAPTIVE_BUFFERED_LIVE_BANDS32_H
#define JESSESORT_E229_JESSESORT_SIMULATED_FROZEN_DIRECT_MERGE_PROBE_ROUTED_ADJACENT_TIMSORT_ADAPTIVE_BUFFERED_LIVE_BANDS32_H

#include <jessesort/tiny_sort.h>
#include <jessesort/experimental/jessesort_simulated-frozen_direct-merge-probe-routed_adjacent-adaptive-buffered_deferred-bands32.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_early_freeze_live_direct_merge {

// frozen-live shares frozen-deferred's early-freeze base structure but keeps each overflow band
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
    simulated_direct_merge::mergeTwoAdjacentRunsToDest(
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

// True live-overflow variation. High-pile/noisy overflow keeps the established
// fixed-size live-sorted bands. E175-final additionally allows low-pile
// structured overflow to preserve its naturally ascending live runs directly,
// avoiding thousands of artificial band boundaries that the merge probe would
// otherwise rediscover and collapse.
template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithLiveOverflowBands(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::size_t overflowCount,
    std::vector<T>& tmp,
    Less less = Less{},
    std::size_t bandSize = 32,
    bool useNaturalOverflowRuns = false,
    bool useBidirectionalNaturalRuns = false,
    std::size_t minNaturalRun = 32
) {
    const std::size_t BandSize = std::max<std::size_t>(1, bandSize);
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

    // E176 experimental state. Weak structure is absorbed into a minimum-size
    // live-sorted band; strong natural structure can grow without a fixed cap.
    enum class OverflowDirection : unsigned char { unknown, ascending, descending };
    OverflowDirection naturalDirection = OverflowDirection::unknown;
    std::size_t naturalStart = normalCount;
    bool fallbackBand = false;

    auto insertionSortRange = [&](std::size_t first, std::size_t last) {
        for (std::size_t k = first + 1; k < last; ++k) {
            T value = std::move(tmp[k]);
            std::size_t pos = k;
            while (pos > first && less(value, tmp[pos - 1])) {
                tmp[pos] = std::move(tmp[pos - 1]);
                --pos;
            }
            tmp[pos] = std::move(value);
        }
    };

    auto finalizeNaturalRun = [&](std::size_t first, std::size_t last,
                                  OverflowDirection direction) {
        if (last <= first) return;
        if (direction == OverflowDirection::descending)
            std::reverse(
                tmp.begin() + static_cast<std::ptrdiff_t>(first),
                tmp.begin() + static_cast<std::ptrdiff_t>(last));
        runStart.push_back(last);
    };

    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == simulated_direct_merge::OVERFLOW_TAG) {
            const std::size_t bandStart =
                normalCount + (overflowSeen / BandSize) * BandSize;
            const std::size_t bandEnd = normalCount + overflowSeen;

            if (useBidirectionalNaturalRuns) {
                tmp[bandEnd] = arr[i];

                if (overflowSeen == 0) {
                    naturalStart = bandEnd;
                    naturalDirection = OverflowDirection::unknown;
                    fallbackBand = false;
                } else if (fallbackBand) {
                    // The current weakly structured minimum-size band is kept
                    // live-sorted using the historical frozen-live insertion behavior.
                    T value = std::move(tmp[bandEnd]);
                    std::size_t pos = bandEnd;
                    while (pos > naturalStart && less(value, tmp[pos - 1])) {
                        tmp[pos] = std::move(tmp[pos - 1]);
                        --pos;
                    }
                    tmp[pos] = std::move(value);

                    if (bandEnd + 1 - naturalStart >= minNaturalRun) {
                        runStart.push_back(bandEnd + 1);
                        naturalStart = bandEnd + 1;
                        naturalDirection = OverflowDirection::unknown;
                        fallbackBand = false;
                    }
                } else {
                    const std::size_t previous = bandEnd - 1;
                    bool continues = true;
                    if (naturalDirection == OverflowDirection::unknown) {
                        if (less(tmp[previous], tmp[bandEnd]))
                            naturalDirection = OverflowDirection::ascending;
                        else if (less(tmp[bandEnd], tmp[previous]))
                            naturalDirection = OverflowDirection::descending;
                    } else if (naturalDirection == OverflowDirection::ascending) {
                        continues = !less(tmp[bandEnd], tmp[previous]);
                    } else {
                        continues = !less(tmp[previous], tmp[bandEnd]);
                    }

                    if (!continues) {
                        const std::size_t naturalLength = bandEnd - naturalStart;
                        if (naturalLength >= minNaturalRun) {
                            finalizeNaturalRun(
                                naturalStart, bandEnd, naturalDirection);
                            naturalStart = bandEnd;
                            naturalDirection = OverflowDirection::unknown;
                        } else {
                            // Do not emit tiny natural runs. Normalize the
                            // short candidate accumulated so far, then keep
                            // inserting live until the 32-element minimum fills.
                            insertionSortRange(naturalStart, bandEnd + 1);
                            fallbackBand = true;
                            naturalDirection = OverflowDirection::unknown;
                            if (bandEnd + 1 - naturalStart >= minNaturalRun) {
                                runStart.push_back(bandEnd + 1);
                                naturalStart = bandEnd + 1;
                                fallbackBand = false;
                            }
                        }
                    }
                }
            } else if (useNaturalOverflowRuns) {
                // E175-final baseline: ascending-only natural runs.
                if (overflowSeen != 0 && less(arr[i], tmp[bandEnd - 1])) {
                    runStart.push_back(bandEnd);
                }
                tmp[bandEnd] = arr[i];
            } else {
                std::size_t pos = bandEnd;
                while (pos > bandStart && less(arr[i], tmp[pos - 1])) {
                    tmp[pos] = std::move(tmp[pos - 1]);
                    --pos;
                }
                tmp[pos] = arr[i];
            }

            ++overflowSeen;
            continue;
        }

        const bool desc = simulated_direct_merge::isDescTag(tag);
        const std::size_t local =
            static_cast<std::size_t>(simulated_direct_merge::localPileId(tag));
        if (desc) tmp[--descCounts[local]] = arr[i];
        else tmp[ascCounts[local]++] = arr[i];
    }
    assert(overflowSeen == overflowCount);

    if (useBidirectionalNaturalRuns) {
        if (overflowCount != 0 && naturalStart < n) {
            if (fallbackBand) {
                // A single undersized tail is acceptable; sort it once.
                insertionSortRange(naturalStart, n);
                runStart.push_back(n);
            } else {
                finalizeNaturalRun(naturalStart, n, naturalDirection);
            }
        }
    } else if (useNaturalOverflowRuns) {
        if (overflowCount != 0) runStart.push_back(n);
    } else {
        for (std::size_t end = normalCount + std::min(BandSize, overflowCount);
             end <= n && end > normalCount;
             end += BandSize) {
            runStart.push_back(end);
            if (end == n) break;
            if (n - end < BandSize) end = n - BandSize;
        }
    }
    return runStart;
}

// Public frozen-live entry point: early freeze with live-sorted overflow bands.
template <typename T, typename Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge, bool naturalRunRoute = false, bool enableSpecializedRoutes = false, bool enableCoherentValuePileCache = true, bool enableBidirectionalNaturalOverflow = false, bool enableAdaptiveOverflowRouter = false, bool enableValleyRescue = false, bool enableExactBalanceRoute = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_early_freeze_live_direct_merge::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_early_freeze_live_direct_merge::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) return;
    if constexpr (simulated_direct_merge::specializedIntegralEligible<T, Less>) {
        if (enableSpecializedRoutes) {
            bool prefixMayMix = true;
            if (arr.size() >= 4) {
                const bool asc = less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
                const bool desc = less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
                prefixMayMix = !(asc || desc);
            }
            if (prefixMayMix && simulated_direct_merge::trySpecializedPrePatienceRoutes(arr, less)) return;
        }
    }
    auto sim = simulated_early_freeze_direct_merge::simulatePatienceInsertionBlueprintEarlyFreeze(
        arr, less, simulated_early_freeze_direct_merge::FreezePolicy::power2_broad, naturalRunRoute, true, enableCoherentValuePileCache, 50, enableAdaptiveOverflowRouter, enableValleyRescue);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }
    bool useRandomBranchlessMerge = false;
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 96) {
        useRandomBranchlessMerge =
            simulated_early_freeze_direct_merge::frozenInsertionLooksRandomLike(
                sim, arr.size());
    }

    // E179 geometry-first STRICT multi-tier router. For <=64 piles, strong
    // asc/desc pile-count dominance routes directly without overflow sampling;
    // ambiguous low-pile and qualifying high-pile cases retain the STRICT sample.
    // The legacy E178 path remains available behind enableAdaptiveOverflowRouter=false.
    const std::size_t finalPileCount = sim.ascCounts.size() + sim.descCounts.size();
    bool useAscendingNaturalOverflow = false;
    bool useBidirectionalNaturalOverflow = false;
    if (enableAdaptiveOverflowRouter) {
        const std::size_t ascPileCount = sim.ascCounts.size();
        const std::size_t descPileCount = sim.descCounts.size();
        const bool lowPileAscObvious = finalPileCount <= 64 &&
            ascPileCount >= 3 * std::max<std::size_t>(descPileCount, 1);
        const bool lowPileDescObvious = finalPileCount <= 64 &&
            descPileCount >= 3 * std::max<std::size_t>(ascPileCount, 1);
        const bool strongSample = sim.overflowCount != 0 &&
            sim.overflowSampleCount == 32 &&
            sim.overflowSampleDirectionChanges <= 1 &&
            sim.overflowSampleLongest >= 24;
        if (sim.overflowCount != 0 && lowPileAscObvious) {
            useAscendingNaturalOverflow = true;
        } else if (sim.overflowCount != 0 && lowPileDescObvious) {
            useBidirectionalNaturalOverflow = true;
        } else if (enableExactBalanceRoute && sim.overflowCount != 0 &&
                   finalPileCount <= 64 && ascPileCount == descPileCount) {
            useBidirectionalNaturalOverflow = true;
        } else if (strongSample && finalPileCount <= 64) {
            if (sim.overflowSampleAscAdj >= sim.overflowSampleDescAdj)
                useAscendingNaturalOverflow = true;
            else
                useBidirectionalNaturalOverflow = true;
        } else if (strongSample &&
                   sim.overflowCount * 100 >= arr.size() * 2 &&
                   static_cast<long double>(finalPileCount) <=
                       0.006L * static_cast<long double>(arr.size())) {
            useBidirectionalNaturalOverflow = true;
        }
    } else {
        const bool legacyNatural = sim.overflowCount != 0 && finalPileCount <= 64;
        useAscendingNaturalOverflow = legacyNatural && !enableBidirectionalNaturalOverflow;
        useBidirectionalNaturalOverflow = legacyNatural && enableBidirectionalNaturalOverflow;
    }

    std::vector<T> tmp;
    auto runStart = reconstructFrozenBlueprintWithLiveOverflowBands(
        arr, sim.blueprint, std::move(sim.ascCounts), std::move(sim.descCounts),
        sim.overflowCount, tmp, less, 32,
        useAscendingNaturalOverflow,
        useBidirectionalNaturalOverflow,
        32);
    if (useRandomBranchlessMerge) {
        simulated_direct_merge::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less,
            simulated_direct_merge::MergeSchedule::adjacent_pairs, true, 7,
            bidirectionalBranchlessMerge);
    } else if (detail::shouldUseTimSortMerge(runStart, arr.size())) {
        detail::mergeRunsTimSortStyle(
            tmp, arr, std::move(runStart), less);
    } else {
        simulated_direct_merge::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less);
    }
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    if (arr.size() >= 10000 && jessesort::simulated_direct_merge::tryLongAscendingNaturalRunDirect(arr, less)) return;
    // E237: propagate only the mathematically proved two-lane zigzag shortcut
    // to this compatible allocating direct variation. Keep small-N pillar
    // behavior untouched; the two-comparison gate avoids the noinline proof
    // helper unless the first two adjacent directions could be a strict zigzag.
    if (arr.size() >= 32768 &&
        (less(arr[0], arr[1]) != less(arr[1], arr[2])) &&
        jessesort::simulated_direct_merge::tryTwoLaneZigzagDirect(arr, less)) return;
    sortImpl(arr, less, true, true, true, true, false, true, true, true);
}

} // namespace jessesort::simulated_early_freeze_live_direct_merge
#endif
