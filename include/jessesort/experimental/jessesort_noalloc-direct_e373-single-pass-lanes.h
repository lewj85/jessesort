#ifndef JESSESORT_E373_SINGLE_PASS_LANES_H
#define JESSESORT_E373_SINGLE_PASS_LANES_H

#include <jessesort/experimental/jessesort_noalloc-low-run-merge_overlap-routed_inplace-adaptive_run-reclaim64.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <new>
#include <type_traits>
#include <vector>

namespace jessesort::allocation_free_direct {

struct Metrics {
    bool directLongRun = false;
};

inline bool e360EnableLeafBufferedMerge = true;
inline bool e344EnableTopLevelOverlapTrim = true;

inline bool e368EnableSparseDisorderDirect = true;
inline volatile bool e373SinglePassLaneAnalysis = true;

template <class It, class T, class Less>
void mergeRecursiveLeafBufferedNoAlloc(It first, It middle, It last, Less less,
                                       T* scratch, std::size_t scratchCapacity,
                                       bool allowLeafBuffer) {
    const auto n1 = middle - first, n2 = last - middle;
    if (n1 == 0 || n2 == 0) return;
    if (!less(*middle, *(middle - 1))) return;
    if (!less(*first, *(last - 1))) { std::rotate(first, middle, last); return; }
    if (n1 + n2 == 2) { if (less(*middle, *first)) std::iter_swap(first, middle); return; }

    if (allowLeafBuffer && e360EnableLeafBufferedMerge &&
        static_cast<std::size_t>(n1 + n2) <= scratchCapacity) {
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
                    // E369: scratch[li,leftN) corresponds exactly to the
                    // contiguous hole [out,r). Restore it before unwinding.
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
                    // E369: scratch[0,ri) corresponds exactly to [l,out).
                    It hole = l;
                    for (std::size_t i = 0; i < ri; ++i)
                        *hole++ = std::move(scratch[i]);
                    for (std::size_t i = 0; i < rightN; ++i) scratch[i].~T();
                    throw;
                }
            }
            for (std::size_t i = 0; i < rightN; ++i) scratch[i].~T();
        }
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
        if (e344EnableTopLevelOverlapTrim) {
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
        if (e344EnableTopLevelOverlapTrim)
            mergeTopLevelTrimmedMatureNoAlloc(first, mid, last, less);
        else
            jessesort::allocation_free_low_run::mergeRecursiveNoAlloc(first, mid, last, less);
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
bool tryLongNaturalRunDirectNoAlloc(std::vector<T>& a, Less less, Metrics* metrics = nullptr) {
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
        if (hasDescendingRun && e360EnableLeafBufferedMerge)
            mergeNaturalRunsLeafBufferedNoAlloc(a, runs, runCount, less);
        else if (e344EnableTopLevelOverlapTrim)
            mergeNaturalRunsTopLevelTrimmedNoAlloc(a, runs, runCount, less);
        else
            jessesort::allocation_free_low_run::mergeNaturalRunsNoAlloc(a, runs, runCount, less);
    }
    if (metrics) metrics->directLongRun = true;
    return true;
}



template <class It>
void gatherEvenPositionsFirst(It first, std::size_t n) {
    using T = typename std::iterator_traits<It>::value_type;
    constexpr std::size_t kLocalScratchBytes = 4096;
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

template <class T, class Less>
bool hasTwoLaneZigzagStructureNoAlloc(const std::vector<T>& a, Less less, int* evenDirOut = nullptr, int* oddDirOut = nullptr) {
    constexpr std::size_t kMinN = 10000;
    constexpr std::size_t kPrefixComparisons = 8;
    constexpr std::size_t kDistributedChecks = 6;
    const std::size_t n = a.size();
    if (n < kMinN) return false;

    auto direction = [&](std::size_t x, std::size_t y) -> int {
        if (less(a[x], a[y])) return 1;
        if (less(a[y], a[x])) return -1;
        return 0;
    };
    int previous = direction(0, 1);
    if (previous == 0) return false;
    for (std::size_t i = 1; i < kPrefixComparisons; ++i) {
        const int current = direction(i, i + 1);
        if (current == 0 || current == previous) return false;
        previous = current;
    }
    const int firstDirection = direction(0, 1);
    for (std::size_t sample = 1; sample <= kDistributedChecks; ++sample) {
        std::size_t i = ((n - 3) * sample) / (kDistributedChecks + 1);
        if (i + 2 >= n) i = n - 3;
        const int expected0 = (i & 1u) ? -firstDirection : firstDirection;
        if (direction(i, i + 1) != expected0 || direction(i + 1, i + 2) != -expected0)
            return false;
    }

    if (!e373SinglePassLaneAnalysis) {
        auto laneDirection = [&](std::size_t lane) -> int {
            for (std::size_t i = lane + 2; i < n; i += 2) {
                if (less(a[i - 2], a[i])) return 1;
                if (less(a[i], a[i - 2])) return -1;
            }
            return 1;
        };
        const int evenDir = laneDirection(0), oddDir = laneDirection(1);
        for (std::size_t lane = 0; lane < 2; ++lane) {
            const int dir = lane == 0 ? evenDir : oddDir;
            for (std::size_t i = lane + 2; i < n; i += 2) {
                if (dir > 0) { if (less(a[i], a[i - 2])) return false; }
                else { if (less(a[i - 2], a[i])) return false; }
            }
        }
        if (evenDirOut) *evenDirOut = evenDir;
        if (oddDirOut) *oddDirOut = oddDir;
        return true;
    }

    auto discoverLane = [&](std::size_t lane, int& dir, std::size_t& strictAt) {
        dir = 1;
        strictAt = n;
        for (std::size_t i = lane + 2; i < n; i += 2) {
            if (less(a[i - 2], a[i])) { dir = 1; strictAt = i; return; }
            if (less(a[i], a[i - 2])) { dir = -1; strictAt = i; return; }
        }
    };
    auto verifyLaneAfter = [&](std::size_t lane, int dir, std::size_t strictAt) -> bool {
        const std::size_t start = strictAt == n ? n : strictAt + 2;
        for (std::size_t i = start; i < n; i += 2) {
            if (dir > 0) { if (less(a[i], a[i - 2])) return false; }
            else { if (less(a[i - 2], a[i])) return false; }
        }
        return true;
    };
    int evenDir = 1, oddDir = 1;
    std::size_t evenStrict = n, oddStrict = n;
    discoverLane(0, evenDir, evenStrict);
    discoverLane(1, oddDir, oddStrict);
    if (!verifyLaneAfter(0, evenDir, evenStrict) || !verifyLaneAfter(1, oddDir, oddStrict)) return false;
    if (evenDirOut) *evenDirOut = evenDir;
    if (oddDirOut) *oddDirOut = oddDir;
    return true;
}

template <class T, class Less>
bool tryTwoLaneZigzagDirectNoAlloc(std::vector<T>& a, Less less) {
    int evenDir = 1, oddDir = 1;
    if (!hasTwoLaneZigzagStructureNoAlloc(a, less, &evenDir, &oddDir)) return false;
    const std::size_t n = a.size();
    if (!e373SinglePassLaneAnalysis) {
        auto laneDirection = [&](std::size_t lane) -> int {
            for (std::size_t i = lane + 2; i < n; i += 2) {
                if (less(a[i - 2], a[i])) return 1;
                if (less(a[i], a[i - 2])) return -1;
            }
            return 1;
        };
        evenDir = laneDirection(0);
        oddDir = laneDirection(1);
    }
    gatherEvenPositionsFirst(a.begin(), n);
    const std::size_t evenCount = (n + 1) / 2;
    if (evenDir < 0) std::reverse(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(evenCount));
    if (oddDir < 0) std::reverse(a.begin() + static_cast<std::ptrdiff_t>(evenCount), a.end());
    jessesort::allocation_free_low_run::mergeRecursiveNoAlloc(
        a.begin(), a.begin() + static_cast<std::ptrdiff_t>(evenCount), a.end(), less);
    return true;
}


// E368: allocation-free analogue of E232. Admission proof is intentionally
// identical to the retained simulated-direct E232 proof; only execution is
// changed to fixed 128-element insertion-sorted blocks followed by the
// maintained zero-heap adjacent in-place merge primitive.
template <class T, class Less>
inline bool hasSparseDistributedDisorderNoAlloc(const std::vector<T>& a, Less less) {
    constexpr std::size_t kMinN = 10000;
    constexpr std::size_t kWindow = 64;
    constexpr std::size_t kWindows = 8;
    const std::size_t n = a.size();
    if (n < kMinN) return false;
    unsigned prefixInversions = 0;
    for (std::size_t i = 1; i < 8; ++i)
        prefixInversions += static_cast<unsigned>(less(a[i], a[i - 1]));
    if (prefixInversions > 2) return false;

    unsigned totalInversions = 0, nonzeroWindows = 0;
    for (std::size_t w = 0; w < kWindows; ++w) {
        const std::size_t start = ((n - kWindow) * (2 * w + 1)) / (2 * kWindows);
        unsigned windowInversions = 0;
        for (std::size_t i = start + 1; i < start + kWindow; ++i)
            windowInversions += static_cast<unsigned>(less(a[i], a[i - 1]));
        if (windowInversions != 0) ++nonzeroWindows;
        if (windowInversions > 16) return false;
        totalInversions += windowInversions;
        if (totalInversions > 96) return false;
        const std::size_t remaining = kWindows - (w + 1);
        if (nonzeroWindows + remaining < 6) return false;
    }
    if (n >= 32768 && totalInversions > 36) return false;
    return nonzeroWindows >= 6 && totalInversions >= 6;
}

template <class T, class Less>
bool trySparseDistributedDisorderDirectNoAlloc(std::vector<T>& a, Less less) {
    if (!e368EnableSparseDisorderDirect || !hasSparseDistributedDisorderNoAlloc(a, less))
        return false;
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

    // Preserve noalloc-low-run's mature specialized-route precedence exactly. These routes
    // are already allocation-free for their eligible integral/default-less
    // domain and should not be displaced by the generic structural imports.
    bool specialValuePrefixMayMix = true;
    if (a.size() >= 4) {
        const bool aa = less(a[0], a[1]) && less(a[1], a[2]) && less(a[2], a[3]);
        const bool dd = less(a[1], a[0]) && less(a[2], a[1]) && less(a[3], a[2]);
        specialValuePrefixMayMix = !(aa || dd);
    }
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (specialValuePrefixMayMix &&
            jessesort::simulated_legacy::trySpecializedPrePatienceRoutes(a, less))
            return;
    }

    // E218 must keep precedence over the long-run import: repeated-overlap
    // sawtooth geometry is cheaper through its dedicated noalloc fallback.
    if (jessesort::allocation_free_low_run::tryEarlyRepeatedOverlapRoute(a, less, nullptr)) return;

    if (a.size() >= 10000) {
        const bool inv1 = less(a[1], a[0]);
        const bool inv2 = less(a[2], a[1]);
        if (inv1 != inv2) {
            if (tryTwoLaneZigzagDirectNoAlloc(a, less)) return;
        } else if (!inv1) {
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
