#ifndef JESSESORT_SIMULATED_PROBE_FIRST_DIRECT_MERGE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_SIMULATED_PROBE_FIRST_DIRECT_MERGE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/detail/pipelines/reference/simulated_direct.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_probe_first_direct {

struct ProbeObservation {
    std::size_t values = 0;
    std::size_t ascPiles = 0;
    std::size_t descPiles = 0;
    std::size_t longestPile = 0;
    std::size_t ascValues = 0;
    std::size_t descValues = 0;
};

// E235/E236 legacy/reference observer retained for experiment comparison.
// Production `simulated-probe-direct` no longer uses this disposable wrapper
// observation after E242; routing now comes from checkpoints inside the live
// mature Patience insertion state.
template <typename T, typename Less>
inline ProbeObservation observePatienceGeometry(const std::vector<T>& arr, Less less) {
    constexpr std::size_t K = 48;
    ProbeObservation out;
    const std::size_t m = std::min(arr.size(), K);
    if (m == 0) return out;

    std::array<std::size_t, K> ascTail{};
    std::array<std::size_t, K> descTail{};
    std::array<std::size_t, K> ascCount{};
    std::array<std::size_t, K> descCount{};
    std::size_t na = 0, nd = 0;
    bool descending = m >= 2 && less(arr[1], arr[0]);

    auto ascPile = [&](std::size_t valueIndex) {
        std::ptrdiff_t idx = -1;
        std::size_t step = na ? (std::size_t{1} << (std::bit_width(na) - 1)) : 0;
        for (; step; step >>= 1) {
            const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
            if (next < na && less(arr[valueIndex], arr[ascTail[next]])) idx = static_cast<std::ptrdiff_t>(next);
        }
        return static_cast<std::size_t>(idx + 1);
    };
    auto descPile = [&](std::size_t valueIndex) {
        std::ptrdiff_t idx = -1;
        std::size_t step = nd ? (std::size_t{1} << (std::bit_width(nd) - 1)) : 0;
        for (; step; step >>= 1) {
            const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
            if (next < nd && less(arr[descTail[next]], arr[valueIndex])) idx = static_cast<std::ptrdiff_t>(next);
        }
        return static_cast<std::size_t>(idx + 1);
    };

    for (std::size_t i = 0; i < m; ++i) {
        if (i) {
            if (less(arr[i - 1], arr[i])) descending = false;
            else if (less(arr[i], arr[i - 1])) descending = true;
        }
        if (descending) {
            const std::size_t p = descPile(i);
            if (p == nd) { descTail[nd] = i; descCount[nd] = 1; ++nd; }
            else { descTail[p] = i; ++descCount[p]; }
            ++out.descValues;
        } else {
            const std::size_t p = ascPile(i);
            if (p == na) { ascTail[na] = i; ascCount[na] = 1; ++na; }
            else { ascTail[p] = i; ++ascCount[p]; }
            ++out.ascValues;
        }
    }
    out.values = m;
    out.ascPiles = na;
    out.descPiles = nd;
    for (std::size_t i = 0; i < na; ++i) out.longestPile = std::max(out.longestPile, ascCount[i]);
    for (std::size_t i = 0; i < nd; ++i) out.longestPile = std::max(out.longestPile, descCount[i]);
    return out;
}

template <typename T, typename Less>
inline bool probeSuggestsSparseAscendingDisorder(const std::vector<T>& arr, Less less,
                                                  ProbeObservation* out = nullptr) {
    if (arr.size() < 10000) return false;
    const ProbeObservation p = observePatienceGeometry(arr, less);
    if (out) *out = p;
    const std::size_t piles = p.ascPiles + p.descPiles;
    if (p.values < 32 || piles > 20) return false;
    if (p.longestPile * 4 < p.values) return false;
    if (p.ascValues * 5 < p.values * 3) return false;

    // E236's broader admission pays at moderate sizes but lost 2-5% in the
    // 1m x50 check. Preserve E235's stricter classifier once n exceeds the
    // moderate-size regime rather than trading large-N reliability for route
    // count. Size-dependent cutoffs are a normal backend-policy choice; E237+
    // may revisit this boundary with a wider size sweep.
    constexpr std::size_t kExpandedMaxN = 262144;
    const bool expanded = arr.size() <= kExpandedMaxN;
    if (!expanded && piles < 4) return false;

    // E236: use the already-paid Patience geometry as the primary signal, then
    // summarize the same six small distributed safety windows instead of
    // requiring one rigid rule for every plausible geometry. Very-low pile
    // counts mean the prefix was almost perfectly ordered, so require disorder
    // to be independently visible in at least two remote regions. Moderate
    // geometry keeps E235's <=2-inversion rule, with one narrow allowance for a
    // single 3-inversion window when the probe itself already formed >=7 piles.
    constexpr std::size_t kChecks = 6;
    constexpr std::size_t kPairs = 8;
    unsigned totalInversions = 0;
    unsigned nonzeroWindows = 0;
    unsigned overTwoWindows = 0;
    for (std::size_t s = 1; s <= kChecks; ++s) {
        const std::size_t start = ((arr.size() - (kPairs + 1)) * s) / (kChecks + 1);
        unsigned inversions = 0;
        for (std::size_t j = 1; j <= kPairs; ++j)
            inversions += static_cast<unsigned>(less(arr[start + j], arr[start + j - 1]));
        if ((!expanded && inversions > 2) || (expanded && inversions > 3)) return false;
        totalInversions += inversions;
        nonzeroWindows += static_cast<unsigned>(inversions != 0);
        overTwoWindows += static_cast<unsigned>(inversions == 3);
        if (overTwoWindows > 1) return false;
    }

    if (!expanded) return true;  // exact E235 acceptance rule at large N
    if (piles < 4)
        return overTwoWindows == 0 && nonzeroWindows >= 2;
    if (overTwoWindows == 0) return true;
    return piles >= 7 && totalInversions <= 11 && nonzeroWindows >= 4;
}

template <typename T, typename Less>
inline void sortSparseBlocksUnconditionally(std::vector<T>& arr, Less less) {
    constexpr std::size_t kBlock = 128;
    const std::size_t n = arr.size();
    std::vector<std::size_t> ends;
    ends.reserve((n + kBlock - 1) / kBlock);
    for (std::size_t begin = 0; begin < n; begin += kBlock) {
        const std::size_t end = std::min(n, begin + kBlock);
        for (std::size_t i = begin + 1; i < end; ++i) {
            T value = std::move(arr[i]);
            std::size_t j = i;
            while (j > begin && less(value, arr[j - 1])) {
                arr[j] = std::move(arr[j - 1]);
                --j;
            }
            arr[j] = std::move(value);
        }
        ends.push_back(end);
    }
    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) tmp.resize(n);
    else tmp = arr;
    jessesort::simulated_direct_merge::mergeRunsAdjacentPairsEnds(arr, tmp, ends, less, false, true);
}

template <typename T, typename Less>
inline bool tryProbeRoutedSparseBlocks(std::vector<T>& arr, Less less) {
    if (!probeSuggestsSparseAscendingDisorder(arr, less)) return false;
    sortSparseBlocksUnconditionally(arr, less);
    return true;
}

// E242: route from the exact live mature-Patience state. The checkpoint may
// represent a materialized monotone prefix longer than `processed`; normalize
// that one-pile case back to the checkpoint width before applying geometry.
template <typename T>
inline bool liveCheckpointGeometrySuggestsSparse(
    const jessesort::simulated_direct_merge::SimulatedInsertionResult<T>& sim,
    std::size_t processed) {
    std::size_t ascValues = 0, descValues = 0, longest = 0;
    for (std::size_t c : sim.ascCounts) { ascValues += c; longest = std::max(longest, c); }
    for (std::size_t c : sim.descCounts) { descValues += c; longest = std::max(longest, c); }
    const std::size_t piles = sim.ascCounts.size() + sim.descCounts.size();
    const std::size_t total = ascValues + descValues;
    if (total > processed && piles == 1) {
        if (!sim.ascCounts.empty()) { ascValues = processed; descValues = 0; }
        else { descValues = processed; ascValues = 0; }
        longest = processed;
    }
    if (piles == 0 || piles > 20) return false;
    if (longest * 4 < processed) return false;
    if (ascValues * 5 < processed * 3) return false;
    return true;
}

template <typename T, typename Less>
inline bool distributedSparseSafetyFromLiveGeometry(
    const std::vector<T>& arr,
    const jessesort::simulated_direct_merge::SimulatedInsertionResult<T>& sim,
    std::size_t processed, Less less) {
    std::size_t piles = sim.ascCounts.size() + sim.descCounts.size();
    if (!liveCheckpointGeometrySuggestsSparse(sim, processed)) return false;
    constexpr std::size_t kExpandedMaxN = 262144;
    const bool expanded = arr.size() <= kExpandedMaxN;
    if (!expanded && piles < 4) return false;

    constexpr std::size_t kChecks = 6;
    constexpr std::size_t kPairs = 8;
    unsigned totalInversions = 0, nonzeroWindows = 0, overTwoWindows = 0;
    for (std::size_t s = 1; s <= kChecks; ++s) {
        const std::size_t start = ((arr.size() - (kPairs + 1)) * s) / (kChecks + 1);
        unsigned inversions = 0;
        for (std::size_t j = 1; j <= kPairs; ++j)
            inversions += static_cast<unsigned>(less(arr[start + j], arr[start + j - 1]));
        if ((!expanded && inversions > 2) || (expanded && inversions > 3)) return false;
        totalInversions += inversions;
        nonzeroWindows += static_cast<unsigned>(inversions != 0);
        overTwoWindows += static_cast<unsigned>(inversions == 3);
        if (overTwoWindows > 1) return false;
    }
    if (!expanded) return true;
    if (piles < 4) return overTwoWindows == 0 && nonzeroWindows >= 2;
    if (overTwoWindows == 0) return true;
    return piles >= 7 && totalInversions <= 11 && nonzeroWindows >= 4;
}

template <typename T, typename Less>
inline bool tryMonotoneDirect(std::vector<T>& arr, Less less) {
    const std::size_t n = arr.size();
    if (n < 2) return true;

    // E245: keep a short scalar rejection gate so random/non-monotone inputs
    // still fail cheaply, then amortize loop/branch overhead on true one-pile
    // inputs by checking four ascending adjacencies per outer iteration.
    std::size_t i = 1;
    const std::size_t gate = std::min<std::size_t>(n, 9);
    while (i < gate && !less(arr[i], arr[i - 1])) ++i;
    if (i == gate) {
        while (i + 3 < n) {
            const bool bad =
                less(arr[i],     arr[i - 1]) |
                less(arr[i + 1], arr[i])     |
                less(arr[i + 2], arr[i + 1]) |
                less(arr[i + 3], arr[i + 2]);
            if (bad) {
                while (i < n && !less(arr[i], arr[i - 1])) ++i;
                break;
            }
            i += 4;
        }
        while (i < n && !less(arr[i], arr[i - 1])) ++i;
        if (i == n) return true;
    }

    // Descending one-pile inputs benefit from a wider batch because the first
    // ascending comparison already rejected them. Preserve the same generic
    // comparator semantics and reverse only after the full proof succeeds.
    i = 1;
    while (i < gate && !less(arr[i - 1], arr[i])) ++i;
    if (i < gate) return false;
    while (i + 7 < n) {
        const bool bad =
            less(arr[i - 1], arr[i])     |
            less(arr[i],     arr[i + 1]) |
            less(arr[i + 1], arr[i + 2]) |
            less(arr[i + 2], arr[i + 3]) |
            less(arr[i + 3], arr[i + 4]) |
            less(arr[i + 4], arr[i + 5]) |
            less(arr[i + 5], arr[i + 6]) |
            less(arr[i + 6], arr[i + 7]);
        if (bad) {
            while (i < n && !less(arr[i - 1], arr[i])) ++i;
            return false;
        }
        i += 8;
    }
    while (i < n && !less(arr[i - 1], arr[i])) ++i;
    if (i == n) {
        std::reverse(arr.begin(), arr.end());
        return true;
    }
    return false;
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    if (arr.size() >= 10000 &&
        jessesort::simulated_direct_merge::tryLongAscendingNaturalRunDirect(arr, less)) return;
    // Keep E233's proof-based zigzag route in both architectural branches.
    if (arr.size() >= 10000 &&
        jessesort::simulated_direct_merge::tryTwoLaneZigzagDirect(arr, less)) return;
    // Preserve the mature monotone exits without paying a discarded probe on
    // fully sorted/reverse inputs.
    if (tryMonotoneDirect(arr, less)) return;

    // E242: the sparse decision now observes the exact live mature-Patience
    // state. A 32-value checkpoint is the common cache-aligned unit; if its
    // geometry is not yet sufficient, the *same* live insertion continues to
    // 48 and gets one second chance. Ordinary fallback then continues the same
    // insertion to 64+ without restarting or reconstructing approximate state.
    auto checkpoint = [&](const auto& sim, std::size_t processed) -> bool {
        if (arr.size() < 10000) return false;
        if (processed == 32) {
            if (!liveCheckpointGeometrySuggestsSparse(sim, processed)) return false;
            if (!distributedSparseSafetyFromLiveGeometry(arr, sim, processed, less)) return false;
            sortSparseBlocksUnconditionally(arr, less);
            return true;
        }
        if (processed == 48 &&
            distributedSparseSafetyFromLiveGeometry(arr, sim, processed, less)) {
            sortSparseBlocksUnconditionally(arr, less);
            return true;
        }
        return false;
    };
    jessesort::simulated_direct_merge::sortImplCoreWithCheckpoint<true>(
        arr, less, true, true, true, true, true, checkpoint);
}

} // namespace jessesort::simulated_probe_first_direct
#endif
