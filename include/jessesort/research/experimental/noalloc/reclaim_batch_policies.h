#pragma once

#include <jessesort/detail/pipelines/reference/noalloc_direct.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <vector>

namespace jessesort::experimental::noalloc_reclaim_batch {

using RunDesc = allocation_free_bounded::RunDesc;
inline constexpr std::size_t kMaxRuns = allocation_free_low_run::kMaxRuns;

enum class Policy { LiveSmallest, AdjacentPairs, ShortestTwoOfThree };

template <class T, class Less>
void mergeRuns(std::vector<T>& a, RunDesc left, RunDesc right, Less less,
               RunDesc& output) {
    allocation_free_low_run::normalizeRunNoAlloc(a, left, less);
    allocation_free_low_run::normalizeRunNoAlloc(a, right, less);
    allocation_free_low_run::mergeRecursiveNoAlloc(
        a.begin() + static_cast<std::ptrdiff_t>(left.begin),
        a.begin() + static_cast<std::ptrdiff_t>(left.end),
        a.begin() + static_cast<std::ptrdiff_t>(right.end), less);
    output = {left.begin, right.end, false};
}

struct AdjacentPairsPolicy {
    template <class T, class Less>
    static void reclaim(std::vector<T>& a, std::array<RunDesc, kMaxRuns>& runs,
                        std::size_t& runCount, Less less) {
        std::size_t out = 0;
        for (std::size_t r = 0; r + 1 < runCount; r += 2)
            mergeRuns(a, runs[r], runs[r + 1], less, runs[out++]);
        if (runCount & 1u) runs[out++] = runs[runCount - 1];
        runCount = out;
    }
};

struct ShortestTwoOfThreePolicy {
    template <class T, class Less>
    static void reclaim(std::vector<T>& a, std::array<RunDesc, kMaxRuns>& runs,
                        std::size_t& runCount, Less less) {
        std::array<RunDesc, kMaxRuns> next{};
        std::size_t out = 0;
        std::size_t r = 0;
        for (; r + 2 < runCount; r += 3) {
            const std::size_t ab = runs[r + 1].end - runs[r].begin;
            const std::size_t bc = runs[r + 2].end - runs[r + 1].begin;
            if (ab <= bc) {
                mergeRuns(a, runs[r], runs[r + 1], less, next[out++]);
                next[out++] = runs[r + 2];
            } else {
                next[out++] = runs[r];
                mergeRuns(a, runs[r + 1], runs[r + 2], less, next[out++]);
            }
        }
        while (r < runCount) next[out++] = runs[r++];
        std::copy_n(next.begin(), out, runs.begin());
        runCount = out;
    }
};

template <class T, class Less>
void handleOverflow(std::vector<T>& a, std::array<RunDesc, kMaxRuns>& runs,
                    std::size_t& runCount, Less less, Policy policy) {
    constexpr std::size_t kMinAverageRunForReclamation = 1024;
    const auto first65 = allocation_free_low_run::profileFirst65Runs(a, less);
    if (first65.span < (kMaxRuns + 1) * kMinAverageRunForReclamation ||
        first65.allOverlap) {
        allocation_free_bounded::fallbackNoAlloc(a, less);
        return;
    }

    runCount = 0;
    std::size_t i = 0;
    while (i < a.size()) {
        const std::size_t begin = i;
        bool descending = false;
        if (i + 1 == a.size()) {
            i = a.size();
        } else {
            descending = less(a[i + 1], a[i]);
            i += 2;
            if (descending) {
                while (i < a.size() && less(a[i], a[i - 1])) ++i;
            } else {
                while (i < a.size() && !less(a[i], a[i - 1])) ++i;
            }
        }
        if (runCount == kMaxRuns) {
            switch (policy) {
                case Policy::LiveSmallest:
                    allocation_free_low_run::reclaimSmallestAdjacentRun(
                        a, runs, runCount, less);
                    break;
                case Policy::AdjacentPairs:
                    AdjacentPairsPolicy::reclaim(a, runs, runCount, less);
                    break;
                case Policy::ShortestTwoOfThree:
                    ShortestTwoOfThreePolicy::reclaim(a, runs, runCount, less);
                    break;
            }
        }
        runs[runCount++] = {begin, i, descending};
    }
    allocation_free_low_run::mergeNaturalRunsNoAlloc(a, runs, runCount, less);
}

template <class T, class Less = std::less<T>>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void sort(std::vector<T>& a, Less less, Policy policy) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>);
    if (a.size() < 2) return;
    if (detail::tryTinyInsertionSort(a, less)) return;

    bool specialValuePrefixMayMix = true;
    if (a.size() >= 4) {
        const bool aa = less(a[0], a[1]) && less(a[1], a[2]) && less(a[2], a[3]);
        const bool dd = less(a[1], a[0]) && less(a[2], a[1]) && less(a[3], a[2]);
        specialValuePrefixMayMix = !(aa || dd);
    }
    if constexpr (simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (specialValuePrefixMayMix &&
            simulated_legacy::trySpecializedPrePatienceRoutes(a, less)) return;
    }
    if (allocation_free_low_run::tryEarlyRepeatedOverlapRoute(a, less, nullptr)) return;

    if (a.size() >= 10000) {
        const bool inv1 = less(a[1], a[0]);
        const bool inv2 = less(a[2], a[1]);
        if (inv1 != inv2) {
            if (allocation_free_direct::tryTwoLaneZigzagDirectNoAlloc(a, less)) return;
        } else if (!inv1) {
            if (allocation_free_direct::tryLongNaturalRunDirectNoAlloc(a, less)) return;
        }
        if (allocation_free_direct::trySparseDistributedDisorderDirectNoAlloc(a, less)) return;
    }

    const auto classified =
        allocation_free_low_run::classifySharedBoundedPatience(a, less, nullptr);
    if (classified == allocation_free_low_run::ClassifyResult::SortedAscending) return;
    if (classified == allocation_free_low_run::ClassifyResult::SortedDescending) {
        std::reverse(a.begin(), a.end());
        return;
    }
    if (classified != allocation_free_low_run::ClassifyResult::Accept) {
        allocation_free_bounded::fallbackNoAlloc(a, less);
        return;
    }

    std::array<RunDesc, kMaxRuns> runs{};
    std::size_t runCount = 0;
    if (!allocation_free_bounded::discoverNaturalRuns(a, runs, runCount, less)) {
        handleOverflow(a, runs, runCount, less, policy);
        return;
    }
    if (allocation_free_bounded::cheapMergeGeometryOnly(a, runs, runCount, less)) {
        allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a, runs, runCount, less);
        return;
    }
    if (allocation_free_low_run::repeatedOverlapGeometry(a, runs, runCount, less)) {
        allocation_free_bounded::fallbackNoAlloc(a, less);
        return;
    }
    allocation_free_low_run::mergeNaturalRunsNoAlloc(a, runs, runCount, less);
}

} // namespace jessesort::experimental::noalloc_reclaim_batch

namespace jessesort::experimental::noalloc_reclaim_live_control {
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    noalloc_reclaim_batch::sort(
        a, less, noalloc_reclaim_batch::Policy::LiveSmallest);
}
} // namespace jessesort::experimental::noalloc_reclaim_live_control

namespace jessesort::experimental::noalloc_reclaim_adjacent_pairs {
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    noalloc_reclaim_batch::sort(
        a, less, noalloc_reclaim_batch::Policy::AdjacentPairs);
}
} // namespace jessesort::experimental::noalloc_reclaim_adjacent_pairs

namespace jessesort::experimental::noalloc_reclaim_shortest_two_of_three {
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    noalloc_reclaim_batch::sort(
        a, less, noalloc_reclaim_batch::Policy::ShortestTwoOfThree);
}
} // namespace jessesort::experimental::noalloc_reclaim_shortest_two_of_three
