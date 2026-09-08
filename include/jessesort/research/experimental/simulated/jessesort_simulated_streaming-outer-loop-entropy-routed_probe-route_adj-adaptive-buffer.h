#ifndef JESSESORT_E486_SIMULATED_STREAMING_OUTER_LOOP_ENTROPY_H
#define JESSESORT_E486_SIMULATED_STREAMING_OUTER_LOOP_ENTROPY_H

#include <jessesort/detail/pipelines/reference/simulated.h>
#include <jessesort/detail/pipelines/engines/phase_range.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

namespace jessesort::experimental::simulated_streaming_outer_loop_entropy {

constexpr std::size_t kMinNaturalRun = 32;
constexpr std::size_t kProbeValues = 64;
constexpr std::size_t kFallbackSpan = kProbeValues * 16;

enum class RunDirection { Ascending, Descending };

// Find one maximal non-strict monotone run beginning at begin. Equal prefixes
// are absorbed into whichever direction is first proved; an all-equal suffix
// is treated as ascending.
template <class T, class Less>
inline std::pair<std::size_t, RunDirection> discoverNaturalRun(
    std::span<T> a, std::size_t begin, Less less) {
    const std::size_t n = a.size();
    if (begin + 1 >= n) return {n, RunDirection::Ascending};

    std::size_t i = begin + 1;
    while (i < n && !less(a[i - 1], a[i]) && !less(a[i], a[i - 1])) ++i;
    if (i == n) return {n, RunDirection::Ascending};

    const bool desc = less(a[i], a[i - 1]);
    ++i;
    if (desc) {
        while (i < n && !less(a[i - 1], a[i])) ++i;
        return {i, RunDirection::Descending};
    }
    while (i < n && !less(a[i], a[i - 1])) ++i;
    return {i, RunDirection::Ascending};
}

template <class T, class Less>
struct ProbeResult {
    std::size_t piles = 0;
    std::size_t ascPiles = 0;
    std::size_t descPiles = 0;
    bool onlyDescending = false;
};

// A small observational dual-Patience probe. It follows the maintained
// adj-direction ownership rule but stores tails only; no values are moved.
template <class T, class Less>
inline ProbeResult<T, Less> probeDualPatience(std::span<T> a, Less less) {
    ProbeResult<T, Less> out{};
    if (a.empty()) return out;
    if (a.size() == 1) { out.piles = 1; return out; }

    bool descendingMode = false;
    std::size_t firstChange = 1;
    while (firstChange < a.size() &&
           !less(a[firstChange - 1], a[firstChange]) &&
           !less(a[firstChange], a[firstChange - 1])) ++firstChange;
    if (firstChange < a.size()) descendingMode = less(a[firstChange], a[firstChange - 1]);

    std::vector<T> ascTails;
    std::vector<T> descTails;
    ascTails.reserve(16);
    descTails.reserve(16);
    std::size_t ascHint = 0, descHint = 0;

    auto insertAsc = [&](const T& v) {
        const std::size_t p = jessesort::simulated_legacy::findAscendingPileWithTailsNoHint(ascTails, v, less);
        if (p == ascTails.size()) ascTails.push_back(v);
        else ascTails[p] = v;
        ascHint = p;
    };
    auto insertDesc = [&](const T& v) {
        const std::size_t p = jessesort::simulated_legacy::findDescendingPileWithTailsNoHint(descTails, v, less);
        if (p == descTails.size()) descTails.push_back(v);
        else descTails[p] = v;
        descHint = p;
    };
    (void)ascHint; (void)descHint;

    if (descendingMode) insertDesc(a[0]);
    else insertAsc(a[0]);

    for (std::size_t i = 1; i < a.size(); ++i) {
        if (less(a[i - 1], a[i])) descendingMode = false;
        else if (less(a[i], a[i - 1])) descendingMode = true;
        if (descendingMode) insertDesc(a[i]);
        else insertAsc(a[i]);
    }

    out.ascPiles = ascTails.size();
    out.descPiles = descTails.size();
    out.piles = out.ascPiles + out.descPiles;
    out.onlyDescending = ascTails.empty() && descTails.size() == 1;
    return out;
}

template <class T, class Less>
inline bool spanSorted(std::span<T> a, Less less) {
    return std::is_sorted(a.begin(), a.end(), less);
}

template <class T, class Less>
inline void retainEnd(std::span<T> whole, std::vector<std::size_t>& ends,
                      std::size_t end, Less less) {
    if (!ends.empty()) {
        const std::size_t prevEnd = ends.back();
        if (prevEnd < end && prevEnd > 0 && !less(whole[prevEnd], whole[prevEnd - 1])) {
            ends.back() = end;
            return;
        }
    }
    ends.push_back(end);
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    const std::size_t n = arr.size();
    if (n < kMinNaturalRun) {
        jessesort::simulated_legacy::sort(arr, less);
        return;
    }

    std::span<T> whole(arr.data(), arr.size());
    std::vector<std::size_t> ends;
    ends.reserve(std::max<std::size_t>(8, n / kFallbackSpan + 8));

    std::size_t pos = 0;
    while (pos < n) {
        const auto [naturalEnd, dir] = discoverNaturalRun(whole, pos, less);
        const std::size_t naturalLen = naturalEnd - pos;
        if (naturalLen >= kMinNaturalRun) {
            if (dir == RunDirection::Descending) {
                std::reverse(whole.begin() + static_cast<std::ptrdiff_t>(pos),
                             whole.begin() + static_cast<std::ptrdiff_t>(naturalEnd));
            }
            pos = naturalEnd;
            retainEnd(whole, ends, pos, less);
            continue;
        }

        const std::size_t probeEnd = std::min(n, pos + kProbeValues);
        std::span<T> probe = whole.subspan(pos, probeEnd - pos);
        const auto pr = probeDualPatience(probe, less);
        if (pr.piles == 1) {
            if (pr.onlyDescending) std::reverse(probe.begin(), probe.end());
            if (spanSorted(probe, less)) {
                pos = probeEnd;
                retainEnd(whole, ends, pos, less);
                continue;
            }
        }

        // Reuse the maintained ordinary-simulated early-random signature: at
        // least six piles in each game after the 64-value probe. Once that
        // evidence appears, stop making bounded runs and let the pillar own the
        // whole remaining suffix. This adds no new benchmark-derived threshold.
        if (pr.ascPiles >= 6 && pr.descPiles >= 6) {
            jessesort::simulated_phase_range_engine::sort(whole.subspan(pos), less);
            pos = n;
            retainEnd(whole, ends, pos, less);
            continue;
        }

        // Unresolved low/medium geometry delegates a bounded span to
        // the maintained dual-game simulated pillar. Later experiments can move
        // 2-pile / medium / high-entropy routing into this loop without changing
        // the outer state machine.
        const std::size_t chunkEnd = std::min(n, pos + kFallbackSpan);
        jessesort::simulated_phase_range_engine::sort(
            whole.subspan(pos, chunkEnd - pos), less);
        pos = chunkEnd;
        retainEnd(whole, ends, pos, less);
    }

    if (ends.size() > 1) {
        jessesort::simulated_phase_range_engine::mergePreparedRunsFromSpanE283(
            whole, ends, less);
    }
}

} // namespace jessesort::experimental::simulated_streaming_outer_loop_entropy

#endif
