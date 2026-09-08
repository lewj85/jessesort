#ifndef JESSESORT_E342_NOALLOC_DIRECT_H
#define JESSESORT_E342_NOALLOC_DIRECT_H

#include <jessesort/detail/routing/sparse_middle_density.h>
#include <jessesort/detail/decomposition/noalloc/low_run_merge.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <new>
#include <span>
#include <type_traits>
#include <vector>

namespace jessesort::allocation_free_direct {

template <class It, class T, class Less>
__attribute__((noinline))
void mergeBufferedLeafNoAlloc(It first, It middle, It last, Less less, T* scratch) {
    const auto n1 = middle - first, n2 = last - middle;
    if (n1 <= n2) {
        const std::size_t leftN = static_cast<std::size_t>(n1);
        for (std::size_t i = 0; i < leftN; ++i)
            ::new (static_cast<void*>(scratch + i)) T(std::move(*(first + static_cast<std::ptrdiff_t>(i))));
        std::size_t li = 0;
        It r = middle;
        It out = first;
        auto doForwardMerge = [&] {
            while (li < leftN && r != last) {
                if (less(*r, scratch[li])) {
                    if (out != r) *out = std::move(*r);
                    ++r;
                } else {
                    *out = std::move(scratch[li++]);
                }
                ++out;
            }
            while (li < leftN) *out++ = std::move(scratch[li++]);
            while (r != last) {
                if (out != r) *out = std::move(*r);
                ++out; ++r;
            }
        };
        if constexpr (noexcept(less(*r, scratch[li]))) {
            doForwardMerge();
        } else {
            try {
                doForwardMerge();
            } catch (...) {
                It hole = out;
                for (std::size_t i = li; i < leftN; ++i)
                    *hole++ = std::move(scratch[i]);
                for (std::size_t i = 0; i < leftN; ++i) scratch[i].~T();
                throw;
            }
        }
        for (std::size_t i = 0; i < leftN; ++i) scratch[i].~T();
    } else {
        const std::size_t rightN = static_cast<std::size_t>(n2);
        for (std::size_t i = 0; i < rightN; ++i)
            ::new (static_cast<void*>(scratch + i)) T(std::move(*(middle + static_cast<std::ptrdiff_t>(i))));
        std::size_t ri = rightN;
        It l = middle;
        It out = last;
        auto doBackwardMerge = [&] {
            while (l != first && ri != 0) {
                --out;
                if (less(scratch[ri - 1], *(l - 1))) {
                    --l;
                    if (out != l) *out = std::move(*l);
                } else {
                    *out = std::move(scratch[--ri]);
                }
            }
            while (ri != 0) { --out; *out = std::move(scratch[--ri]); }
        };
        if constexpr (noexcept(less(scratch[ri - 1], *(l - 1)))) {
            doBackwardMerge();
        } else {
            try {
                doBackwardMerge();
            } catch (...) {
                It hole = l;
                for (std::size_t i = 0; i < ri; ++i)
                    *hole++ = std::move(scratch[i]);
                for (std::size_t i = 0; i < rightN; ++i) scratch[i].~T();
                throw;
            }
        }
        for (std::size_t i = 0; i < rightN; ++i) scratch[i].~T();
    }
}

template <class It, class T, class Less>
void mergeRecursiveLeafBufferedNoAlloc(It first, It middle, It last, Less less,
                                       T* scratch, std::size_t scratchCapacity,
                                       bool allowLeafBuffer) {
    const auto n1 = middle - first, n2 = last - middle;
    if (n1 == 0 || n2 == 0) return;
    if (!less(*middle, *(middle - 1))) return;
    if (!less(*first, *(last - 1))) { std::rotate(first, middle, last); return; }
    if (n1 + n2 == 2) { if (less(*middle, *first)) std::iter_swap(first, middle); return; }

    if (allowLeafBuffer && static_cast<std::size_t>(n1 + n2) <= scratchCapacity) {
        mergeBufferedLeafNoAlloc(first, middle, last, less, scratch);
        return;
    }

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
    mergeRecursiveLeafBufferedNoAlloc(first, firstCut, newMiddle, less,
                                      scratch, scratchCapacity, true);
    mergeRecursiveLeafBufferedNoAlloc(newMiddle, secondCut, last, less,
                                      scratch, scratchCapacity, true);
}

template <class It, class Less>
void mergeTopLevelTrimmedMatureNoAlloc(It first, It middle, It last, Less less) {
    if (first == middle || middle == last) return;
    if (!less(*middle, *(middle - 1))) return;
    if (!less(*first, *(last - 1))) { std::rotate(first, middle, last); return; }
    first = std::upper_bound(first, middle, *middle, less);
    last = std::lower_bound(middle, last, *(middle - 1), less);
    if (first == middle || middle == last) return;
    jessesort::allocation_free_low_run::mergeRecursiveNoAlloc(first, middle, last, less);
}

template <class T, class Less>
void mergePairLeafBufferedNoAlloc(std::vector<T>& a, std::size_t begin,
                                  std::size_t middle, std::size_t end, Less less) {
    constexpr std::size_t kScratchBytes = 8192;
    constexpr std::size_t kCapacity = kScratchBytes / sizeof(T);
    if constexpr (kCapacity >= 2) {
        using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
        std::array<Storage, kCapacity> storage;
        T* scratch = reinterpret_cast<T*>(storage.data());
        auto first = a.begin() + static_cast<std::ptrdiff_t>(begin);
        auto mid = a.begin() + static_cast<std::ptrdiff_t>(middle);
        auto last = a.begin() + static_cast<std::ptrdiff_t>(end);
        {
            if (first == mid || mid == last) return;
            if (!less(*mid, *(mid - 1))) return;
            if (!less(*first, *(last - 1))) { std::rotate(first, mid, last); return; }
            first = std::upper_bound(first, mid, *mid, less);
            last = std::lower_bound(mid, last, *(mid - 1), less);
            if (first == mid || mid == last) return;
        }
        mergeRecursiveLeafBufferedNoAlloc(first, mid, last,
                                          less, scratch, kCapacity, false);
    } else {
        auto first = a.begin() + static_cast<std::ptrdiff_t>(begin);
        auto mid = a.begin() + static_cast<std::ptrdiff_t>(middle);
        auto last = a.begin() + static_cast<std::ptrdiff_t>(end);
        mergeTopLevelTrimmedMatureNoAlloc(first, mid, last, less);
    }
}

template <class T, class Less>
void mergeNaturalRunsLeafBufferedNoAlloc(
    std::vector<T>& a,
    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns>& runs,
    std::size_t runCount, Less less) {
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
            mergePairLeafBufferedNoAlloc(a, begin, middle, end, less);
            runs[out++] = {begin, end, false};
        }
        runCount = out;
    }
}

template <class T, class Less>
void mergeNaturalRunsTopLevelTrimmedNoAlloc(
    std::vector<T>& a,
    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns>& runs,
    std::size_t runCount, Less less) {
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
            mergeTopLevelTrimmedMatureNoAlloc(
                a.begin() + static_cast<std::ptrdiff_t>(begin),
                a.begin() + static_cast<std::ptrdiff_t>(middle),
                a.begin() + static_cast<std::ptrdiff_t>(end), less);
            runs[out++] = {begin, end, false};
        }
        runCount = out;
    }
}

template <class T, class Less>
bool tryLongNaturalRunDirectNoAlloc(std::vector<T>& a, Less less) {
    constexpr std::size_t kMinN = 10000;
    constexpr std::size_t kPrefix = 64;
    const std::size_t n = a.size();
    if (n < kMinN) return false;

    // Same structural prerequisite used by the mature direct long-run route:
    // the first 64 values must be nondecreasing.
    for (std::size_t i = 1; i < kPrefix; ++i)
        if (less(a[i], a[i - 1])) return false;

    // A globally monotone array is handled more cheaply by the base sorter.
    // Require a coarse drop somewhere beyond the prefix before doing full
    // natural-run discovery.
    bool coarseDrop = false;
    std::size_t previous = 0;
    for (std::size_t sample = 1; sample < 9; ++sample) {
        const std::size_t index = ((n - 1) * sample) / 8;
        if (less(a[index], a[previous])) {
            coarseDrop = true;
            break;
        }
        previous = index;
    }
    if (!coarseDrop) return false;

    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns> runs{};
    std::size_t runCount = 0;
    if (!jessesort::allocation_free_bounded::discoverNaturalRuns(a, runs, runCount, less))
        return false;
    if (runCount <= 1) return false;

    // Preserve the mature noalloc economics policy. Only claim the direct
    // route once we know which existing allocation-free merge path is valid.
    if (jessesort::allocation_free_bounded::cheapMergeGeometryOnly(a, runs, runCount, less)) {
        jessesort::allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a, runs, runCount, less);
    } else {
        if (jessesort::allocation_free_low_run::repeatedOverlapGeometry(a, runs, runCount, less))
            return false;
        bool hasDescendingRun = false;
        for (std::size_t r = 0; r < runCount; ++r) hasDescendingRun |= runs[r].descending;
        if (hasDescendingRun)
            mergeNaturalRunsLeafBufferedNoAlloc(a, runs, runCount, less);
        else
            mergeNaturalRunsTopLevelTrimmedNoAlloc(a, runs, runCount, less);
    }
    return true;
}



template <class It>
void gatherEvenPositionsFirst(It first, std::size_t n) {
    using T = typename std::iterator_traits<It>::value_type;
    constexpr std::size_t kLocalScratchBytes = 16384;
    constexpr std::size_t kLocalCapacity = kLocalScratchBytes / sizeof(T);
#ifndef JESSESORT_E359_DISABLE_LOCAL_GATHER
    // E359: once recursion reaches a page-sized local block, finish the
    // stable parity deinterleave directly in bounded stack scratch. This is
    // size-generic (not tied to canonical n), heap-free, and move-only-safe.
    if constexpr (kLocalCapacity >= 4) {
        if (n <= kLocalCapacity) {
            using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
            std::array<Storage, kLocalCapacity> scratch;
            T* tmp = reinterpret_cast<T*>(scratch.data());
            std::size_t out = 0;
            for (std::size_t i = 0; i < n; i += 2, ++out)
                ::new (static_cast<void*>(tmp + out)) T(std::move(*(first + static_cast<std::ptrdiff_t>(i))));
            for (std::size_t i = 1; i < n; i += 2, ++out)
                ::new (static_cast<void*>(tmp + out)) T(std::move(*(first + static_cast<std::ptrdiff_t>(i))));
            for (std::size_t i = 0; i < n; ++i) {
                *(first + static_cast<std::ptrdiff_t>(i)) = std::move(tmp[i]);
                tmp[i].~T();
            }
            return;
        }
    }
#endif
    if (n <= 2) return;
    if (n == 3) {
        // [e0,o0,e1] -> [e0,e1,o0]
        std::rotate(first + 1, first + 2, first + 3);
        return;
    }
    // Split at an even source index so parity in the right half matches the
    // global parity. After recursively deinterleaving both halves, rotate the
    // left odd lane behind the right even lane.
    std::size_t leftN = (n / 2) & ~std::size_t{1};
    if (leftN < 2) leftN = 2;
    if (leftN >= n) leftN = n - (n & 1u ? 1u : 2u);
    const std::size_t rightN = n - leftN;
    gatherEvenPositionsFirst(first, leftN);
    gatherEvenPositionsFirst(first + static_cast<std::ptrdiff_t>(leftN), rightN);
    const std::size_t leftEven = (leftN + 1) / 2;
    const std::size_t rightEven = (rightN + 1) / 2;
    std::rotate(first + static_cast<std::ptrdiff_t>(leftEven),
                first + static_cast<std::ptrdiff_t>(leftN),
                first + static_cast<std::ptrdiff_t>(leftN + rightEven));
}

// E727: allocation-free realization of the E726 middle-density sparse route.
// Admission is shared exactly with simulated-direct_live-phase; only execution
// differs to satisfy this pipeline's allocation contract.
template <class T, class Less>
inline bool sparseMiddleDensityCandidateNoAlloc(const std::vector<T>& a, Less less) {
    return jessesort::detail::sparseMiddleDensityCandidate<T, Less>(
        std::span<const T>(a.data(), a.size()), less);
}

template <class T, class Less>
bool trySparseDistributedDisorderDirectNoAlloc(std::vector<T>& a, Less less) {
    if (!sparseMiddleDensityCandidateNoAlloc(a, less)) return false;
    constexpr std::size_t kBlock = 128;
    const std::size_t n = a.size();
    for (std::size_t begin = 0; begin < n; begin += kBlock) {
        const std::size_t end = std::min(n, begin + kBlock);
        for (std::size_t i = begin + 1; i < end; ++i) {
            T value = std::move(a[i]);
            std::size_t j = i;
            // E369: Rust-style comparator-panic safety. For noexcept
            // comparators compile the original hot loop unchanged. Otherwise
            // restore the extracted value into the current hole if `less`
            // unwinds. (Rust moves cannot throw; the equivalent C++ guarantee
            // assumes nonthrowing move construction/assignment.)
            auto doInsertion = [&] {
                while (j > begin && less(value, a[j - 1])) {
                    a[j] = std::move(a[j - 1]);
                    --j;
                }
            };
            if constexpr (noexcept(less(value, a[j - 1]))) {
                doInsertion();
            } else {
                try {
                    doInsertion();
                } catch (...) {
                    a[j] = std::move(value);
                    throw;
                }
            }
            a[j] = std::move(value);
        }
    }
    for (std::size_t width = kBlock; width < n; ) {
        for (std::size_t begin = 0; begin < n; begin += 2 * width) {
            const std::size_t middle = std::min(n, begin + width);
            const std::size_t end = std::min(n, begin + 2 * width);
            if (middle < end)
                mergePairLeafBufferedNoAlloc(a, begin, middle, end, less);
        }
        if (width > n / 2) break;
        width *= 2;
    }
    return true;
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& a, Less less = Less{}) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "allocation-free direct JesseSort requires movable values");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");
    if (a.size() < 2) return;
    if (jessesort::detail::tryTinyInsertionSort(a, less)) return;

    // E727 production-policy rebase: do not inherit the historical E661
    // low-cardinality counting route or other simulated_legacy specialist routes.
    // The noalloc production APIs now share the E726 generic sparse-disorder
    // admission policy with simulated-direct_live-phase.

    // E218 must keep precedence over the long-run import: repeated-overlap
    // sawtooth geometry is cheaper through its dedicated noalloc fallback.
    if (jessesort::allocation_free_low_run::tryEarlyRepeatedOverlapRoute(a, less, nullptr)) return;

    if (a.size() >= 10000) {
        const bool inv1 = less(a[1], a[0]);
        const bool inv2 = less(a[2], a[1]);
        // E727: the old two-lane Alternating shortcut is intentionally not part
        // of the production noalloc policy. Preserve only the generic long-run
        // route when the prefix already agrees on ascending direction.
        if (inv1 == inv2 && !inv1) {
            if (tryLongNaturalRunDirectNoAlloc(a, less)) return;
        }
        if (trySparseDistributedDisorderDirectNoAlloc(a, less)) return;
    }

    const auto classified =
        jessesort::allocation_free_low_run::classifySharedBoundedPatience(a, less, nullptr);
    if (classified == jessesort::allocation_free_low_run::ClassifyResult::SortedAscending) return;
    if (classified == jessesort::allocation_free_low_run::ClassifyResult::SortedDescending) {
        std::reverse(a.begin(), a.end()); return;
    }
    if (classified != jessesort::allocation_free_low_run::ClassifyResult::Accept) {
        jessesort::allocation_free_bounded::fallbackNoAlloc(a, less); return;
    }

    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns> runs{};
    std::size_t runCount = 0;
    if (!jessesort::allocation_free_bounded::discoverNaturalRuns(a, runs, runCount, less)) {
        jessesort::allocation_free_low_run::handleRunCapacityOverflow(
            a, runs, runCount, less, nullptr);
        return;
    }
    if (jessesort::allocation_free_bounded::cheapMergeGeometryOnly(a, runs, runCount, less)) {
        jessesort::allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a, runs, runCount, less);
        return;
    }
    if (jessesort::allocation_free_low_run::repeatedOverlapGeometry(a, runs, runCount, less)) {
        jessesort::allocation_free_bounded::fallbackNoAlloc(a, less); return;
    }
    jessesort::allocation_free_low_run::mergeNaturalRunsNoAlloc(a, runs, runCount, less);
}

} // namespace jessesort::allocation_free_direct

#endif
