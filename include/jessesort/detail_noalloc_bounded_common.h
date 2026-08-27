#ifndef JESSESORT_DETAIL_NOALLOC_BOUNDED_COMMON_H
#define JESSESORT_DETAIL_NOALLOC_BOUNDED_COMMON_H

#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::allocation_free_bounded {

inline constexpr std::size_t kMaxRuns = 64;

struct RunDesc {
    std::size_t begin = 0;
    std::size_t end = 0;
    bool descending = false;
};

template <class T, class Less>
bool discoverNaturalRuns(const std::vector<T>& a,
                         std::array<RunDesc, kMaxRuns>& runs,
                         std::size_t& runCount,
                         Less less) {
    const std::size_t n = a.size();
    runCount = 0;
    std::size_t i = 0;
    while (i < n) {
        if (runCount == kMaxRuns) return false;
        const std::size_t begin = i;
        if (i + 1 == n) {
            runs[runCount++] = {begin, n, false};
            break;
        }
        const bool descending = less(a[i + 1], a[i]);
        i += 2;
        if (descending) {
            while (i < n && less(a[i], a[i - 1])) ++i;
        } else {
            while (i < n && !less(a[i], a[i - 1])) ++i;
        }
        runs[runCount++] = {begin, i, descending};
    }
    return true;
}

struct RunSummary {
    std::size_t minIndex = 0;
    std::size_t maxIndex = 0;
};

template <class T, class Less>
bool cheapMergeGeometryOnly(const std::vector<T>& a,
                            const std::array<RunDesc, kMaxRuns>& runs,
                            std::size_t runCount,
                            Less less) {
    std::array<RunSummary, kMaxRuns> current{};
    for (std::size_t r = 0; r < runCount; ++r) {
        current[r] = {
            runs[r].descending ? runs[r].end - 1 : runs[r].begin,
            runs[r].descending ? runs[r].begin : runs[r].end - 1
        };
    }

    while (runCount > 1) {
        std::size_t out = 0;
        for (std::size_t r = 0; r < runCount; r += 2) {
            if (r + 1 == runCount) { current[out++] = current[r]; continue; }
            const RunSummary left = current[r];
            const RunSummary right = current[r + 1];
            const bool ordered = !less(a[right.minIndex], a[left.maxIndex]);
            const bool reverseDisjoint = !less(a[left.minIndex], a[right.maxIndex]);
            if (!ordered && !reverseDisjoint) return false;
            current[out++] = ordered
                ? RunSummary{left.minIndex, right.maxIndex}
                : RunSummary{right.minIndex, left.maxIndex};
        }
        runCount = out;
    }
    return true;
}

template <class T, class Less>
void sortCheapNaturalRunsNoAlloc(std::vector<T>& a,
                                 std::array<RunDesc, kMaxRuns>& runs,
                                 std::size_t runCount,
                                 Less less) {
    for (std::size_t r = 0; r < runCount; ++r) {
        if (runs[r].descending)
            std::reverse(a.begin() + static_cast<std::ptrdiff_t>(runs[r].begin),
                         a.begin() + static_cast<std::ptrdiff_t>(runs[r].end));
    }

    while (runCount > 1) {
        std::size_t out = 0;
        for (std::size_t r = 0; r < runCount; r += 2) {
            if (r + 1 == runCount) {
                runs[out++] = {runs[r].begin, runs[r].end, false};
                continue;
            }
            const std::size_t begin = runs[r].begin;
            const std::size_t middle = runs[r].end;
            const std::size_t end = runs[r + 1].end;
            if (less(a[middle], a[middle - 1])) {
                // Preflight proved that the only remaining possibility is a
                // whole-range reverse-disjoint concatenation.
                std::rotate(a.begin() + static_cast<std::ptrdiff_t>(begin),
                            a.begin() + static_cast<std::ptrdiff_t>(middle),
                            a.begin() + static_cast<std::ptrdiff_t>(end));
            }
            runs[out++] = {begin, end, false};
        }
        runCount = out;
    }
}

template <class T, class Less>
__attribute__((noinline)) void fallbackNoAlloc(std::vector<T>& a, Less less) {
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less> &&
                  std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>) {
        jessesort::simulated_legacy::highEntropyQuickSortProduction(
            a.data(), a.size(),
            2 * static_cast<int>(std::bit_width(a.size())), less,
            false, T{});
    } else {
        // E266 propagation: use the retained allocation-free outlined heap for
        // the generic/no-copy bounded fallback as well. This is intentionally
        // the same heap primitive used by the shared high-entropy depth escape.
        jessesort::simulated_legacy::highEntropyHeapSortFallback(
            a.data(), a.size(), less);
    }
}


} // namespace jessesort::allocation_free_bounded

#endif
