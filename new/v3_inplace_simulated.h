#ifndef JESSESORT_SIMULATED_INPLACE_FLATTEN_HPP
#define JESSESORT_SIMULATED_INPLACE_FLATTEN_HPP

#include "v2_simulated.h"

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
// V3 materializes ascending-game runs first, followed by descending-game
// runs. This ordering is local to the in-place flattening strategy:
//   [ascending pile 0 ... ascending pile A-1]
//   [descending pile 0 ... descending pile D-1]
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
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        runStart[p] = out;
        out += ascCounts[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t run = numAscPiles + p;
        runStart[run] = out;
        out += descCounts[p];
    }
    runStart[numRuns] = out;
    assert(out == n);

    // Reuse count storage as destination cursors.
    // Ascending piles fill left-to-right; descending piles fill right-to-left.
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        ascCounts[p] = runStart[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t run = numAscPiles + p;
        descCounts[p] = runStart[run + 1];
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
void sort(std::vector<T>& arr, Less less = Less{}) {
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
            jessesort::simulated::simulatePatienceInsertionBlueprint(arr, less, true);

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
        branchlessRandomMerge =
            arr.size() >= 10000 &&
            sim.earlyRandomLike &&
            static_cast<Wide>(finalPileCount) * finalPileCount >=
                static_cast<Wide>(5) * arr.size() &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 2 * sizeof(void*);

        runStart = flattenTaggedBlueprintInPlace(
            arr,
            sim.blueprint,
            std::move(sim.ascCounts),
            std::move(sim.descCounts));
    } // Release blueprint and pile-tail metadata before allocating merge storage.

    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) {
        tmp.resize(arr.size());
        // The shared merge implementation expects reconstructed runs in tmp and
        // writable slots in arr. Swapping the vectors is O(1) and preserves the
        // in-place flattening result as the first merge source.
        arr.swap(tmp);
    } else {
        // Valid destination objects are required for assignment-based merging.
        // Copy construction preserves support for deleted default constructors.
        tmp = arr;
    }

    // V3 reconstructs the same ordered run layout as V2, but its cycle-based
    // flattening has a different cost profile. Direct A/B testing showed that
    // V2's selected PowerSort policy still provides repeatable gains for sparse
    // noise and sufficiently imbalanced compact run sets without affecting
    // balanced inputs. Keep the selection local to V3 rather than routing
    // through jessesort::simulated::sort so flattening remains isolated.
    if (jessesort::simulated::detail::shouldUsePowerSortMerge(
            runStart, arr.size())) {
        jessesort::simulated::detail::mergeRunsPowerSortStyle(
            tmp,
            arr,
            std::move(runStart),
            less);
    } else {
        jessesort::simulated::mergeRunsFromTmpToArr(
            tmp,
            arr,
            std::move(runStart),
            less,
            jessesort::simulated::MergeSchedule::adjacent_pairs,
            branchlessRandomMerge);
    }
}

} // namespace jessesort::simulated_inplace_flatten

#endif
