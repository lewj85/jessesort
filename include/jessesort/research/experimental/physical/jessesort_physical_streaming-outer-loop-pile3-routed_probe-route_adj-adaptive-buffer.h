#ifndef JESSESORT_E488_PHYSICAL_STREAMING_OUTER_LOOP_PILE3_H
#define JESSESORT_E488_PHYSICAL_STREAMING_OUTER_LOOP_PILE3_H

#include <jessesort/detail/pipelines/reference/physical.h>
#include <jessesort/research/experimental/simulated/jessesort_simulated_streaming-outer-loop-entropy-routed_probe-route_adj-adaptive-buffer.h>
#include <jessesort/detail/pipelines/engines/phase_range.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

namespace jessesort::experimental::physical_streaming_outer_loop_pile3 {

constexpr std::size_t kMinNaturalRun = 32;
constexpr std::size_t kProbeValues = 64;
constexpr std::size_t kFallbackSpan = kProbeValues * 16;
using RunDirection = jessesort::experimental::simulated_streaming_outer_loop_entropy::RunDirection;

template <class T, class Less>
inline auto discoverNaturalRun(std::span<T> a, std::size_t begin, Less less) {
    return jessesort::experimental::simulated_streaming_outer_loop_entropy::discoverNaturalRun(a, begin, less);
}

template <class T, class Less>
inline auto probeDualPatience(std::span<T> a, Less less) {
    return jessesort::experimental::simulated_streaming_outer_loop_entropy::probeDualPatience(a, less);
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

// Temporary range adapter used only because the maintained physical pillar
// currently owns std::vector<T>& rather than std::span<T>.  Values are moved,
// not copied.  E487 benchmarks a whole-array adapter control separately so the
// cost of this representation bridge is measurable rather than hidden.
template <class T, class Less>
inline void physicalSortSpan(std::span<T> s, Less less) {
    if (s.size() < 2) return;
    std::vector<T> tmp;
    tmp.reserve(s.size());
    for (T& v : s) tmp.push_back(std::move(v));
    jessesort::actual_piles_legacy::sort(tmp, less);
    for (std::size_t i = 0; i < s.size(); ++i) s[i] = std::move(tmp[i]);
}

template <class T, class Less = std::less<T>>
void sortAdapterControl(std::vector<T>& arr, Less less = Less{}) {
    physicalSortSpan(std::span<T>(arr.data(), arr.size()), less);
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    const std::size_t n = arr.size();
    if (n < kMinNaturalRun) {
        jessesort::actual_piles_legacy::sort(arr, less);
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

        // E488 streaming router: one pile is committed above; exactly two piles
        // remain on the bounded structured path; any 3+ pile probe is already
        // nontrivial enough that repeated 1024-value fragmentation is the wrong
        // ownership model. Hand the whole suffix to the maintained pillar.
        // The 3-pile boundary reuses the maintained physical E183 boundary
        // concept rather than introducing a benchmark-derived threshold.
        if (pr.piles >= 3) {
            physicalSortSpan(whole.subspan(pos), less);
            pos = n;
            retainEnd(whole, ends, pos, less);
            continue;
        }

        const std::size_t chunkEnd = std::min(n, pos + kFallbackSpan);
        physicalSortSpan(whole.subspan(pos, chunkEnd - pos), less);
        pos = chunkEnd;
        retainEnd(whole, ends, pos, less);
    }

    if (ends.size() > 1) {
        jessesort::simulated_phase_range_engine::mergePreparedRunsFromSpanE283(
            whole, ends, less);
    }
}

} // namespace jessesort::experimental::physical_streaming_outer_loop_pile3

#endif
