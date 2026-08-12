#ifndef JESSESORT_SIMULATED_EARLY_FREEZE_SINGLE_OVERFLOW_HPP
#define JESSESORT_SIMULATED_EARLY_FREEZE_SINGLE_OVERFLOW_HPP

#include <jessesort/v5_deferred_bands.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_early_freeze_single_overflow {


namespace detail {

// The single-overflow layout benefits from PowerSort when the normal piles
// form a moderately sparse run set and the terminal overflow run remains a
// small fraction of the input. Random input is deliberately rejected because
// its overflow run is typically much larger, while dense-noise inputs are
// rejected by the average-run-length condition.
inline bool shouldUsePowerSortMerge(
    const std::vector<std::size_t>& runStart,
    std::size_t n,
    std::size_t overflowCount
) {
    if (n < 32768 || runStart.size() <= 16) return false;

    const std::size_t runCount = runStart.size() - 1;
    __extension__ typedef unsigned __int128 Wide;
    return
        static_cast<Wide>(runCount) * 25 <= n &&
        static_cast<Wide>(overflowCount) * 10 <= n;
}

} // namespace detail

// Early-freeze variation with one deferred overflow run. All overflow values are
// gathered contiguously during reconstruction, then the complete overflow
// region is sorted once with std::sort and exposed as exactly one merge run.
template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithSingleOverflowRun(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::size_t overflowCount,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    const std::size_t n = arr.size();
    const std::size_t normalCount = n - overflowCount;
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numNormalRuns = numAscPiles + numDescPiles;

    std::vector<std::size_t> runStart;
    runStart.reserve(numNormalRuns + (overflowCount != 0 ? 2 : 1));
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
            tmp[normalCount + overflowSeen] = arr[i];
            ++overflowSeen;
            continue;
        }

        const bool desc = simulated::isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(simulated::localPileId(tag));
        if (desc) tmp[--descCounts[local]] = arr[i];
        else tmp[ascCounts[local]++] = arr[i];
    }
    assert(overflowSeen == overflowCount);

    if (overflowCount != 0) {
        std::sort(
            tmp.begin() + static_cast<std::ptrdiff_t>(normalCount),
            tmp.end(),
            less);
        runStart.push_back(n);
    }
    return runStart;
}

// Public V4 entry point: early freeze with one deferred overflow run.
template <typename T, typename Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge, bool naturalRunRoute = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_early_freeze_single_overflow::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_early_freeze_single_overflow::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) return;
    auto sim = simulated_early_freeze::simulatePatienceInsertionBlueprintEarlyFreeze(
        arr, less, simulated_early_freeze::FreezePolicy::power2_single_overflow, naturalRunRoute);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    bool useRandomBranchlessMerge = false;
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 96) {
        useRandomBranchlessMerge =
            simulated_early_freeze::frozenInsertionLooksRandomLike(
                sim, arr.size());
    }

    std::vector<T> tmp;
    auto runStart = reconstructFrozenBlueprintWithSingleOverflowRun(
        arr, sim.blueprint, std::move(sim.ascCounts), std::move(sim.descCounts),
        sim.overflowCount, tmp, less);
    if (useRandomBranchlessMerge) {
        simulated::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less,
            simulated::MergeSchedule::adjacent_pairs, true, 7,
            bidirectionalBranchlessMerge);
    } else if (detail::shouldUsePowerSortMerge(
                   runStart, arr.size(), sim.overflowCount)) {
        simulated::detail::mergeRunsPowerSortStyle(
            tmp, arr, std::move(runStart), less);
    } else {
        simulated::mergeRunsFromTmpToArr(
            tmp, arr, std::move(runStart), less);
    }
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    sortImpl(arr, less, true, true);
}

} // namespace jessesort::simulated_early_freeze_single_overflow

#endif
