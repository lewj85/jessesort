#ifndef JESSESORT_E229_JESSESORT_SIMULATED_FROZEN_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_DEFERRED_BANDS32_H
#define JESSESORT_E229_JESSESORT_SIMULATED_FROZEN_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_DEFERRED_BANDS32_H

#include <jessesort/detail/decomposition/frozen/early_freeze_core.h>

namespace jessesort::simulated_early_freeze_legacy {

template <typename T, typename Less>
inline void e090Bitonic32(T* x, Less less) {
    auto ce = [&](T& a, T& b) {
        const bool swap = less(b, a);
        T lo = swap ? b : a;
        T hi = swap ? a : b;
        a = std::move(lo);
        b = std::move(hi);
    };
    for (unsigned k = 2; k <= 32; k <<= 1) {
        for (unsigned j = k >> 1; j > 0; j >>= 1) {
            for (unsigned i = 0; i < 32; ++i) {
                const unsigned l = i ^ j;
                if (l > i) {
                    if ((i & k) == 0) ce(x[i], x[l]);
                    else ce(x[l], x[i]);
                }
            }
        }
    }
}

template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithOverflowBands(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::size_t overflowCount,
    std::vector<T>& tmp,
    Less less = Less{},
    bool useE090SmallSort = false,
    bool patienceSortOverflow = false
) {
    constexpr std::size_t BandSize = 32;
    const std::size_t n = arr.size();
    const std::size_t normalCount = n - overflowCount;
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numNormalRuns = numAscPiles + numDescPiles;

    const std::size_t overflowRuns =
        (overflowCount + BandSize - 1) / BandSize;
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

    if constexpr (std::is_default_constructible_v<T>) {
        tmp.resize(n);
    } else {
        // Copy-construct valid slots for non-default-constructible values.
        // Reconstruction overwrites each slot exactly once afterward.
        tmp = arr;
    }

    // Reuse moved count-vector storage as reconstruction cursors.
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        ascCounts[p] = runStart[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        descCounts[p] = runStart[numAscPiles + p + 1];
    }

    // Gather overflow values contiguously during the blueprint scan. Sorting
    // each fixed-size band afterward avoids shifting up to 31 objects for every
    // overflow insertion while preserving the same ascending run boundaries.
    std::size_t overflowSeen = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == simulated_legacy::OVERFLOW_TAG) {
            tmp[normalCount + overflowSeen] = arr[i];
            ++overflowSeen;
            continue;
        }

        const bool desc = simulated_legacy::isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(simulated_legacy::localPileId(tag));
        if (desc) tmp[--descCounts[local]] = arr[i];
        else tmp[ascCounts[local]++] = arr[i];
    }
    assert(overflowSeen == overflowCount);

    if (overflowCount != 0 && patienceSortOverflow) {
        std::vector<T> overflow(
            tmp.begin() + static_cast<std::ptrdiff_t>(normalCount), tmp.end());
        simulated_legacy::sortImplCore(overflow, less, true, false, false, false);
        std::move(overflow.begin(), overflow.end(),
                  tmp.begin() + static_cast<std::ptrdiff_t>(normalCount));
        runStart.push_back(n);
    } else {
        for (std::size_t pos = normalCount; pos < n; pos += BandSize) {
            const std::size_t end = std::min(n, pos + BandSize);
            if (useE090SmallSort && end - pos == BandSize &&
                std::is_trivially_copyable_v<T>) {
                e090Bitonic32(tmp.data() + pos, less);
            } else {
                std::sort(
                    tmp.begin() + static_cast<std::ptrdiff_t>(pos),
                    tmp.begin() + static_cast<std::ptrdiff_t>(end),
                    less);
            }
            runStart.push_back(end);
        }
    }
    return runStart;
}



// Public frozen-deferred entry point: early freeze with deferred-sort overflow bands.
template <typename T, typename Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge, bool useE090SmallSort = false, bool naturalRunRoute = false, bool enableSpecializedRoutes = false, bool enableCoherentValuePileCache = true, bool patienceSortOverflow = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_early_freeze_legacy::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_early_freeze_legacy::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) return;
    if constexpr (simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (enableSpecializedRoutes) {
            bool prefixMayMix = true;
            if (arr.size() >= 4) {
                const bool asc = less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
                const bool desc = less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
                prefixMayMix = !(asc || desc);
            }
            if (prefixMayMix && simulated_legacy::trySpecializedPrePatienceRoutes(arr, less)) return;
        }
    }
    FrozenInsertionResult<T> sim =
        simulatePatienceInsertionBlueprintEarlyFreeze(
            arr, less, FreezePolicy::power2_broad, naturalRunRoute, true, enableCoherentValuePileCache, 50, false, true);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }
    bool useRandomBranchlessMerge = false;
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 96) {
        useRandomBranchlessMerge =
            frozenInsertionLooksRandomLike(sim, arr.size());
    }

    const bool routedPatienceOverflow = patienceSortOverflow &&
        arr.size() >= 50000 &&
        sim.overflowCount >= 4096 &&
        (sim.ascCounts.size() + sim.descCounts.size()) <= 4096;
    std::vector<T> tmp;
    std::vector<std::size_t> runStart =
        reconstructFrozenBlueprintWithOverflowBands(
            arr, sim.blueprint, std::move(sim.ascCounts),
            std::move(sim.descCounts), sim.overflowCount, tmp, less,
            useE090SmallSort, routedPatienceOverflow);
    simulated_legacy::mergeRunsFromTmpToArr(
        tmp, arr, std::move(runStart), less,
        simulated_legacy::MergeSchedule::defer_dominant_first_endpoint,
        useRandomBranchlessMerge, 7, bidirectionalBranchlessMerge);
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    sortImpl(arr, less, true, true, true, true, true, true);
}


} // namespace jessesort::simulated_early_freeze_legacy
#endif
