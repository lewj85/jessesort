#ifndef JESSESORT_E229_JESSESORT_NOALLOC_BOUNDED_FALLBACK_INPLACE_ADAPTIVE_PARTITION_HEAPSORT_FALLBACK_H
#define JESSESORT_E229_JESSESORT_NOALLOC_BOUNDED_FALLBACK_INPLACE_ADAPTIVE_PARTITION_HEAPSORT_FALLBACK_H

#include <jessesort/detail/decomposition/noalloc/bounded_common.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::allocation_free_bounded {

// E209 baseline + E210 fixed-workspace compression.
// Identity: no heap allocation inside sort; fixed contiguous index-tail metadata
// is used to admit only low-pile Jesse-friendly shapes. Admitted shapes with at
// most kMaxRuns natural runs are reconstructed/sorted by allocation-free in-place
// run merging. Everything else falls back to an allocation-free partition/heap
// backend. indexed remains the allocating index-tail/genericity variation.
inline constexpr std::size_t kAscCapacity = 63;
inline constexpr std::size_t kDescCapacity = 64;

struct Metrics {
    std::size_t ascPiles = 0;
    std::size_t descPiles = 0;
    std::size_t naturalRuns = 0;
    bool earlyRandomFallback = false;
    bool pileCapacityFallback = false;
    bool runCapacityFallback = false;
    bool mergeGeometryFallback = false;
    bool specializedDirect = false;
    bool liveRunPath = false;
    bool monotoneReturn = false;
};

enum class ClassifyResult { Accept, SortedAscending, SortedDescending, FallbackRandom, FallbackCapacity };

template <class T, class Less>
inline std::size_t findAscFixed(const std::vector<T>& a,
                                const std::array<std::size_t, kAscCapacity>& tails,
                                std::size_t count,
                                std::size_t valueIndex,
                                Less less) {
    const T& value = a[valueIndex];
    std::ptrdiff_t idx = -1;
    std::size_t step = count ? (std::size_t{1} << (std::bit_width(count) - 1)) : 0;
    for (; step; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < count && less(value, a[tails[next]])) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <class T, class Less>
inline std::size_t findDescFixed(const std::vector<T>& a,
                                 const std::array<std::size_t, kDescCapacity>& tails,
                                 std::size_t count,
                                 std::size_t valueIndex,
                                 Less less) {
    const T& value = a[valueIndex];
    std::ptrdiff_t idx = -1;
    std::size_t step = count ? (std::size_t{1} << (std::bit_width(count) - 1)) : 0;
    for (; step; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < count && less(a[tails[next]], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <class T, class Less>
ClassifyResult classifyBoundedPatience(const std::vector<T>& a, Less less, Metrics* metrics) {
    const std::size_t n = a.size();
    if (n < 2) return ClassifyResult::SortedAscending;

    enum class Direction { Unknown, Ascending, Descending };
    Direction prefixDirection = Direction::Unknown;
    std::size_t prefixEnd = 1;
    for (; prefixEnd < n; ++prefixEnd) {
        const T& previous = a[prefixEnd - 1];
        const T& value = a[prefixEnd];
        if (prefixDirection == Direction::Ascending) {
            if (less(value, previous)) break;
        } else if (prefixDirection == Direction::Descending) {
            if (less(previous, value)) break;
        } else if (less(previous, value)) {
            prefixDirection = Direction::Ascending;
        } else if (less(value, previous)) {
            prefixDirection = Direction::Descending;
        }
    }
    if (prefixEnd == n) {
        if (metrics) metrics->monotoneReturn = true;
        return prefixDirection == Direction::Descending
            ? ClassifyResult::SortedDescending
            : ClassifyResult::SortedAscending;
    }

    std::array<std::size_t, kAscCapacity> asc{};
    std::array<std::size_t, kDescCapacity> desc{};
    std::size_t ascCount = 0, descCount = 0;
    bool descendingMode = false;
    std::size_t processStart = 1;

    const bool confirmedShort =
        prefixDirection != Direction::Unknown &&
        prefixEnd < 32 &&
        jessesort::simulated_legacy::confirmedShortSameDirectionPrefix(
            a, prefixEnd, prefixDirection == Direction::Descending, less, 32);
    const bool materializePrefix =
        prefixDirection != Direction::Unknown && (prefixEnd >= 32 || confirmedShort);

    if (materializePrefix) {
        processStart = prefixEnd;
        if (prefixDirection == Direction::Ascending) {
            asc[0] = prefixEnd - 1; ascCount = 1; descendingMode = false;
        } else {
            desc[0] = prefixEnd - 1; descCount = 1; descendingMode = true;
        }
    } else if (prefixDirection == Direction::Descending) {
        desc[0] = 0; descCount = 1; descendingMode = true;
    } else {
        asc[0] = 0; ascCount = 1; descendingMode = false;
    }

    bool overridePending = false;
    bool overrideDescending = false;
    if (materializePrefix && processStart == prefixEnd && processStart + 1 < n) {
        if (less(a[processStart], a[processStart + 1])) {
            overridePending = true; overrideDescending = false;
        } else if (less(a[processStart + 1], a[processStart])) {
            overridePending = true; overrideDescending = true;
        }
    }

    auto insert = [&](std::size_t i) -> bool {
        if (descendingMode) {
            const std::size_t p = findDescFixed(a, desc, descCount, i, less);
            if (p < descCount) desc[p] = i;
            else {
                if (descCount == kDescCapacity || ascCount + descCount >= kAscCapacity + kDescCapacity)
                    return false;
                desc[descCount++] = i;
            }
        } else {
            const std::size_t p = findAscFixed(a, asc, ascCount, i, less);
            if (p < ascCount) asc[p] = i;
            else {
                if (ascCount == kAscCapacity || ascCount + descCount >= kAscCapacity + kDescCapacity)
                    return false;
                asc[ascCount++] = i;
            }
        }
        return true;
    };

    for (std::size_t i = processStart; i < n; ++i) {
        if (overridePending) {
            descendingMode = overrideDescending;
            overridePending = false;
        } else if (less(a[i - 1], a[i])) {
            descendingMode = false;
        } else if (less(a[i], a[i - 1])) {
            descendingMode = true;
        } // equal values remain in the current game without changing direction.

        if (!insert(i)) {
            if (metrics) {
                metrics->ascPiles = ascCount;
                metrics->descPiles = descCount;
                metrics->pileCapacityFallback = true;
            }
            return ClassifyResult::FallbackCapacity;
        }

        // Match the existing early-random concept: once the first 64 source
        // positions have demonstrated at least six piles in each game, do not
        // spend another O(n) pass building pile metadata that we will discard.
        if (n >= 10000 && i + 1 == 64 && ascCount >= 6 && descCount >= 6) {
            if (metrics) {
                metrics->ascPiles = ascCount;
                metrics->descPiles = descCount;
                metrics->earlyRandomFallback = true;
            }
            return ClassifyResult::FallbackRandom;
        }
    }

    if (metrics) { metrics->ascPiles = ascCount; metrics->descPiles = descCount; }
    return ClassifyResult::Accept;
}

template <class T, class Less = std::less<T>>
void sortWithMetrics(std::vector<T>& a, Less less = Less{}, Metrics* metrics = nullptr) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "noalloc-bounded requires movable values");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (a.size() < 2) return;
    if (jessesort::detail::tryTinyInsertionSort(a, less)) return;

    // Reuse only specialized routes known to use bounded local storage, with
    // the same monotone-prefix guard as simulated. Without this guard, globally
    // structured inputs such as Rotated can be misclassified as high entropy.
    bool specialValuePrefixMayMix = true;
    if (a.size() >= 4) {
        const bool firstThreeAscending =
            less(a[0], a[1]) && less(a[1], a[2]) && less(a[2], a[3]);
        const bool firstThreeDescending =
            less(a[1], a[0]) && less(a[2], a[1]) && less(a[3], a[2]);
        specialValuePrefixMayMix = !(firstThreeAscending || firstThreeDescending);
    }
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (specialValuePrefixMayMix &&
            jessesort::simulated_legacy::trySpecializedPrePatienceRoutes(a, less)) {
            if (metrics) metrics->specializedDirect = true;
            return;
        }
    }

    const ClassifyResult classified = classifyBoundedPatience(a, less, metrics);
    if (classified == ClassifyResult::SortedAscending) return;
    if (classified == ClassifyResult::SortedDescending) {
        std::reverse(a.begin(), a.end());
        return;
    }
    if (classified != ClassifyResult::Accept) {
        fallbackNoAlloc(a, less);
        return;
    }

    std::array<RunDesc, kMaxRuns> runs{};
    std::size_t runCount = 0;
    if (!discoverNaturalRuns(a, runs, runCount, less)) {
        if (metrics) {
            metrics->naturalRuns = kMaxRuns + 1;
            metrics->runCapacityFallback = true;
        }
        fallbackNoAlloc(a, less);
        return;
    }
    if (metrics) metrics->naturalRuns = runCount;
    if (!cheapMergeGeometryOnly(a, runs, runCount, less)) {
        if (metrics) metrics->mergeGeometryFallback = true;
        fallbackNoAlloc(a, less);
        return;
    }
    if (metrics) metrics->liveRunPath = true;
    sortCheapNaturalRunsNoAlloc(a, runs, runCount, less);
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    sortWithMetrics(a, less, nullptr);
}

} // namespace jessesort::allocation_free_bounded

#endif
