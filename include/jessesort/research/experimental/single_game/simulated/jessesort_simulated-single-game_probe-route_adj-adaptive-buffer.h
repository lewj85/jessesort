#ifndef JESSESORT_SIMULATED_SINGLE_GAME_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_SIMULATED_SINGLE_GAME_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/detail/pipelines/reference/simulated.h>

namespace jessesort::simulated_single_game {

// E468 baseline: preserve the maintained simulated family's front-end value routes,
// blueprint reconstruction, and adjacent merge backend, but replace dual-direction
// Patience simulation with one source-order ascending game. No reverse traversal,
// front insertion, freeze logic, or single-game-specific structural rescue is used.

template <typename T, typename Less = std::less<T>>
simulated_legacy::SimulatedInsertionResult<T> simulateSingleGameBlueprint(
    const std::vector<T>& arr, Less less = Less{}) {
    const std::size_t n = arr.size();
    simulated_legacy::SimulatedInsertionResult<T> result;
    if (n == 0) return result;

    enum class PrefixDirection { Unknown, Ascending, Descending };
    PrefixDirection prefixDirection = PrefixDirection::Unknown;
    std::size_t prefixEnd = 1;
    for (; prefixEnd < n; ++prefixEnd) {
        const T& previous = arr[prefixEnd - 1];
        const T& value = arr[prefixEnd];
        if (prefixDirection == PrefixDirection::Ascending) {
            if (less(value, previous)) break;
        } else if (prefixDirection == PrefixDirection::Descending) {
            if (less(previous, value)) break;
        } else if (less(previous, value)) {
            prefixDirection = PrefixDirection::Ascending;
        } else if (less(value, previous)) {
            prefixDirection = PrefixDirection::Descending;
        }
    }

    if (prefixEnd == n) {
        result.alreadySortedAscending = prefixDirection != PrefixDirection::Descending;
        result.reverseSortedDescending = prefixDirection == PrefixDirection::Descending;
        return result;
    }

    // Important: SimulatedInsertionResult defaults both monotone flags true because
    // the production simulator sets them after its prefix scan. A non-monotone
    // single-game simulation must explicitly clear them before reconstruction.
    result.alreadySortedAscending = false;
    result.reverseSortedDescending = false;
    result.blueprint.resize(n);
    const std::size_t reservePiles = simulated_legacy::estimatePileReserve(n);
    result.ascCounts.reserve(reservePiles);
    result.ascTails.reserve(reservePiles);

    std::size_t lastPileIndexAscending = 0;
    result.ascTails.push_back(arr[0]);
    result.ascCounts.push_back(1);
    result.blueprint[0] = simulated_legacy::makeAscTag(0);

    // Keep the baseline deliberately literal: every remaining value is inserted
    // into the same ascending game, regardless of source-order direction.
    for (std::size_t i = 1; i < n; ++i) {
        simulated_legacy::simulateInsertValueAscendingPiles(
            result.ascTails, lastPileIndexAscending, arr[i], i,
            result.blueprint, result.ascCounts, less);
        if (i == 63) {
            // Preserve the downstream random-merge selector with a one-game analogue
            // of the production probe's ~12 total-pile threshold. This does not alter
            // pile ownership or insertion behavior.
            result.earlyRandomLike = result.ascCounts.size() >= 12;
        }
    }
    if (n < 64)
        result.earlyRandomLike = result.ascCounts.size() >= 12;
    return result;
}

template <typename T, typename Less = std::less<T>>
void sortImplCore(std::vector<T>& arr, Less less,
                  bool enableLowCardinalityDirect = true,
                  bool enableDominantValueDirect = true,
                  bool enableHighEntropyPartition = true) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>);
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>);
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>);

    if (arr.size() < 2) return;

    bool specialValuePrefixMayMix = true;
    if (arr.size() >= 4) {
        const bool firstThreeAscending =
            less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
        const bool firstThreeDescending =
            less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
        specialValuePrefixMayMix = !(firstThreeAscending || firstThreeDescending);
    }

    if constexpr (simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (enableDominantValueDirect && specialValuePrefixMayMix) {
            T dominant = arr[0];
            if (simulated_legacy::dominantValueSampleCandidate(arr, dominant, less)) {
                simulated_legacy::highEntropyQuickSort(
                    arr.data(), arr.size(),
                    2 * static_cast<int>(std::bit_width(arr.size())), less,
                    false, T{}, false, true, nullptr, false);
                return;
            }
        }

        if (enableLowCardinalityDirect && specialValuePrefixMayMix &&
            simulated_legacy::lowCardinalityDirectionGate(arr, less) &&
            simulated_legacy::lowCardinalitySampleCandidate(arr, less) &&
            simulated_legacy::trySortLowCardinalityDirectConfirmed(arr, less)) {
            return;
        }

        bool highEntropyPrefixAlternates = false;
        if (enableHighEntropyPartition && specialValuePrefixMayMix && arr.size() >= 8) {
            int previousDirection = 0;
            highEntropyPrefixAlternates = true;
            for (std::size_t i = 1; i < 8; ++i) {
                int direction = 0;
                if (less(arr[i - 1], arr[i])) direction = 1;
                else if (less(arr[i], arr[i - 1])) direction = -1;
                if (direction == 0 ||
                    (previousDirection != 0 && direction == previousDirection)) {
                    highEntropyPrefixAlternates = false;
                    break;
                }
                previousDirection = direction;
            }
        }
        if (enableHighEntropyPartition && specialValuePrefixMayMix &&
            !highEntropyPrefixAlternates &&
            simulated_legacy::trySortHighEntropyPartitionDirect(arr, less)) {
            return;
        }
    }

    auto sim = simulateSingleGameBlueprint(arr, less);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    const bool useRandomBranchlessMerge =
        simulated_legacy::shouldUseRandomBranchlessMerge(sim, arr.size());

    std::vector<T> tmp;
    std::vector<std::size_t> runStart =
        simulated_legacy::reconstructTaggedBlueprintNormalizedForSimulated(
            arr,
            std::move(sim.blueprint),
            std::move(sim.ascCounts),
            {},
            tmp,
            false, // single ascending-game piles preserve encounter order
            true,
            true,
            true);

    std::vector<std::size_t> ends(runStart.begin() + 1, runStart.end());
    simulated_legacy::mergeRunsAdjacentPairsEnds(
        tmp, arr, ends, less, useRandomBranchlessMerge, true);
    arr = std::move(tmp);
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    sortImplCore(arr, less, true, true, true);
}

} // namespace jessesort::simulated_single_game

#endif
