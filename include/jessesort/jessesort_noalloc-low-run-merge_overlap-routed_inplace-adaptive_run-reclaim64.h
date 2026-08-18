#ifndef JESSESORT_E229_JESSESORT_NOALLOC_LOW_RUN_MERGE_OVERLAP_ROUTED_INPLACE_ADAPTIVE_RUN_RECLAIM64_H
#define JESSESORT_E229_JESSESORT_NOALLOC_LOW_RUN_MERGE_OVERLAP_ROUTED_INPLACE_ADAPTIVE_RUN_RECLAIM64_H

#include <jessesort/jessesort_noalloc_bounded-fallback_inplace-adaptive_partition-heapsort-fallback.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <vector>

namespace jessesort::allocation_free_v11 {

#if defined(__GNUC__) || defined(__clang__)
#define JESSESORT_V11_NOINLINE __attribute__((noinline, cold))
#else
#define JESSESORT_V11_NOINLINE
#endif

// V11 identity (E216): allocation-free bounded JesseSort with a shared
// 127-index tail pool plus selective general in-place merging for low-run
// structured inputs. Sawtooth-like repeated-overlap run geometry remains on
// V10's allocation-free fallback path.
inline constexpr std::size_t kTotalTailCapacity = 127;
inline constexpr std::size_t kAscCapacity = 63;
inline constexpr std::size_t kDescCapacity = 64;
inline constexpr std::size_t kMaxRuns = allocation_free_v10::kMaxRuns;

struct Metrics {
    std::size_t ascPiles = 0;
    std::size_t descPiles = 0;
    std::size_t naturalRuns = 0;
    bool earlyRandomFallback = false;
    bool earlyRepeatedOverlapFallback = false;
    bool pileCapacityFallback = false;
    bool runCapacityFallback = false;
    bool repeatedOverlapFallback = false;
    bool specializedDirect = false;
    bool cheapRunPath = false;
    bool generalRunPath = false;
    bool monotoneReturn = false;
    bool streamedRunPath = false;
    std::size_t runReclaims = 0;
};

enum class ClassifyResult { Accept, SortedAscending, SortedDescending, FallbackRandom, FallbackCapacity };

template <class T, class Less>
inline std::size_t findAscShared(const std::vector<T>& a,
                                 const std::array<std::size_t, kTotalTailCapacity>& tails,
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
inline std::size_t findDescShared(const std::vector<T>& a,
                                  const std::array<std::size_t, kTotalTailCapacity>& tails,
                                  std::size_t count,
                                  std::size_t valueIndex,
                                  Less less) {
    const T& value = a[valueIndex];
    std::ptrdiff_t idx = -1;
    std::size_t step = count ? (std::size_t{1} << (std::bit_width(count) - 1)) : 0;
    for (; step; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        const std::size_t physical = kTotalTailCapacity - 1 - next;
        if (next < count && less(a[tails[physical]], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <class T, class Less>
ClassifyResult classifySharedBoundedPatience(const std::vector<T>& a, Less less, Metrics* metrics) {
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
            ? ClassifyResult::SortedDescending : ClassifyResult::SortedAscending;
    }

    std::array<std::size_t, kTotalTailCapacity> tails{};
    std::size_t ascCount = 0, descCount = 0;
    bool descendingMode = false;
    std::size_t processStart = 1;

    const bool confirmedShort =
        prefixDirection != Direction::Unknown && prefixEnd < 32 &&
        jessesort::simulated_legacy::confirmedShortSameDirectionPrefix(
            a, prefixEnd, prefixDirection == Direction::Descending, less, 32);
    const bool materializePrefix =
        prefixDirection != Direction::Unknown && (prefixEnd >= 32 || confirmedShort);

    if (materializePrefix) {
        processStart = prefixEnd;
        if (prefixDirection == Direction::Ascending) {
            tails[0] = prefixEnd - 1; ascCount = 1; descendingMode = false;
        } else {
            tails[kTotalTailCapacity - 1] = prefixEnd - 1; descCount = 1; descendingMode = true;
        }
    } else if (prefixDirection == Direction::Descending) {
        tails[kTotalTailCapacity - 1] = 0; descCount = 1; descendingMode = true;
    } else {
        tails[0] = 0; ascCount = 1; descendingMode = false;
    }

    bool overridePending = false, overrideDescending = false;
    if (materializePrefix && processStart == prefixEnd && processStart + 1 < n) {
        if (less(a[processStart], a[processStart + 1])) {
            overridePending = true; overrideDescending = false;
        } else if (less(a[processStart + 1], a[processStart])) {
            overridePending = true; overrideDescending = true;
        }
    }

    auto insert = [&](std::size_t i) -> bool {
        if (descendingMode) {
            const std::size_t p = findDescShared(a, tails, descCount, i, less);
            if (p < descCount) tails[kTotalTailCapacity - 1 - p] = i;
            else {
                if (descCount == kDescCapacity || ascCount + descCount >= kTotalTailCapacity) return false;
                tails[kTotalTailCapacity - 1 - descCount++] = i;
            }
        } else {
            const std::size_t p = findAscShared(a, tails, ascCount, i, less);
            if (p < ascCount) tails[p] = i;
            else {
                if (ascCount == kAscCapacity || ascCount + descCount >= kTotalTailCapacity) return false;
                tails[ascCount++] = i;
            }
        }
        return true;
    };

    for (std::size_t i = processStart; i < n; ++i) {
        if (overridePending) { descendingMode = overrideDescending; overridePending = false; }
        else if (less(a[i - 1], a[i])) descendingMode = false;
        else if (less(a[i], a[i - 1])) descendingMode = true;

        if (!insert(i)) {
            if (metrics) {
                metrics->ascPiles = ascCount; metrics->descPiles = descCount;
                metrics->pileCapacityFallback = true;
            }
            return ClassifyResult::FallbackCapacity;
        }
        if (n >= 10000 && i + 1 == 64 && ascCount >= 6 && descCount >= 6) {
            if (metrics) {
                metrics->ascPiles = ascCount; metrics->descPiles = descCount;
                metrics->earlyRandomFallback = true;
            }
            return ClassifyResult::FallbackRandom;
        }
    }
    if (metrics) { metrics->ascPiles = ascCount; metrics->descPiles = descCount; }
    return ClassifyResult::Accept;
}

template <class It, class Less>
void mergeRecursiveNoAlloc(It first, It middle, It last, Less less) {
    const auto n1 = middle - first, n2 = last - middle;
    if (n1 == 0 || n2 == 0) return;
    if (!less(*middle, *(middle - 1))) return;
    if (!less(*first, *(last - 1))) { std::rotate(first, middle, last); return; }
    if (n1 + n2 == 2) { if (less(*middle, *first)) std::iter_swap(first, middle); return; }
    It firstCut, secondCut;
    if (n1 > n2) {
        firstCut = first + n1 / 2;
        secondCut = std::lower_bound(middle, last, *firstCut, less);
    } else {
        secondCut = middle + n2 / 2;
        firstCut = std::upper_bound(first, middle, *secondCut,
            [&](const auto& value, const auto& element) { return less(value, element); });
    }
    It newMiddle = std::rotate(firstCut, middle, secondCut);
    mergeRecursiveNoAlloc(first, firstCut, newMiddle, less);
    mergeRecursiveNoAlloc(newMiddle, secondCut, last, less);
}

template <class T, class Less>
void mergeNaturalRunsNoAlloc(std::vector<T>& a,
                             std::array<allocation_free_v10::RunDesc, kMaxRuns>& runs,
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
            mergeRecursiveNoAlloc(a.begin() + static_cast<std::ptrdiff_t>(begin),
                                  a.begin() + static_cast<std::ptrdiff_t>(middle),
                                  a.begin() + static_cast<std::ptrdiff_t>(end), less);
            runs[out++] = {begin, end, false};
        }
        runCount = out;
    }
}

template <class T, class Less>
bool repeatedOverlapGeometry(const std::vector<T>& a,
                             const std::array<allocation_free_v10::RunDesc, kMaxRuns>& runs,
                             std::size_t runCount,
                             Less less) {
    if (runCount < 8) return false;
    for (std::size_t r = 0; r + 1 < runCount; ++r) {
        const auto& L = runs[r]; const auto& R = runs[r + 1];
        const T& l0 = a[L.begin]; const T& l1 = a[L.end - 1];
        const T& r0 = a[R.begin]; const T& r1 = a[R.end - 1];
        const T& lmin = less(l1, l0) ? l1 : l0;
        const T& lmax = less(l1, l0) ? l0 : l1;
        const T& rmin = less(r1, r0) ? r1 : r0;
        const T& rmax = less(r1, r0) ? r0 : r1;
        if (!less(rmin, lmax) || !less(lmin, rmax)) return false;
    }
    return true;
}



struct First65RunProfile {
    std::size_t span = 0;
    bool allOverlap = false;
};

template <class T, class Less>
First65RunProfile profileFirst65Runs(const std::vector<T>& a, Less less) {
    std::size_t i = 0;
    std::size_t rawRunCount = 0;
    allocation_free_v10::RunDesc previous{};
    bool havePrevious = false;
    bool allOverlap = true;

    while (i < a.size() && rawRunCount < kMaxRuns + 1) {
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
        const allocation_free_v10::RunDesc current{begin, i, descending};
        if (havePrevious) {
            const T& l0 = a[previous.begin];
            const T& l1 = a[previous.end - 1];
            const T& r0 = a[current.begin];
            const T& r1 = a[current.end - 1];
            const T& lmin = less(l1, l0) ? l1 : l0;
            const T& lmax = less(l1, l0) ? l0 : l1;
            const T& rmin = less(r1, r0) ? r1 : r0;
            const T& rmax = less(r1, r0) ? r0 : r1;
            if (!less(rmin, lmax) || !less(lmin, rmax)) allOverlap = false;
        }
        previous = current;
        havePrevious = true;
        ++rawRunCount;
    }
    if (rawRunCount != kMaxRuns + 1) return {};
    return {i, allOverlap};
}

template <class T, class Less>
void normalizeRunNoAlloc(std::vector<T>& a, allocation_free_v10::RunDesc& run, Less) {
    if (!run.descending) return;
    std::reverse(a.begin() + static_cast<std::ptrdiff_t>(run.begin),
                 a.begin() + static_cast<std::ptrdiff_t>(run.end));
    run.descending = false;
}

template <class T, class Less>
void reclaimSmallestAdjacentRun(std::vector<T>& a,
                                std::array<allocation_free_v10::RunDesc, kMaxRuns>& runs,
                                std::size_t& runCount,
                                Less less) {
    std::size_t best = 0;
    std::size_t bestCombined = (runs[0].end - runs[0].begin) + (runs[1].end - runs[1].begin);
    for (std::size_t r = 1; r + 1 < runCount; ++r) {
        const std::size_t combined =
            (runs[r].end - runs[r].begin) + (runs[r + 1].end - runs[r + 1].begin);
        if (combined < bestCombined) {
            best = r;
            bestCombined = combined;
        }
    }

    normalizeRunNoAlloc(a, runs[best], less);
    normalizeRunNoAlloc(a, runs[best + 1], less);
    const std::size_t begin = runs[best].begin;
    const std::size_t middle = runs[best].end;
    const std::size_t end = runs[best + 1].end;
    mergeRecursiveNoAlloc(a.begin() + static_cast<std::ptrdiff_t>(begin),
                          a.begin() + static_cast<std::ptrdiff_t>(middle),
                          a.begin() + static_cast<std::ptrdiff_t>(end), less);
    runs[best] = {begin, end, false};
    for (std::size_t r = best + 1; r + 1 < runCount; ++r) runs[r] = runs[r + 1];
    --runCount;
}

template <class T, class Less>
std::size_t discoverNaturalRunsWithReclamation(
    std::vector<T>& a,
    std::array<allocation_free_v10::RunDesc, kMaxRuns>& runs,
    std::size_t& runCount,
    Less less,
    std::size_t* reclaimCount = nullptr) {
    std::size_t rawRunCount = 0;
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
            reclaimSmallestAdjacentRun(a, runs, runCount, less);
            if (reclaimCount) ++*reclaimCount;
        }
        runs[runCount++] = {begin, i, descending};
        ++rawRunCount;
    }
    return rawRunCount;
}

template <class T, class Less>
JESSESORT_V11_NOINLINE void handleRunCapacityOverflow(
    std::vector<T>& a,
    std::array<allocation_free_v10::RunDesc, kMaxRuns>& runs,
    std::size_t& runCount,
    Less less,
    Metrics* metrics) {
    constexpr std::size_t kMinAverageRunForReclamation = 1024;
    const First65RunProfile first65 = profileFirst65Runs(a, less);
    if (first65.span < (kMaxRuns + 1) * kMinAverageRunForReclamation || first65.allOverlap) {
        if (metrics) {
            metrics->naturalRuns = kMaxRuns + 1;
            metrics->runCapacityFallback = true;
            metrics->repeatedOverlapFallback = first65.allOverlap;
        }
        allocation_free_v10::fallbackNoAlloc(a, less);
        return;
    }

    std::size_t reclaims = 0;
    const std::size_t rawRuns = discoverNaturalRunsWithReclamation(a, runs, runCount, less, &reclaims);
    if (metrics) {
        metrics->naturalRuns = rawRuns;
        metrics->streamedRunPath = true;
        metrics->runReclaims = reclaims;
    }
    mergeNaturalRunsNoAlloc(a, runs, runCount, less);
}

template <class T, class Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool tryEarlyRepeatedOverlapRoute(std::vector<T>& a, Less less, Metrics* metrics) {
    const std::size_t n = a.size();
    if (n < 256) return false;

    int previousDirection = 0;
    std::size_t directionChanges = 0;
    std::size_t previousIndex = 0;
    for (std::size_t s = 1; s <= 8; ++s) {
        const std::size_t index = (s * (n - 1)) / 8;
        int direction = 0;
        if (less(a[previousIndex], a[index])) direction = 1;
        else if (less(a[index], a[previousIndex])) direction = -1;
        if (direction != 0) {
            if (previousDirection != 0 && direction != previousDirection) ++directionChanges;
            previousDirection = direction;
        }
        previousIndex = index;
    }
    if (directionChanges < 3) return false;

    std::size_t end = 1;
    if (!less(a[0], a[1])) return false;
    end = 2;
    while (end < n && !less(a[end], a[end - 1])) ++end;
    if (end < 32 || end > n / 2 || end + 1 >= n) return false;

    const std::size_t secondEnd = end * 2;
    const auto equivalent = [&](const T& x, const T& y) {
        return !less(x, y) && !less(y, x);
    };
    if (!less(a[end], a[end - 1]) || !less(a[end], a[end + 1]) ||
        !equivalent(a[0], a[end]) || !equivalent(a[end - 1], a[secondEnd - 1]) ||
        (secondEnd < n &&
         (!less(a[secondEnd], a[secondEnd - 1]) || !equivalent(a[0], a[secondEnd]))))
        return false;

    if (metrics) metrics->earlyRepeatedOverlapFallback = true;
    allocation_free_v10::fallbackNoAlloc(a, less);
    return true;
}

template <class T, class Less = std::less<T>>
void sortWithMetrics(std::vector<T>& a, Less less = Less{}, Metrics* metrics = nullptr) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "V11 requires movable values");
    if (a.size() < 2) return;
    if (jessesort::detail::tryTinyInsertionSort(a, less)) return;

    bool specialValuePrefixMayMix = true;
    if (a.size() >= 4) {
        const bool aa = less(a[0], a[1]) && less(a[1], a[2]) && less(a[2], a[3]);
        const bool dd = less(a[1], a[0]) && less(a[2], a[1]) && less(a[3], a[2]);
        specialValuePrefixMayMix = !(aa || dd);
    }
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (specialValuePrefixMayMix && jessesort::simulated_legacy::trySpecializedPrePatienceRoutes(a, less)) {
            if (metrics) metrics->specializedDirect = true;
            return;
        }
    }

    if (tryEarlyRepeatedOverlapRoute(a, less, metrics)) return;

    const ClassifyResult classified = classifySharedBoundedPatience(a, less, metrics);
    if (classified == ClassifyResult::SortedAscending) return;
    if (classified == ClassifyResult::SortedDescending) { std::reverse(a.begin(), a.end()); return; }
    if (classified != ClassifyResult::Accept) { allocation_free_v10::fallbackNoAlloc(a, less); return; }

    std::array<allocation_free_v10::RunDesc, kMaxRuns> runs{};
    std::size_t runCount = 0;
    if (!allocation_free_v10::discoverNaturalRuns(a, runs, runCount, less)) {
        handleRunCapacityOverflow(a, runs, runCount, less, metrics);
        return;
    }
    if (metrics) metrics->naturalRuns = runCount;

    if (allocation_free_v10::cheapMergeGeometryOnly(a, runs, runCount, less)) {
        if (metrics) metrics->cheapRunPath = true;
        allocation_free_v10::sortCheapNaturalRunsNoAlloc(a, runs, runCount, less);
        return;
    }
    if (repeatedOverlapGeometry(a, runs, runCount, less)) {
        if (metrics) metrics->repeatedOverlapFallback = true;
        allocation_free_v10::fallbackNoAlloc(a, less); return;
    }
    if (metrics) metrics->generalRunPath = true;
    mergeNaturalRunsNoAlloc(a, runs, runCount, less);
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) { sortWithMetrics(a, less, nullptr); }

#undef JESSESORT_V11_NOINLINE

} // namespace jessesort::allocation_free_v11

#endif
