#ifndef JESSESORT_SIMULATED_INPLACE_FLATTEN_HPP
#define JESSESORT_SIMULATED_INPLACE_FLATTEN_HPP

#include <jessesort/v2_simulated.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_inplace_flatten {

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
                jessesort::simulated::localPileId(tag));
            const std::size_t destination =
                jessesort::simulated::isDescTag(tag)
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
                jessesort::simulated::localPileId(tag));
            destination[i] = jessesort::simulated::isDescTag(tag)
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

// Public V3 entry point: V2-style simulated insertion with in-place flattening.
template <typename T, typename Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge, bool naturalRunRoute = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_inplace_flatten::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_inplace_flatten::sort requires movable values for permutation swaps and merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) {
        return;
    }

    std::vector<std::size_t> runStart;
    bool branchlessRandomMerge = false;
    {
        jessesort::simulated::SimulatedInsertionResult<T> sim =
            jessesort::simulated::simulatePatienceInsertionBlueprint(arr, less, true, naturalRunRoute);

        if (sim.alreadySortedAscending) {
            return;
        }
        if (sim.reverseSortedDescending) {
            std::reverse(arr.begin(), arr.end());
            return;
        }

        const std::size_t finalPileCount =
            sim.ascCounts.size() + sim.descCounts.size();
        __extension__ typedef unsigned __int128 Wide;
        const Wide finalPileSquare =
            static_cast<Wide>(finalPileCount) * finalPileCount;
        // E109 convergence baseline: V1/V2/V3 share 3.5 / 4.25 / 5.0.
        // The small-size 3.5 threshold is cherry-picked from the more recent
        // V1 evidence; mid/large thresholds already agree with revised E102.
        bool densitySelectsBranchless = false;
        if (arr.size() <= 20000) {
            densitySelectsBranchless =
                static_cast<Wide>(2) * finalPileSquare >=
                    static_cast<Wide>(7) * arr.size();
        } else if (arr.size() < 500000) {
            densitySelectsBranchless =
                static_cast<Wide>(4) * finalPileSquare >=
                static_cast<Wide>(17) * arr.size();
        } else {
            densitySelectsBranchless =
                finalPileSquare >= static_cast<Wide>(5) * arr.size();
        }
        branchlessRandomMerge =
            arr.size() >= 10000 &&
            sim.earlyRandomLike &&
            densitySelectsBranchless &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 96;

        runStart = flattenTaggedBlueprintInPlace(
            arr,
            sim.blueprint,
            std::move(sim.ascCounts),
            std::move(sim.descCounts));
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
    jessesort::simulated::mergeRunsAdjacentPairsEnds(
        tmp, arr, ends, less, branchlessRandomMerge, bidirectionalBranchlessMerge);
    arr = std::move(tmp);
}

// Public V3 entry point. E087 retains E085's bidirectional branchless kernel
// only after a V3-specific causal A/B; sortImpl remains available internally
// so the benchmark can compare the old E082 route in the same process.
template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    sortImpl(arr, less, true, true);
}

} // namespace jessesort::simulated_inplace_flatten

#endif
