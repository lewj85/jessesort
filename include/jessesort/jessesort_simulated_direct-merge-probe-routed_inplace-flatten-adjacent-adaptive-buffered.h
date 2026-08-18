#ifndef JESSESORT_E229_JESSESORT_SIMULATED_DIRECT_MERGE_PROBE_ROUTED_INPLACE_FLATTEN_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_E229_JESSESORT_SIMULATED_DIRECT_MERGE_PROBE_ROUTED_INPLACE_FLATTEN_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/tiny_sort.h>
#include <jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_inplace_flatten_direct_merge {

// Convert the packed pile blueprint into a source-index -> destination-index
// permutation, then resolve that permutation in place with cycle swaps.
//
// E109 convergence baseline: materialize descending-game runs first, then
// ascending-game runs, matching V1/V2.  The in-place permutation remains V3's
// distinguishing flattening mechanism; only the shared run ordering is aligned.
//   [descending pile 0 ... descending pile D-1]
//   [ascending pile 0 ... ascending pile A-1]
//
// Ascending-game piles preserve encounter order. Descending-game piles are
// reversed so every reconstructed pile is ascending under the comparator.
template <typename T>
std::vector<std::size_t> flattenTaggedBlueprintInPlace(
    std::vector<T>& arr,
    std::vector<std::uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts
) {
    const std::size_t n = arr.size();
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numRuns = numAscPiles + numDescPiles;

    assert(blueprint.size() == n);

    std::vector<std::size_t> runStart(numRuns + 1, 0);
    std::size_t out = 0;
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        runStart[p] = out;
        out += descCounts[p];
    }
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t run = numDescPiles + p;
        runStart[run] = out;
        out += ascCounts[p];
    }
    runStart[numRuns] = out;
    assert(out == n);

    // Reuse count storage as destination cursors.
    // Ascending piles fill left-to-right; descending piles fill right-to-left.
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        descCounts[p] = runStart[p + 1];
    }
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t run = numDescPiles + p;
        ascCounts[p] = runStart[run];
    }

    if (n <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        // The packed blueprint is no longer needed after this pass, so overwrite
        // it with destination indices and use it directly as the permutation.
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t tag = blueprint[i];
            const std::size_t local = static_cast<std::size_t>(
                jessesort::simulated_direct_merge::localPileId(tag));
            const std::size_t destination =
                jessesort::simulated_direct_merge::isDescTag(tag)
                    ? --descCounts[local]
                    : ascCounts[local]++;
            blueprint[i] = static_cast<std::uint32_t>(destination);
        }

        for (std::size_t i = 0; i < n; ++i) {
            while (static_cast<std::size_t>(blueprint[i]) != i) {
                const std::size_t j = static_cast<std::size_t>(blueprint[i]);
                assert(j < n);
                using std::swap;
                swap(arr[i], arr[j]);
                swap(blueprint[i], blueprint[j]);
            }
        }
    } else {
        // Practical vectors will almost always use the compact path. Retain a
        // size_t fallback so the public algorithm is not artificially capped at
        // 2^32 elements by its temporary permutation representation.
        std::vector<std::size_t> destination(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t tag = blueprint[i];
            const std::size_t local = static_cast<std::size_t>(
                jessesort::simulated_direct_merge::localPileId(tag));
            destination[i] = jessesort::simulated_direct_merge::isDescTag(tag)
                ? --descCounts[local]
                : ascCounts[local]++;
        }

        for (std::size_t i = 0; i < n; ++i) {
            while (destination[i] != i) {
                const std::size_t j = destination[i];
                assert(j < n);
                using std::swap;
                swap(arr[i], arr[j]);
                swap(destination[i], destination[j]);
            }
        }
    }

    return runStart;
}


// E133/E156 audit: V3 deliberately shares V2's specialized pre-Patience
// router. Keep the policy in one implementation so later V2 routing changes
// cannot silently fail to propagate to V3.

// Public V3 entry point: V2-style simulated insertion with in-place flattening.
template <typename T, typename Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge, bool naturalRunRoute = false, bool enableSpecializedRoutes = false, bool enableCoherentValuePileCache = true) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_inplace_flatten_direct_merge::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_inplace_flatten_direct_merge::sort requires movable values for permutation swaps and merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) {
        return;
    }

    if constexpr (jessesort::simulated_direct_merge::specializedIntegralEligible<T, Less>) {
        if (enableSpecializedRoutes) {
            bool specialValuePrefixMayMix = true;
            if (arr.size() >= 4) {
                const bool firstThreeAscending =
                    less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
                const bool firstThreeDescending =
                    less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
                specialValuePrefixMayMix = !(firstThreeAscending || firstThreeDescending);
            }
            if (specialValuePrefixMayMix && jessesort::simulated_direct_merge::trySpecializedPrePatienceRoutes(arr, less)) {
                return;
            }
        }
    }

    std::vector<std::size_t> runStart;
    bool branchlessRandomMerge = false;
    {
        jessesort::simulated_direct_merge::SimulatedInsertionResult<T> sim =
            jessesort::simulated_direct_merge::simulatePatienceInsertionBlueprint(
                arr, less, true, naturalRunRoute, enableCoherentValuePileCache);

        if (sim.alreadySortedAscending) return;
        if (sim.reverseSortedDescending) {
            std::reverse(arr.begin(), arr.end());
            return;
        }

        branchlessRandomMerge =
            jessesort::simulated_direct_merge::shouldUseRandomBranchlessMerge(sim, arr.size());

        // V3's defining difference from V2: resolve the exact same simulated
        // blueprint into the same run order by permutation-cycle flattening
        // directly in arr instead of V2's streaming reconstruction into tmp.
        runStart = flattenTaggedBlueprintInPlace(
            arr, sim.blueprint, std::move(sim.ascCounts), std::move(sim.descCounts));
    } // Release blueprint and pile-tail metadata before allocating merge storage.

    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) {
        tmp.resize(arr.size());
        arr.swap(tmp);
    } else {
        tmp = arr;
    }

    // E115: V1-V3 share the ends-based adjacent-pair merge driver.
    // The driver computes its own E045 gallop trigger from final run lengths.
    // E111: the reconciled V1/V2/V3 revalidation no longer supports the
    // selective PowerSort path here. Keep the current adjacent-pair scheduler.
    std::vector<std::size_t> ends(runStart.begin() + 1, runStart.end());
    jessesort::simulated_direct_merge::mergeRunsAdjacentPairsEnds(
        tmp, arr, ends, less, branchlessRandomMerge, bidirectionalBranchlessMerge);
    arr = std::move(tmp);
}

// Public V3 entry point. E087 retains E085's bidirectional branchless kernel
// only after a V3-specific causal A/B; sortImpl remains available internally
// so the benchmark can compare the old E082 route in the same process.
template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    if (arr.size() >= 10000 && jessesort::simulated_direct_merge::tryLongAscendingNaturalRunDirect(arr, less)) return;
    sortImpl(arr, less, true, true, true, true);
}

} // namespace jessesort::simulated_inplace_flatten_direct_merge

#endif
