#pragma once

// Maintained strict allocation-free live-phase pipeline.
// E735 promotes the E728 strict live-phase architecture and places the retained
// E731/E729 size-gated whole-input natural-run prepass ahead of it.
// Contract: no dynamic allocation, fixed-capacity metadata, bounded stack, and
// worst-case O(n log n) on every reachable route.

#include <jessesort/detail/pipelines/reference/live_phase_adaptive_noalloc.h>
#include <jessesort/detail/pipelines/reference/rust_unstable_noalloc.h>

#include <array>
#include <functional>
#include <type_traits>
#include <vector>

namespace jessesort::allocation_free_live_phase_strict {

template<class T, class Less>
bool tryE729PrepassStrict(std::vector<T>& a, Less less) {
    constexpr std::size_t kPrefix = 64;
    const std::size_t n = a.size();
    if (n < 50000) return false;
    for (std::size_t i = 1; i < kPrefix; ++i)
        if (less(a[i], a[i - 1])) return false;

    bool coarseDrop = false;
    std::size_t previous = 0;
    for (std::size_t sample = 1; sample < 9; ++sample) {
        const std::size_t index = ((n - 1) * sample) / 8;
        if (less(a[index], a[previous])) { coarseDrop = true; break; }
        previous = index;
    }
    if (!coarseDrop) return false;

    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns> runs{};
    std::size_t rc = 0, i = 0;
    bool firstTwoOverlap = true;
    while (i < n) {
        if (rc == runs.size()) return false;
        const std::size_t begin = i;
        if (i + 1 == n) { runs[rc++] = {begin, n, false}; break; }
        const bool descending = less(a[i + 1], a[i]);
        i += 2;
        if (descending) while (i < n && less(a[i], a[i - 1])) ++i;
        else while (i < n && !less(a[i], a[i - 1])) ++i;
        runs[rc++] = {begin, i, descending};

        if (rc >= 2 && rc <= 3) {
            const auto L = runs[rc - 2], R = runs[rc - 1];
            const T& l0 = a[L.begin]; const T& l1 = a[L.end - 1];
            const T& r0 = a[R.begin]; const T& r1 = a[R.end - 1];
            const T& lmin = less(l1, l0) ? l1 : l0;
            const T& lmax = less(l1, l0) ? l0 : l1;
            const T& rmin = less(r1, r0) ? r1 : r0;
            const T& rmax = less(r1, r0) ? r0 : r1;
            firstTwoOverlap &= less(rmin, lmax) && less(lmin, rmax);
            if (rc == 3 && firstTwoOverlap && i <= 8192) return false;
        }
    }
    if (rc <= 1) return false;

    if (jessesort::allocation_free_bounded::cheapMergeGeometryOnly(a, runs, rc, less)) {
        jessesort::allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a, runs, rc, less);
        return true;
    }
    if (jessesort::allocation_free_low_run::repeatedOverlapGeometry(a, runs, rc, less))
        return false;

    bool hasDescending = false;
    for (std::size_t r = 0; r < rc; ++r) hasDescending |= runs[r].descending;
    if (hasDescending)
        jessesort::rust_unstable_noalloc_detail::mergeNaturalRunsLeafBufferedNoAlloc(a, runs, rc, less);
    else
        jessesort::rust_unstable_noalloc_detail::mergeNaturalRunsTopLevelTrimmedNoAlloc(a, runs, rc, less);
    return true;
}

template<class T, class Less = std::less<T>>
void sort(std::vector<T>& values, Less less = Less{}) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "strict live-phase noalloc requires movable values");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");
    if (values.size() >= 50000 && tryE729PrepassStrict(values, less)) return;
    // Strict instantiation: no adaptive scratch-buffer merge path is reachable.
    jessesort::allocation_free_live_phase_adaptive::sortImpl<T, Less, false>(values, less);
}

} // namespace jessesort::allocation_free_live_phase_strict
