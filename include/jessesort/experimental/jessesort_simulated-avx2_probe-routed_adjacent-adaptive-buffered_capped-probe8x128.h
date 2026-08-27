#ifndef JESSESORT_E229_JESSESORT_SIMULATED_AVX2_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_CAPPED_PROBE8X128_H
#define JESSESORT_E229_JESSESORT_SIMULATED_AVX2_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_CAPPED_PROBE8X128_H

#include <jessesort/tiny_sort.h>
#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
#include <functional>
#include <type_traits>
#include <utility>
#include <climits>
#include <cassert>
#include <bit>


namespace jessesort::simulated_avx2_legacy {


// avx2 is the first SIMD-specific variation. It preserves simulated's overall pipeline
// and specializes the exact-eight int32 pile-tail lookup with one AVX2 compare.
// =========================================================
// Simulated JesseSort patience insertion + reconstruction + merge
// =========================================================
//
// Pipeline:
//   1. Simulate dual patience insertion using only tail/base arrays.
//   2. Store one packed blueprint entry per original element:
//        high bit = game ID, low bits = local pile ID.
//   3. Store per-game pile counts during simulation.
//   4. Reconstruct physically ascending flat runs into tmp.
//        - ascending-game piles preserve encounter order.
//        - descending-game piles reverse encounter order.
//   5. Return run boundaries for the reconstructed runs in tmp.
//   6. Merge adjacent run pairs bottom-up.
//        - first merge pass reads tmp and writes arr.
//        - later passes ping-pong between arr and tmp.
//        - odd leftover runs are copied/moved forward unchanged.
//        - already-ordered adjacent runs use a fast whole-span move.
//        - reverse-disjoint adjacent runs use a right-then-left fast path.
//   7. Ensure arr owns the final sorted result before returning.
//
// Important convention:
//   Ascending-game piles are ascending in encounter order, but their base/tail
//   array is descending.
//
//   Descending-game piles are descending in encounter order, but their base/tail
//   array is ascending.
//
// Reconstruction within each game:
//   ascending-game runs: preserve encounter order
//   descending-game runs: reverse encounter order
// Game grouping is always descending-game piles first, then ascending-game piles.
//
// Merge strategy:
//   Balanced/random-like run sets use adjacent bottom-up pairwise merging for a
//   simple sequential access pattern. Sparse-dominant and compact-imbalanced run
//   sets selected by run-size metadata use a PowerSort-style alphabetic tree.
//   The selector is variation-specific to ordinary simulated blueprint sorting.
//
// Stability:
//   The ordinary element-by-element merge path is stable, but the
//   reverse-disjoint fast path is not stable across equal keys. The overall sort
//   should therefore be treated as unstable.
//
// For floating-point types, NaNs are not supported by this logic because they
// break strict weak ordering.

// =========================================================
// Packed blueprint tags
// =========================================================
//
// high bit:
//   0 = ascending game
//   1 = descending game
//
// lower 31 bits:
//   local pile ID inside that game

static constexpr uint32_t DESC_BIT  = 0x80000000u;
static constexpr uint32_t PILE_MASK = 0x7fffffffu;
static constexpr uint32_t OVERFLOW_TAG = 0xffffffffu;

inline uint32_t makeAscTag(uint32_t localPile) {
    return localPile;
}

inline uint32_t makeDescTag(uint32_t localPile) {
    return DESC_BIT | localPile;
}

inline bool isDescTag(uint32_t tag) {
    return (tag & DESC_BIT) != 0;
}

inline bool isAscTag(uint32_t tag) {
    return (tag & DESC_BIT) == 0;
}

inline uint32_t localPileId(uint32_t tag) {
    return tag & PILE_MASK;
}

// =========================================================
// Result structs
// =========================================================

template <typename T>
struct SimulatedInsertionResult {
    std::vector<uint32_t> blueprint;

    // Counts are split while simulating because numAscPiles is not final until
    // insertion finishes. Reconstruction later selects the game grouping from
    // the final combined run count.
    std::vector<std::size_t> ascCounts;
    std::vector<std::size_t> descCounts;

    bool alreadySortedAscending = true;
    bool reverseSortedDescending = true;
    // Set from the first 64 input elements; refined later with final pile shape.
    bool earlyRandomLike = false;

    // Current pile tails. No artificial sentinel values are stored.
    std::vector<T> ascTails;   // ordered descending
    std::vector<T> descTails;  // ordered ascending

    // Optional avx2 probe-capping experiment. Values that would create a new
    // pile after a game reaches the configured probe cap are held here until
    // the 64-element routing decision is complete. The sorted overflow run is
    // then married into one existing ascending-game pile selected by its max.
    std::vector<T> probeOverflowSorted;
    std::size_t probeOverflowTargetAscPile = static_cast<std::size_t>(-1);
};

struct ReconstructedRuns {
    std::vector<std::size_t> runStart;
    bool sourceIsTmp = true;
};

// =========================================================
// Small helpers
// =========================================================

inline std::size_t estimatePileReserve(std::size_t n) {
    if (n == 0) return 1;

    const double root = std::sqrt(static_cast<double>(n));

    // Conservative per game. Over-reserving O(sqrt(n)) metadata is small
    // compared to n-sized value/blueprint arrays.
    return std::max<std::size_t>(
        32,
        static_cast<std::size_t>(2.0 * root) + 16
    );
}

// =========================================================
// Pile searches and sentinel-free simulated insertion
// =========================================================
//
// Tail arrays contain only real values. Their actual size is the pile count, so
// no numeric extrema or user-supplied sentinel objects are required.

template <typename T, typename Less = std::less<T>>
inline std::size_t findDescendingPileWithTails(
    const std::vector<T>& tails,
    std::size_t& hint,
    const T& value,
    Less less = Less{},
    bool enableSixteenAvx = false,
    bool enableWindowAvx = false
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        assert(hint < n);
        if (!less(tails[hint], value) &&
            (hint == 0 || less(tails[hint - 1], value))) {
            return hint;
        }
#if defined(__AVX2__)
#ifndef JESSESORT_V7_SIMD_MAX_PILES
#define JESSESORT_V7_SIMD_MAX_PILES 64
#endif
        if constexpr (std::is_same_v<T, int> && std::is_same_v<Less, std::less<int>>) {
            if (n == 8 || (enableSixteenAvx && n == 16)) {
                const __m256i vv = _mm256_set1_epi32(value);
                std::size_t base = 0;
                for (; base + 8 <= n; base += 8) {
                    const __m256i tv = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i*>(tails.data() + base));
                    // Descending-game tails are ascending. Find first tail >= value.
                    const __m256i lt = _mm256_cmpgt_epi32(vv, tv);
                    const unsigned mask = static_cast<unsigned>(
                        _mm256_movemask_ps(_mm256_castsi256_ps(lt))) & 0xffu;
                    if (mask != 0xffu) {
                        const unsigned first = static_cast<unsigned>(__builtin_ctz((~mask) & 0xffu));
                        hint = base + first;
                        return hint;
                    }
                }
                while (base < n && less(tails[base], value)) ++base;
                hint = base;
                return hint;
            }
            if (enableWindowAvx && n > 8) {
                std::size_t base = hint > 3 ? hint - 3 : 0;
                if (base + 8 > n) base = n - 8;
                const __m256i vv = _mm256_set1_epi32(value);
                const __m256i tv = _mm256_loadu_si256(
                    reinterpret_cast<const __m256i*>(tails.data() + base));
                const __m256i lt = _mm256_cmpgt_epi32(vv, tv);
                const unsigned mask = static_cast<unsigned>(
                    _mm256_movemask_ps(_mm256_castsi256_ps(lt))) & 0xffu;
                if (mask != 0xffu) {
                    const unsigned first = static_cast<unsigned>(__builtin_ctz((~mask) & 0xffu));
                    const std::size_t candidate = base + first;
                    if (first != 0 || base == 0 || less(tails[base - 1], value)) {
                        hint = candidate;
                        return hint;
                    }
                } else if (base + 8 == n) {
                    hint = n;
                    return hint;
                }
            }
        }
#endif
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(tails[static_cast<std::size_t>(next)], value)) {
                idx = next;
            }
        }
        hint = static_cast<std::size_t>(idx + 1);
        return hint;
    }

    if (hint < n && !less(tails[hint], value) &&
        (hint == 0 || less(tails[hint - 1], value))) return hint;

    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(tails[next], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    hint = static_cast<std::size_t>(idx + 1);
    return hint;
}

template <typename T, typename Less = std::less<T>>
inline std::size_t findAscendingPileWithTails(
    const std::vector<T>& tails,
    std::size_t& hint,
    const T& value,
    Less less = Less{},
    bool enableSixteenAvx = false,
    bool enableWindowAvx = false
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        assert(hint < n);
        if (!less(value, tails[hint]) &&
            (hint == 0 || less(value, tails[hint - 1]))) {
            return hint;
        }
#if defined(__AVX2__)
        if constexpr (std::is_same_v<T, int> && std::is_same_v<Less, std::less<int>>) {
            if (n == 8 || (enableSixteenAvx && n == 16)) {
                const __m256i vv = _mm256_set1_epi32(value);
                std::size_t base = 0;
                for (; base + 8 <= n; base += 8) {
                    const __m256i tv = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i*>(tails.data() + base));
                    // Ascending-game tails are descending. Find first tail <= value.
                    const __m256i gt = _mm256_cmpgt_epi32(tv, vv);
                    const unsigned mask = static_cast<unsigned>(
                        _mm256_movemask_ps(_mm256_castsi256_ps(gt))) & 0xffu;
                    if (mask != 0xffu) {
                        const unsigned first = static_cast<unsigned>(__builtin_ctz((~mask) & 0xffu));
                        hint = base + first;
                        return hint;
                    }
                }
                while (base < n && less(value, tails[base])) ++base;
                hint = base;
                return hint;
            }
            if (enableWindowAvx && n > 8) {
                std::size_t base = hint > 3 ? hint - 3 : 0;
                if (base + 8 > n) base = n - 8;
                const __m256i vv = _mm256_set1_epi32(value);
                const __m256i tv = _mm256_loadu_si256(
                    reinterpret_cast<const __m256i*>(tails.data() + base));
                const __m256i gt = _mm256_cmpgt_epi32(tv, vv);
                const unsigned mask = static_cast<unsigned>(
                    _mm256_movemask_ps(_mm256_castsi256_ps(gt))) & 0xffu;
                if (mask != 0xffu) {
                    const unsigned first = static_cast<unsigned>(__builtin_ctz((~mask) & 0xffu));
                    const std::size_t candidate = base + first;
                    if (first != 0 || base == 0 || less(value, tails[base - 1])) {
                        hint = candidate;
                        return hint;
                    }
                } else if (base + 8 == n) {
                    hint = n;
                    return hint;
                }
            }
        }
#endif
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(value, tails[static_cast<std::size_t>(next)])) {
                idx = next;
            }
        }
        hint = static_cast<std::size_t>(idx + 1);
        return hint;
    }

    if (hint < n && !less(value, tails[hint]) &&
        (hint == 0 || less(value, tails[hint - 1]))) return hint;

    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(value, tails[next])) idx = static_cast<std::ptrdiff_t>(next);
    }
    hint = static_cast<std::size_t>(idx + 1);
    return hint;
}

// Direct full pile searches used by the early Random-like insertion route.
// They intentionally skip the last-pile hint validation. E043 found that once
// the first 64 input elements already show substantial pile growth in both
// games, paying for a hint that almost always misses is slower than going
// directly to the bit-walk. The final pile layout is still analyzed separately
// before merge policy is selected.
template <typename T, typename Less = std::less<T>>
inline std::size_t findDescendingPileWithTailsNoHint(
    const std::vector<T>& tails,
    const T& value,
    Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(tails[static_cast<std::size_t>(next)], value)) {
                idx = next;
            }
        }
        return static_cast<std::size_t>(idx + 1);
    }

    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(tails[next], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <typename T, typename Less = std::less<T>>
inline std::size_t findAscendingPileWithTailsNoHint(
    const std::vector<T>& tails,
    const T& value,
    Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(value, tails[static_cast<std::size_t>(next)])) {
                idx = next;
            }
        }
        return static_cast<std::size_t>(idx + 1);
    }

    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(value, tails[next])) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

// The ordinary inline bit-walk is best when the hint misses frequently.
// When the hint succeeds almost every time, keeping the full search body in the
// caller still increases hot-loop code size. The split helpers below preserve
// the same search result while moving the rare miss path out of line.
#if defined(__GNUC__) || defined(__clang__)
#define JESSESORT_SEARCH_NOINLINE __attribute__((noinline))
#else
#define JESSESORT_SEARCH_NOINLINE
#endif

template <typename T, typename Less>
JESSESORT_SEARCH_NOINLINE std::size_t fullDescendingPileSearchSplit(
    const std::vector<T>& tails, const T& value, Less less
) {
    const std::size_t n = tails.size();
    if (n <= 256) {
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(tails[static_cast<std::size_t>(next)], value)) idx = next;
        }
        return static_cast<std::size_t>(idx + 1);
    }
    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(tails[next], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <typename T, typename Less>
JESSESORT_SEARCH_NOINLINE std::size_t fullAscendingPileSearchSplit(
    const std::vector<T>& tails, const T& value, Less less
) {
    const std::size_t n = tails.size();
    if (n <= 256) {
        int idx = -1;
        const int smallN = static_cast<int>(n);
        int step = 1 << (31 - __builtin_clz(static_cast<unsigned>(smallN)));
        for (; step != 0; step >>= 1) {
            const int next = idx + step;
            if (next < smallN && less(value, tails[static_cast<std::size_t>(next)])) idx = next;
        }
        return static_cast<std::size_t>(idx + 1);
    }
    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(value, tails[next])) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <typename T, typename Less = std::less<T>>
inline std::size_t findDescendingPileWithTailsSplit(
    const std::vector<T>& tails, std::size_t& hint, const T& value, Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;
    assert(hint < n);
    if (!less(tails[hint], value) && (hint == 0 || less(tails[hint - 1], value))) return hint;
    hint = fullDescendingPileSearchSplit(tails, value, less);
    return hint;
}

template <typename T, typename Less = std::less<T>>
inline std::size_t findAscendingPileWithTailsSplit(
    const std::vector<T>& tails, std::size_t& hint, const T& value, Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;
    assert(hint < n);
    if (!less(value, tails[hint]) && (hint == 0 || less(value, tails[hint - 1]))) return hint;
    hint = fullAscendingPileSearchSplit(tails, value, less);
    return hint;
}

#undef JESSESORT_SEARCH_NOINLINE

template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueDescendingPiles(
    std::vector<T>& tails,
    std::size_t& lastPileIndex,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& descCounts,
    Less less = Less{},
    bool enableWindowAvx = false
) {
    const std::size_t pileIndex = findDescendingPileWithTails(
        tails, lastPileIndex, value, less, false, enableWindowAvx);
    if (pileIndex < tails.size()) {
        tails[pileIndex] = value;
        ++descCounts[pileIndex];
    } else {
        assert(pileIndex <= PILE_MASK);
        tails.push_back(value);
        descCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = makeDescTag(static_cast<uint32_t>(pileIndex));
}

template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueAscendingPiles(
    std::vector<T>& tails,
    std::size_t& lastPileIndex,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& ascCounts,
    Less less = Less{},
    bool enableWindowAvx = false
) {
    const std::size_t pileIndex = findAscendingPileWithTails(
        tails, lastPileIndex, value, less, false, enableWindowAvx);
    if (pileIndex < tails.size()) {
        tails[pileIndex] = value;
        ++ascCounts[pileIndex];
    } else {
        assert(pileIndex <= PILE_MASK);
        tails.push_back(value);
        ascCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = makeAscTag(static_cast<uint32_t>(pileIndex));
}

template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueDescendingPilesSplit(
    std::vector<T>& tails, std::size_t& lastPileIndex, const T& value,
    std::size_t originalIndex, std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& descCounts, Less less = Less{}
) {
    const std::size_t pileIndex = findDescendingPileWithTailsSplit(tails, lastPileIndex, value, less);
    if (pileIndex < tails.size()) { tails[pileIndex] = value; ++descCounts[pileIndex]; }
    else { assert(pileIndex <= PILE_MASK); tails.push_back(value); descCounts.push_back(1); }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = makeDescTag(static_cast<uint32_t>(pileIndex));
}

template <typename T, typename Less = std::less<T>>
inline void simulateInsertValueAscendingPilesSplit(
    std::vector<T>& tails, std::size_t& lastPileIndex, const T& value,
    std::size_t originalIndex, std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& ascCounts, Less less = Less{}
) {
    const std::size_t pileIndex = findAscendingPileWithTailsSplit(tails, lastPileIndex, value, less);
    if (pileIndex < tails.size()) { tails[pileIndex] = value; ++ascCounts[pileIndex]; }
    else { assert(pileIndex <= PILE_MASK); tails.push_back(value); ascCounts.push_back(1); }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = makeAscTag(static_cast<uint32_t>(pileIndex));
}

#if defined(__GNUC__) || defined(__clang__)
#define JESSESORT_CONTINUE_NOINLINE __attribute__((noinline))
#else
#define JESSESORT_CONTINUE_NOINLINE
#endif

template <bool UseSplit, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionValleyRescue(
    const std::vector<T>& arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less,
    bool enableWindowAvx = false
) {
    auto process = [&](const T& previous, const T& value, std::size_t i) {
        bool desc = less(value, previous);
        bool asc = !desc && less(previous, value);
        if (desc && i + 1 < arr.size() && less(value, arr[i + 1])) {
            const bool wouldCreateDesc = result.descTails.empty() || less(result.descTails.back(), value);
            const bool canContinueAsc = !result.ascTails.empty() && !less(value, result.ascTails.back());
            if (wouldCreateDesc && canContinueAsc) { desc = false; asc = true; }
        }
        if (asc) descendingMode = false; else if (desc) descendingMode = true;
        if (descendingMode) {
            if constexpr (UseSplit) simulateInsertValueDescendingPilesSplit(
                result.descTails, lastPileIndexDescending, value, i, result.blueprint, result.descCounts, less);
            else simulateInsertValueDescendingPiles(
                result.descTails, lastPileIndexDescending, value, i, result.blueprint, result.descCounts, less, enableWindowAvx);
        } else {
            if constexpr (UseSplit) simulateInsertValueAscendingPilesSplit(
                result.ascTails, lastPileIndexAscending, value, i, result.blueprint, result.ascCounts, less);
            else simulateInsertValueAscendingPiles(
                result.ascTails, lastPileIndexAscending, value, i, result.blueprint, result.ascCounts, less, enableWindowAvx);
        }
    };
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[start - 1];
        for (std::size_t i = start; i < arr.size(); ++i) { const T value = arr[i]; process(previous, value, i); previous = value; }
    } else {
        for (std::size_t i = start; i < arr.size(); ++i) process(arr[i - 1], arr[i], i);
    }
}

template <bool UseSplit, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertion(
    const std::vector<T>& arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less,
    bool enableWindowAvx = false
) {
    auto process = [&](const T& previous, const T& value, std::size_t i) {
        if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
        if (descendingMode) {
            if constexpr (UseSplit) {
                simulateInsertValueDescendingPilesSplit(
                    result.descTails, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, less);
            } else {
                simulateInsertValueDescendingPiles(
                    result.descTails, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, less, enableWindowAvx);
            }
        } else {
            if constexpr (UseSplit) {
                simulateInsertValueAscendingPilesSplit(
                    result.ascTails, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, less);
            } else {
                simulateInsertValueAscendingPiles(
                    result.ascTails, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, less, enableWindowAvx);
            }
        }
    };

    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[start - 1];
        for (std::size_t i = start; i < arr.size(); ++i) {
            const T value = arr[i];
            process(previous, value, i);
            previous = value;
        }
    } else {
        for (std::size_t i = start; i < arr.size(); ++i) process(arr[i - 1], arr[i], i);
    }
}

template <typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionNoHint(
    const std::vector<T>& arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less
) {
    auto process = [&](const T& previous, const T& value, std::size_t i) {
        if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;

        if (descendingMode) {
            const std::size_t pileIndex =
                findDescendingPileWithTailsNoHint(result.descTails, value, less);
            if (pileIndex < result.descTails.size()) {
                result.descTails[pileIndex] = value;
                ++result.descCounts[pileIndex];
            } else {
                assert(pileIndex <= PILE_MASK);
                result.descTails.push_back(value);
                result.descCounts.push_back(1);
            }
            lastPileIndexDescending = pileIndex;
            result.blueprint[i] = makeDescTag(static_cast<uint32_t>(pileIndex));
        } else {
            const std::size_t pileIndex =
                findAscendingPileWithTailsNoHint(result.ascTails, value, less);
            if (pileIndex < result.ascTails.size()) {
                result.ascTails[pileIndex] = value;
                ++result.ascCounts[pileIndex];
            } else {
                assert(pileIndex <= PILE_MASK);
                result.ascTails.push_back(value);
                result.ascCounts.push_back(1);
            }
            lastPileIndexAscending = pileIndex;
            result.blueprint[i] = makeAscTag(static_cast<uint32_t>(pileIndex));
        }
    };

    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[start - 1];
        for (std::size_t i = start; i < arr.size(); ++i) {
            const T value = arr[i];
            process(previous, value, i);
            previous = value;
        }
    } else {
        for (std::size_t i = start; i < arr.size(); ++i)
            process(arr[i - 1], arr[i], i);
    }
}


template <typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionCoherentCache(
    const std::vector<T>& arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less
) {
    static_assert(jessesort::simulated_legacy::specializedIntegralEligible<T, Less>);
    struct Entry { std::uint64_t key = 0; std::uint32_t pile = 0; bool valid = false; };
    std::array<Entry, 128> ascCache{}, descCache{};
    auto keyOf = [](const T& value) -> std::uint64_t {
        return static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(value));
    };
    auto bucketKey = [](std::uint64_t key) -> std::size_t {
        return static_cast<std::size_t>((key * 11400714819323198485ULL) >> 57);
    };
    auto seed = [&](const std::vector<T>& tails, auto& cache) {
        for (std::size_t p = 0; p < tails.size(); ++p) {
            const std::uint64_t key = keyOf(tails[p]);
            cache[bucketKey(key)] = Entry{key, static_cast<std::uint32_t>(p), true};
        }
    };
    seed(result.ascTails, ascCache);
    seed(result.descTails, descCache);

    auto process = [&](const T& previous, const T& value, std::size_t i) {
        if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
        auto& tails = descendingMode ? result.descTails : result.ascTails;
        auto& counts = descendingMode ? result.descCounts : result.ascCounts;
        auto& cache = descendingMode ? descCache : ascCache;
        auto& lastPile = descendingMode ? lastPileIndexDescending : lastPileIndexAscending;
        const std::uint64_t key = keyOf(value);
        const std::size_t b = bucketKey(key);
        auto& e = cache[b];
        std::size_t p;
        if (e.valid && e.key == key && e.pile < tails.size()) {
            p = e.pile;
            ++counts[p];
        } else {
            p = descendingMode
                ? findDescendingPileWithTailsNoHint(tails, value, less)
                : findAscendingPileWithTailsNoHint(tails, value, less);
            if (p < tails.size()) {
                const std::uint64_t oldKey = keyOf(tails[p]);
                auto& old = cache[bucketKey(oldKey)];
                if (old.valid && old.key == oldKey && old.pile == p) old.valid = false;
                tails[p] = value;
                ++counts[p];
            } else {
                assert(p <= PILE_MASK);
                tails.push_back(value);
                counts.push_back(1);
            }
            cache[b] = Entry{key, static_cast<std::uint32_t>(p), true};
        }
        lastPile = p;
        result.blueprint[i] = descendingMode
            ? makeDescTag(static_cast<std::uint32_t>(p))
            : makeAscTag(static_cast<std::uint32_t>(p));
    };

    T previous = arr[start - 1];
    for (std::size_t i = start; i < arr.size(); ++i) {
        const T value = arr[i];
        process(previous, value, i);
        previous = value;
    }
}

#undef JESSESORT_CONTINUE_NOINLINE

template <typename T, typename Less = std::less<T>>
SimulatedInsertionResult<T> simulatePatienceInsertionBlueprint(
    const std::vector<T>& arr,
    Less less = Less{},
    bool enableEarlyRandomInsertionRoute = false,
    std::size_t probePileCap = 0,
    std::size_t probeValues = 64,
    bool enableNaturalRunRoute = false,
    bool enableWindowAvx = false,
    bool enableCoherentValuePileCache = false
) {
    constexpr std::size_t MinPrefixPileLength = 32;
    const std::size_t n = arr.size();
    SimulatedInsertionResult<T> result;
    if (n == 0) return result;

    enum class PrefixDirection { Unknown, Ascending, Descending };
    PrefixDirection prefixDirection = PrefixDirection::Unknown;
    std::size_t prefixEnd = 1;
    while (prefixEnd < n) {
        const T& previous = arr[prefixEnd - 1];
        const T& value = arr[prefixEnd];
        if (less(previous, value)) {
            prefixDirection = PrefixDirection::Ascending;
            ++prefixEnd;
            break;
        }
        if (less(value, previous)) {
            prefixDirection = PrefixDirection::Descending;
            ++prefixEnd;
            break;
        }
        ++prefixEnd;
    }
    if (prefixDirection == PrefixDirection::Ascending) {
        for (; prefixEnd < n; ++prefixEnd)
            if (less(arr[prefixEnd], arr[prefixEnd - 1])) break;
    } else if (prefixDirection == PrefixDirection::Descending) {
        for (; prefixEnd < n; ++prefixEnd)
            if (less(arr[prefixEnd - 1], arr[prefixEnd])) break;
    }

    if (prefixEnd == n) {
        result.alreadySortedAscending =
            prefixDirection != PrefixDirection::Descending;
        result.reverseSortedDescending =
            prefixDirection == PrefixDirection::Descending;
        return result;
    }

    result.alreadySortedAscending = false;
    result.reverseSortedDescending = false;
    result.blueprint.resize(n);
    const std::size_t reservePiles = estimatePileReserve(n);
    result.ascCounts.reserve(reservePiles);
    result.descCounts.reserve(reservePiles);
    result.ascTails.reserve(reservePiles);
    result.descTails.reserve(reservePiles);

    std::size_t lastPileIndexAscending = 0;
    std::size_t lastPileIndexDescending = 0;
    bool descendingMode = false;
    std::size_t processStart = 1;

    // E181: cold confirmation for short same-direction prefix handoff.
    const bool confirmedShortSameDirectionPrefix =
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd < MinPrefixPileLength &&
        jessesort::simulated_legacy::confirmedShortSameDirectionPrefix(
            arr, prefixEnd, prefixDirection == PrefixDirection::Descending, less,
            MinPrefixPileLength);
    const bool materializePrefix =
        prefixDirection != PrefixDirection::Unknown &&
        (prefixEnd >= MinPrefixPileLength || confirmedShortSameDirectionPrefix);

    if (materializePrefix) {
        processStart = prefixEnd;
        if (prefixDirection == PrefixDirection::Ascending) {
            result.ascTails.push_back(arr[prefixEnd - 1]);
            result.ascCounts.push_back(prefixEnd);
            std::fill_n(result.blueprint.begin(), prefixEnd, makeAscTag(0));
            descendingMode = false;
        } else {
            result.descTails.push_back(arr[prefixEnd - 1]);
            result.descCounts.push_back(prefixEnd);
            std::fill_n(result.blueprint.begin(), prefixEnd, makeDescTag(0));
            descendingMode = true;
        }
    } else {
        if (prefixDirection == PrefixDirection::Descending) {
            result.descTails.push_back(arr[0]);
            result.descCounts.push_back(1);
            result.blueprint[0] = makeDescTag(0);
            descendingMode = true;
        } else {
            result.ascTails.push_back(arr[0]);
            result.ascCounts.push_back(1);
            result.blueprint[0] = makeAscTag(0);
            descendingMode = false;
        }
    }

    bool postPrefixDirectionOverride = false;
    bool postPrefixOverrideDescending = false;
    if (processStart == prefixEnd && materializePrefix &&
        processStart + 1 < n) {
        if (less(arr[processStart], arr[processStart + 1])) {
            postPrefixDirectionOverride = true;
            postPrefixOverrideDescending = false;
        } else if (less(arr[processStart + 1], arr[processStart])) {
            postPrefixDirectionOverride = true;
            postPrefixOverrideDescending = true;
        }
    }

    auto processValueSample = [&](const T& previous, const T& value, std::size_t i) {
        if (postPrefixDirectionOverride) {
            descendingMode = postPrefixOverrideDescending;
            postPrefixDirectionOverride = false;
        } else if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;

        const bool capProbe = probePileCap != 0 && i < probeValues;
        bool hit = false;

        if (descendingMode) {
            const bool hadPiles = !result.descTails.empty();
            const std::size_t oldHint = lastPileIndexDescending;
            const std::size_t pileIndex = findDescendingPileWithTails(
                result.descTails, lastPileIndexDescending, value, less,
                probePileCap == 16, enableWindowAvx);
            if (capProbe && pileIndex == result.descTails.size() &&
                result.descTails.size() >= probePileCap) {
                result.blueprint[i] = OVERFLOW_TAG;
                result.probeOverflowSorted.push_back(value);
                // Leave the capped tail set unchanged until after routing.
                lastPileIndexDescending = oldHint;
            } else {
                if (pileIndex < result.descTails.size()) {
                    result.descTails[pileIndex] = value;
                    ++result.descCounts[pileIndex];
                } else {
                    assert(pileIndex <= PILE_MASK);
                    result.descTails.push_back(value);
                    result.descCounts.push_back(1);
                }
                lastPileIndexDescending = pileIndex;
                result.blueprint[i] = makeDescTag(static_cast<uint32_t>(pileIndex));
                hit = hadPiles && oldHint == pileIndex;
            }
        } else {
            const bool hadPiles = !result.ascTails.empty();
            const std::size_t oldHint = lastPileIndexAscending;
            const std::size_t pileIndex = findAscendingPileWithTails(
                result.ascTails, lastPileIndexAscending, value, less,
                probePileCap == 16, enableWindowAvx);
            if (capProbe && pileIndex == result.ascTails.size() &&
                result.ascTails.size() >= probePileCap) {
                result.blueprint[i] = OVERFLOW_TAG;
                result.probeOverflowSorted.push_back(value);
                lastPileIndexAscending = oldHint;
            } else {
                if (pileIndex < result.ascTails.size()) {
                    result.ascTails[pileIndex] = value;
                    ++result.ascCounts[pileIndex];
                } else {
                    assert(pileIndex <= PILE_MASK);
                    result.ascTails.push_back(value);
                    result.ascCounts.push_back(1);
                }
                lastPileIndexAscending = pileIndex;
                result.blueprint[i] = makeAscTag(static_cast<uint32_t>(pileIndex));
                hit = hadPiles && oldHint == pileIndex;
            }
        }
        return hit;
    };

    auto classifyProbeByRunLengths = [&]() {
        std::size_t total = result.probeOverflowSorted.size();
        std::size_t largest = result.probeOverflowSorted.size();
        unsigned long long sumSquares =
            static_cast<unsigned long long>(result.probeOverflowSorted.size()) *
            result.probeOverflowSorted.size();
        auto consume = [&](const std::vector<std::size_t>& counts) {
            for (const std::size_t len : counts) {
                total += len;
                largest = std::max(largest, len);
                sumSquares += static_cast<unsigned long long>(len) * len;
            }
        };
        consume(result.ascCounts);
        consume(result.descCounts);
        if (total < 32) return false;
        // Random-like probes spread occupancy across many short runs.  This
        // concentration test remains meaningful when the number of ordinary
        // piles is artificially capped; the overflow run participates exactly
        // like every other probe run.
        const unsigned long long totalSq =
            static_cast<unsigned long long>(total) * total;
        return largest * 4 <= total && sumSquares * 7 <= totalSq;
    };

    auto marryProbeOverflowAfterRouting = [&]() {
        if (result.probeOverflowSorted.empty()) return;
        std::sort(result.probeOverflowSorted.begin(), result.probeOverflowSorted.end(), less);
        if (result.ascTails.empty()) {
            // The normal first element creates an ascending pile. This fallback
            // only protects unusual long-prefix paths where the cap experiment
            // cannot satisfy the "merge into an existing pile" requirement.
            result.probeOverflowSorted.clear();
            result.probeOverflowTargetAscPile = static_cast<std::size_t>(-1);
            return;
        }
        const T& finalValue = result.probeOverflowSorted.back();
        const std::size_t target = findAscendingPileWithTailsNoHint(
            result.ascTails, finalValue, less);
        const std::size_t existingTarget =
            std::min(target, result.ascTails.size() - 1);
        result.probeOverflowTargetAscPile = existingTarget;
        // If finalValue would have created pile cap+1, marry it into the
        // boundary pile but keep that pile's larger existing tail. Otherwise
        // the merged run's tail is finalValue. In both cases this is simply
        // max(existing tail, overflow max) under the comparator.
        if (less(result.ascTails[existingTarget], finalValue))
            result.ascTails[existingTarget] = finalValue;
        result.ascCounts[existingTarget] += result.probeOverflowSorted.size();
        lastPileIndexAscending = existingTarget;
    };

    // E099: propagate E098's narrow natural-run batching into avx2. This is
    // entered only for long monotone-prefix inputs and before the capped-probe
    // machinery, so the Random-oriented probe behavior is unchanged.
    const bool naturalRunCandidate =
        enableNaturalRunRoute && (probePileCap == 0 || processStart >= probeValues) &&
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd >= MinPrefixPileLength && prefixEnd * 8 >= n;
    if (naturalRunCandidate && processStart < n) {
        std::size_t i = processStart;
        auto insertOne = [&](std::size_t j) {
            const T& previous = arr[j - 1];
            const T& value = arr[j];
            if (postPrefixDirectionOverride) {
                descendingMode = postPrefixOverrideDescending;
                postPrefixDirectionOverride = false;
            } else if (less(previous, value)) descendingMode = false;
            else if (less(value, previous)) descendingMode = true;
            if (descendingMode) {
                const std::size_t pileIndex = findDescendingPileWithTailsNoHint(result.descTails, value, less);
                if (pileIndex < result.descTails.size()) { result.descTails[pileIndex] = value; ++result.descCounts[pileIndex]; }
                else { result.descTails.push_back(value); result.descCounts.push_back(1); }
                lastPileIndexDescending = pileIndex;
                result.blueprint[j] = makeDescTag(static_cast<uint32_t>(pileIndex));
            } else {
                const std::size_t pileIndex = findAscendingPileWithTailsNoHint(result.ascTails, value, less);
                if (pileIndex < result.ascTails.size()) { result.ascTails[pileIndex] = value; ++result.ascCounts[pileIndex]; }
                else { result.ascTails.push_back(value); result.ascCounts.push_back(1); }
                lastPileIndexAscending = pileIndex;
                result.blueprint[j] = makeAscTag(static_cast<uint32_t>(pileIndex));
            }
        };
        while (i < n) {
            bool asc;
            bool desc;
            if (postPrefixDirectionOverride) {
                asc = !postPrefixOverrideDescending;
                desc = postPrefixOverrideDescending;
            } else {
                asc = less(arr[i-1], arr[i]);
                desc = !asc && less(arr[i], arr[i-1]);
            }
            if (!asc && !desc) { insertOne(i++); continue; }
            std::size_t end = i + 1;
            if (asc) while (end < n && less(arr[end-1], arr[end])) ++end;
            else while (end < n && less(arr[end], arr[end-1])) ++end;
            const std::size_t len = end - i;
            bool batched = false;
            if (len >= 8) {
                if (asc) {
                    const auto firstPile = findAscendingPileWithTailsNoHint(result.ascTails, arr[i], less);
                    const auto lastPile = findAscendingPileWithTailsNoHint(result.ascTails, arr[end-1], less);
                    if (firstPile == lastPile) {
                        if (firstPile < result.ascTails.size()) { result.ascTails[firstPile] = arr[end-1]; result.ascCounts[firstPile] += len; }
                        else { result.ascTails.push_back(arr[end-1]); result.ascCounts.push_back(len); }
                        std::fill(result.blueprint.begin()+static_cast<std::ptrdiff_t>(i), result.blueprint.begin()+static_cast<std::ptrdiff_t>(end), makeAscTag(static_cast<uint32_t>(firstPile)));
                        lastPileIndexAscending = firstPile; descendingMode = false; postPrefixDirectionOverride = false; batched = true;
                    }
                } else {
                    const auto firstPile = findDescendingPileWithTailsNoHint(result.descTails, arr[i], less);
                    const auto lastPile = findDescendingPileWithTailsNoHint(result.descTails, arr[end-1], less);
                    if (firstPile == lastPile) {
                        if (firstPile < result.descTails.size()) { result.descTails[firstPile] = arr[end-1]; result.descCounts[firstPile] += len; }
                        else { result.descTails.push_back(arr[end-1]); result.descCounts.push_back(len); }
                        std::fill(result.blueprint.begin()+static_cast<std::ptrdiff_t>(i), result.blueprint.begin()+static_cast<std::ptrdiff_t>(end), makeDescTag(static_cast<uint32_t>(firstPile)));
                        lastPileIndexDescending = firstPile; descendingMode = true; postPrefixDirectionOverride = false; batched = true;
                    }
                }
            }
            if (!batched) for (std::size_t j=i; j<end; ++j) insertOne(j);
            i=end;
        }
        return result;
    }

    // Sample once, then dispatch to a specialized continuation loop. A >=75%
    // exact-hint rate favors the split hinted path. Sustained high-pile Random-like
    // structure instead uses the restored no-hint continuation (E065); other inputs
    // retain the ordinary inline search. The decision is made outside the hot loop.
    const std::size_t SampleValues = probePileCap != 0 ? probeValues : 64;
    const std::size_t StructureProbeValues = probePileCap != 0 ? probeValues : 64;
    const std::size_t sampleEnd = processStart < SampleValues
        ? std::min(n, SampleValues)
        : std::min(n, processStart + SampleValues);
    std::size_t sampleHits = 0;
    std::size_t sampleCount = 0;

    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[processStart - 1];
        std::size_t i = processStart;
        for (; i < sampleEnd; ++i) {
            const T value = arr[i];
            sampleHits += processValueSample(previous, value, i) ? 1u : 0u;
            ++sampleCount;
            if (i + 1 == StructureProbeValues) {
                if (probePileCap != 0)
                    result.earlyRandomLike = classifyProbeByRunLengths();
                else
                    result.earlyRandomLike =
                        result.ascCounts.size() >= 6 && result.descCounts.size() >= 6;
            }
            previous = value;
        }
        if (probePileCap != 0) marryProbeOverflowAfterRouting();
        const bool earlyRandomInsertion =
            enableEarlyRandomInsertionRoute &&
            n >= 10000 &&
            result.earlyRandomLike &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 2 * sizeof(void*);
        const bool useSplit = sampleCount != 0 && sampleHits * 4 >= sampleCount * 3;
        // E105: one-register hint-centered AVX2 continuation is only worthwhile
        // for small, low-pile layouts. Larger inputs lose the benefit as the
        // rare-miss window becomes too small a fraction of insertion work.
        const bool windowRoute = enableWindowAvx && n >= 5000 && n <= 20000 &&
            result.ascTails.size() + result.descTails.size() <= 12;
        bool coherentCacheCandidate = false;
        if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
            coherentCacheCandidate = enableCoherentValuePileCache &&
                result.earlyRandomLike &&
                jessesort::simulated_legacy::coherentValuePileCacheSampleCandidate(arr, less, n >= 50000);
        }
        const std::size_t e183ProbePiles = result.ascCounts.size() + result.descCounts.size();
        const bool e183ValleyRescue = n >= 10000 && e183ProbePiles >= 3 && e183ProbePiles <= 12;
        if (i < n) {
            if (e183ValleyRescue) {
                if (useSplit) continuePatienceInsertionValleyRescue<true>(
                    arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, false);
                else continuePatienceInsertionValleyRescue<false>(
                    arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, windowRoute);
            } else if (coherentCacheCandidate) {
                if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>)
                    continuePatienceInsertionCoherentCache(
                        arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            } else if (earlyRandomInsertion) continuePatienceInsertionNoHint(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useSplit) continuePatienceInsertion<true>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, false);
            else continuePatienceInsertion<false>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, windowRoute);
        }
    } else {
        std::size_t i = processStart;
        for (; i < sampleEnd; ++i) {
            sampleHits += processValueSample(arr[i - 1], arr[i], i) ? 1u : 0u;
            ++sampleCount;
            if (i + 1 == StructureProbeValues) {
                if (probePileCap != 0)
                    result.earlyRandomLike = classifyProbeByRunLengths();
                else
                    result.earlyRandomLike =
                        result.ascCounts.size() >= 6 && result.descCounts.size() >= 6;
            }
        }
        if (probePileCap != 0) marryProbeOverflowAfterRouting();
        const bool earlyRandomInsertion =
            enableEarlyRandomInsertionRoute &&
            n >= 10000 &&
            result.earlyRandomLike &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 2 * sizeof(void*);
        const bool useSplit = sampleCount != 0 && sampleHits * 4 >= sampleCount * 3;
        // E105: one-register hint-centered AVX2 continuation is only worthwhile
        // for small, low-pile layouts. Larger inputs lose the benefit as the
        // rare-miss window becomes too small a fraction of insertion work.
        const bool windowRoute = enableWindowAvx && n >= 5000 && n <= 20000 &&
            result.ascTails.size() + result.descTails.size() <= 12;
        bool coherentCacheCandidate = false;
        if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
            coherentCacheCandidate = enableCoherentValuePileCache &&
                result.earlyRandomLike &&
                jessesort::simulated_legacy::coherentValuePileCacheSampleCandidate(arr, less, n >= 50000);
        }
        const std::size_t e183ProbePiles = result.ascCounts.size() + result.descCounts.size();
        const bool e183ValleyRescue = n >= 10000 && e183ProbePiles >= 3 && e183ProbePiles <= 12;
        if (i < n) {
            if (e183ValleyRescue) {
                if (useSplit) continuePatienceInsertionValleyRescue<true>(
                    arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, false);
                else continuePatienceInsertionValleyRescue<false>(
                    arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, windowRoute);
            } else if (coherentCacheCandidate) {
                if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>)
                    continuePatienceInsertionCoherentCache(
                        arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            } else if (earlyRandomInsertion) continuePatienceInsertionNoHint(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useSplit) continuePatienceInsertion<true>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, false);
            else continuePatienceInsertion<false>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less, windowRoute);
        }
    }
    return result;
}

// =========================================================
// Reconstruction to temp
// =========================================================
//
// Input:
//   arr       = original values, still unmoved
//   blueprint = packed gameID + local pileID per original index
//   ascCounts / descCounts = counts produced during simulation
//
// Output:
//   tmp      = physically materialized flat runs
//   return   = run boundaries into tmp
//
// Global run order in tmp is always:
//   [descending-game piles][ascending-game piles]
// Direct A/B testing found this broadly neutral-to-faster across the routine
// workload suite, with rotated three-run inputs as the notable regression.
//
// Within either grouping:
//   ascending-game runs preserve encounter order.
//   descending-game runs reverse encounter order so they become ascending.

template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructTaggedBlueprintToTemp_WithSplitCounts(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::vector<T>& tmp,
    bool reverseAscRuns = false,
    bool reverseDescRuns = true,
    const std::vector<T>* probeOverflowSorted = nullptr,
    std::size_t probeOverflowTargetAscPile = static_cast<std::size_t>(-1),
    Less less = Less{}
) {
    const std::size_t n = arr.size();
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numRuns = numAscPiles + numDescPiles;

    std::vector<std::size_t> start(numRuns + 1, 0);
    std::size_t out = 0;
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        start[p] = out;
        out += descCounts[p];
    }
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t r = numDescPiles + p;
        start[r] = out;
        out += ascCounts[p];
    }
    start[numRuns] = out;
    assert(out == n);
    if constexpr (std::is_default_constructible_v<T>) {
        tmp.resize(n);
    } else {
        // Copy-construct valid slots for non-default-constructible values.
        // Reconstruction overwrites each slot exactly once afterward.
        tmp = arr;
    }

    // Reuse count storage as direction-specific cursors.
    for (std::size_t p = 0; p < numDescPiles; ++p)
        descCounts[p] = reverseDescRuns ? start[p + 1] : start[p];
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t r = numDescPiles + p;
        ascCounts[p] = reverseAscRuns ? start[r + 1] : start[r];
    }

    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == OVERFLOW_TAG) continue;
        const bool desc = isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(localPileId(tag));
        if (desc) {
            if (reverseDescRuns) tmp[--descCounts[local]] = arr[i];
            else tmp[descCounts[local]++] = arr[i];
        } else {
            if (reverseAscRuns) tmp[--ascCounts[local]] = arr[i];
            else tmp[ascCounts[local]++] = arr[i];
        }
    }
    if (probeOverflowSorted && !probeOverflowSorted->empty() &&
        probeOverflowTargetAscPile < numAscPiles && !reverseAscRuns) {
        const std::size_t r = numDescPiles + probeOverflowTargetAscPile;
        const std::size_t begin = start[r];
        const std::size_t end = start[r + 1];
        const std::size_t overflowCount = probeOverflowSorted->size();
        assert(end - begin >= overflowCount);
        const std::size_t normalEnd = end - overflowCount;
        std::vector<T> merged;
        merged.reserve(end - begin);
        std::merge(tmp.begin() + static_cast<std::ptrdiff_t>(begin),
                   tmp.begin() + static_cast<std::ptrdiff_t>(normalEnd),
                   probeOverflowSorted->begin(), probeOverflowSorted->end(),
                   std::back_inserter(merged), less);
        std::move(merged.begin(), merged.end(),
                  tmp.begin() + static_cast<std::ptrdiff_t>(begin));
    }
    return start;
}

// E083 test: avx2 counterpart of E081. Normalize non-overflow blueprint tags to
// global run IDs before the scattered reconstruction pass. OVERFLOW_TAG entries
// remain sentinel values and are merged into their target pile afterward.
template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructTaggedBlueprintNormalizedForAvx2(
    const std::vector<T>& arr, std::vector<uint32_t> blueprint,
    std::vector<std::size_t> ascCounts, std::vector<std::size_t> descCounts,
    std::vector<T>& tmp, bool reverseAscRuns, bool reverseDescRuns,
    const std::vector<T>* probeOverflowSorted, std::size_t probeOverflowTargetAscPile,
    Less less = Less{},
    bool enableRawBulkSpanDecode = false
) {
    const std::size_t n=arr.size(), na=ascCounts.size(), nd=descCounts.size(), nr=na+nd;
    std::vector<std::size_t> start(nr+1,0); std::size_t out=0;
    for(std::size_t p=0;p<nd;++p){start[p]=out;out+=descCounts[p];}
    for(std::size_t p=0;p<na;++p){const auto r=nd+p;start[r]=out;out+=ascCounts[p];}
    start[nr]=out; assert(out==n);
    if constexpr(std::is_default_constructible_v<T>) tmp.resize(n); else tmp=arr;
    std::vector<std::size_t> cursor(nr);
    for(std::size_t p=0;p<nd;++p) cursor[p]=reverseDescRuns?start[p+1]-1:start[p];
    for(std::size_t p=0;p<na;++p){const auto r=nd+p;cursor[r]=reverseAscRuns?start[r+1]-1:start[r];}
    // E162 candidate gate: only repeated NON-overflow raw tags count as evidence.
    // Repeated OVERFLOW_TAG values from the capped probe are not pile locality.
    std::size_t comparableAdj = 0, sameNonOverflowAdj = 0;
    const std::size_t tagProbeEnd = std::min<std::size_t>(n, 64);
    for (std::size_t i = 1; i < tagProbeEnd; ++i) {
        const auto a = blueprint[i - 1], b = blueprint[i];
        if (a == OVERFLOW_TAG || b == OVERFLOW_TAG) continue;
        ++comparableAdj;
        sameNonOverflowAdj += a == b ? 1u : 0u;
    }
    // E189: restore E162's retained raw bulk-span path, which had later fallen
    // out of the public avx2 call. The original 75% gate regresses the newer
    // canonical Noise10 row, so require 95% same non-overflow adjacency.
    const bool rawBulk =
        enableRawBulkSpanDecode && comparableAdj >= 12 &&
        sameNonOverflowAdj * 100 >= comparableAdj * 95;

    if (!rawBulk) {
        for(std::size_t i=0;i<n;++i){const auto tag=blueprint[i]; if(tag==OVERFLOW_TAG) continue; const auto local=(std::size_t)localPileId(tag); blueprint[i]=(uint32_t)(isDescTag(tag)?local:nd+local);}
        // E096: avx2's production normalized layout always reverses descending runs
        // and keeps ascending runs forward. Global run IDs therefore encode the
        // direction directly, avoiding a separate random-access step[] stream.
        if (reverseDescRuns && !reverseAscRuns) {
            for(std::size_t i=0;i<n;++i){const auto tag=blueprint[i]; if(tag==OVERFLOW_TAG) continue; const auto r=(std::size_t)tag; const auto pos=cursor[r]; cursor[r] += (r < nd) ? static_cast<std::size_t>(-1) : 1u; tmp[pos]=arr[i];}
        } else {
            for(std::size_t i=0;i<n;++i){const auto tag=blueprint[i]; if(tag==OVERFLOW_TAG) continue; const auto r=(std::size_t)tag; const auto pos=cursor[r]; const bool backwards=(r<nd)?reverseDescRuns:reverseAscRuns; cursor[r] += backwards ? static_cast<std::size_t>(-1) : 1u; tmp[pos]=arr[i];}
        }
    } else {
        std::size_t i = 0;
        while (i < n) {
            const uint32_t rawTag = blueprint[i];
            std::size_t j = i + 1;
            while (j < n && blueprint[j] == rawTag) ++j;
            if (rawTag != OVERFLOW_TAG) {
                const std::size_t local = static_cast<std::size_t>(localPileId(rawTag));
                const std::size_t r = isDescTag(rawTag) ? local : nd + local;
                const std::size_t len = j - i;
                const bool backwards = (r < nd) ? reverseDescRuns : reverseAscRuns;
                if (backwards) {
                    const std::size_t pos = cursor[r];
                    const std::size_t first = pos + 1 - len;
                    std::reverse_copy(
                        arr.begin() + static_cast<std::ptrdiff_t>(i),
                        arr.begin() + static_cast<std::ptrdiff_t>(j),
                        tmp.begin() + static_cast<std::ptrdiff_t>(first));
                    cursor[r] -= len;
                } else {
                    const std::size_t pos = cursor[r];
                    std::copy(
                        arr.begin() + static_cast<std::ptrdiff_t>(i),
                        arr.begin() + static_cast<std::ptrdiff_t>(j),
                        tmp.begin() + static_cast<std::ptrdiff_t>(pos));
                    cursor[r] += len;
                }
            }
            i = j;
        }
    }
    if(probeOverflowSorted && !probeOverflowSorted->empty() && probeOverflowTargetAscPile<na && !reverseAscRuns){
        const auto r=nd+probeOverflowTargetAscPile, begin=start[r], end=start[r+1], oc=probeOverflowSorted->size(), normalEnd=end-oc;
        std::vector<T> merged; merged.reserve(end-begin);
        std::merge(tmp.begin()+(std::ptrdiff_t)begin,tmp.begin()+(std::ptrdiff_t)normalEnd,probeOverflowSorted->begin(),probeOverflowSorted->end(),std::back_inserter(merged),less);
        std::move(merged.begin(),merged.end(),tmp.begin()+(std::ptrdiff_t)begin);
    }
    return start;
}

// =========================================================
// Validation helpers
// =========================================================

template <typename T, typename Less = std::less<T>>
bool validateRunsAscending(
    const std::vector<T>& data,
    const std::vector<std::size_t>& runStart,
    Less less = Less{}
) {
    if (runStart.empty()) return data.empty();

    const std::size_t numRuns = runStart.size() - 1;

    for (std::size_t r = 0; r < numRuns; ++r) {
        const std::size_t begin = runStart[r];
        const std::size_t end = runStart[r + 1];

        for (std::size_t i = begin + 1; i < end; ++i) {
            if (less(data[i], data[i - 1])) {
                return false;
            }
        }
    }

    return true;
}

// Optional final sorted check for testing after your merge is added.
template <typename T, typename Less = std::less<T>>
bool validateSorted(
    const std::vector<T>& data,
    Less less = Less{}
) {
    for (std::size_t i = 1; i < data.size(); ++i) {
        if (less(data[i], data[i - 1])) {
            return false;
        }
    }
    return true;
}

// =========================================================
// Merge logic
// =========================================================
//
// Contract:
//
//   Before merge:
//     tmp contains flat ascending runs.
//     arr still contains the original input.
//     runStart gives run boundaries inside tmp.
//
//   After merge:
//     arr contains the fully sorted result.
//
// Implementation:
//   Adjacent bottom-up pairwise merge.
//   Ping-pongs between tmp and arr.
//   First pass reads tmp and writes arr.
//   If the final sorted data ends in tmp, arr.swap(tmp) makes arr the result.
//
// Notes:
//   - This merge is stable in the ordinary element-by-element path.
//   - The reverse-disjoint fast path is unstable for equal elements across runs,
//     but still sorted. Remove that fast path if stability ever matters.

template <typename T, typename Less = std::less<T>>
void mergeTwoAdjacentRunsToDest(
    std::vector<T>& src,
    std::vector<T>& dst,
    std::size_t begin,
    std::size_t mid,
    std::size_t end,
    Less less = Less{},
    unsigned gallopTrigger = 7
) {

    if (begin == end) {
        return;
    }

    if (begin == mid) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(mid),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    if (mid == end) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    // Fast path 1:
    // left.max <= right.min, so the two runs are already globally ordered.
    //
    // src[mid - 1] <= src[mid]
    if (!less(src[mid], src[mid - 1])) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));
        return;
    }

    // Fast path 2:
    // right.max <= left.min, so the merged output is simply right then left.
    //
    // src[end - 1] <= src[begin]
    //
    // This is sorted but unstable across equal keys.
    if (!less(src[begin], src[end - 1])) {
        const std::size_t rightLen = end - mid;

        std::move(src.begin() + static_cast<std::ptrdiff_t>(mid),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin));

        std::move(src.begin() + static_cast<std::ptrdiff_t>(begin),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(begin + rightLen));

        return;
    }

    std::size_t i = begin;
    std::size_t j = mid;
    std::size_t out = begin;
    unsigned leftWins = 0, rightWins = 0;
    const unsigned GALLOP_TRIGGER = gallopTrigger;

    while (i < mid && j < end) {
        if (less(src[j], src[i])) {
            dst[out++] = std::move(src[j++]);
            ++rightWins; leftWins = 0;
            if (rightWins >= GALLOP_TRIGGER && i < mid && j < end) {
                std::size_t step = 1;
                while (j + step < end && less(src[j + step], src[i]))
                    step <<= 1;
                std::size_t lo = j;
                std::size_t hi = std::min(end, j + step + 1);
                while (lo < hi) {
                    const std::size_t m = lo + (hi - lo) / 2;
                    if (less(src[m], src[i])) lo = m + 1;
                    else hi = m;
                }
                while (j < lo) dst[out++] = std::move(src[j++]);
                rightWins = 0;
            }
        } else {
            dst[out++] = std::move(src[i++]);
            ++leftWins; rightWins = 0;
            if (leftWins >= GALLOP_TRIGGER && i < mid && j < end) {
                std::size_t step = 1;
                while (i + step < mid && !less(src[j], src[i + step]))
                    step <<= 1;
                std::size_t lo = i;
                std::size_t hi = std::min(mid, i + step + 1);
                while (lo < hi) {
                    const std::size_t m = lo + (hi - lo) / 2;
                    if (!less(src[j], src[m])) lo = m + 1;
                    else hi = m;
                }
                while (i < lo) dst[out++] = std::move(src[i++]);
                leftWins = 0;
            }
        }
    }

    if (i < mid) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(i),
                  src.begin() + static_cast<std::ptrdiff_t>(mid),
                  dst.begin() + static_cast<std::ptrdiff_t>(out));
    } else if (j < end) {
        std::move(src.begin() + static_cast<std::ptrdiff_t>(j),
                  src.begin() + static_cast<std::ptrdiff_t>(end),
                  dst.begin() + static_cast<std::ptrdiff_t>(out));
    }
}


template <typename T, typename Less = std::less<T>>
void mergeTwoAdjacentRunsToDestBranchless(
    std::vector<T>& src,
    std::vector<T>& dst,
    std::size_t begin,
    std::size_t mid,
    std::size_t end,
    Less less = Less{}
) {
    T* const srcBase = src.data();
    T* const dstBase = dst.data();
    if (begin == mid) { std::move(srcBase + mid, srcBase + end, dstBase + begin); return; }
    if (mid == end) { std::move(srcBase + begin, srcBase + mid, dstBase + begin); return; }
    if (!less(srcBase[mid], srcBase[mid - 1])) { std::move(srcBase + begin, srcBase + end, dstBase + begin); return; }
    if (!less(srcBase[begin], srcBase[end - 1])) {
        const std::size_t rightLen = end - mid;
        std::move(srcBase + mid, srcBase + end, dstBase + begin);
        std::move(srcBase + begin, srcBase + mid, dstBase + begin + rightLen);
        return;
    }
    T* left = srcBase + begin; T* const leftEnd = srcBase + mid;
    T* right = srcBase + mid; T* const rightEnd = srcBase + end;
    T* out = dstBase + begin;
    auto takeOne = [&]() {
        const bool takeRight = less(*right, *left);
        *out++ = std::move(takeRight ? *right : *left);
        right += static_cast<std::ptrdiff_t>(takeRight);
        left += static_cast<std::ptrdiff_t>(!takeRight);
    };
    while (left + 4 <= leftEnd && right + 4 <= rightEnd) { takeOne(); takeOne(); takeOne(); takeOne(); }
    while (left < leftEnd && right < rightEnd) takeOne();
    if (left < leftEnd) std::move(left, leftEnd, out);
    else if (right < rightEnd) std::move(right, rightEnd, out);
}


template <typename T, typename Less = std::less<T>>
void mergeTwoAdjacentRunsToDestBranchlessBidirectional(
    std::vector<T>& src, std::vector<T>& dst, std::size_t begin,
    std::size_t mid, std::size_t end, Less less = Less{}) {
    T* const sb = src.data(); T* const db = dst.data();
    if (begin == mid) { std::move(sb + mid, sb + end, db + begin); return; }
    if (mid == end) { std::move(sb + begin, sb + mid, db + begin); return; }
    if (!less(sb[mid], sb[mid - 1])) { std::move(sb + begin, sb + end, db + begin); return; }
    if (!less(sb[begin], sb[end - 1])) {
        const std::size_t rl = end - mid;
        std::move(sb + mid, sb + end, db + begin);
        std::move(sb + begin, sb + mid, db + begin + rl); return;
    }
    T* llo=sb+begin; T* lhi=sb+mid; T* rlo=sb+mid; T* rhi=sb+end;
    T* olo=db+begin; T* ohi=db+end;
    while (lhi-llo>=2 && rhi-rlo>=2) {
        const bool fr=less(*rlo,*llo); *olo++=std::move(fr?*rlo:*llo);
        rlo+=(std::ptrdiff_t)fr; llo+=(std::ptrdiff_t)!fr;
        T* lb=lhi-1; T* rb=rhi-1; const bool bl=less(*rb,*lb);
        *--ohi=std::move(bl?*lb:*rb); lhi-=(std::ptrdiff_t)bl; rhi-=(std::ptrdiff_t)!bl;
    }
    while (llo<lhi && rlo<rhi) { const bool tr=less(*rlo,*llo); *olo++=std::move(tr?*rlo:*llo); rlo+=(std::ptrdiff_t)tr; llo+=(std::ptrdiff_t)!tr; }
    if (llo<lhi) std::move(llo,lhi,olo); else if (rlo<rhi) std::move(rlo,rhi,olo);
}


namespace detail {

struct PowerSortMergeNode {
    std::size_t begin = 0;
    std::size_t mid = 0;
    std::size_t end = 0;
    std::size_t left = 0;
    std::size_t right = 0;
    bool leaf = true;
};

inline std::size_t makePowerSortMergedNode(
    std::vector<PowerSortMergeNode>& nodes,
    std::size_t left,
    std::size_t right
) {
    assert(nodes[left].end == nodes[right].begin);
    const std::size_t index = nodes.size();
    nodes.push_back(PowerSortMergeNode{
        nodes[left].begin,
        nodes[left].end,
        nodes[right].end,
        left,
        right,
        false
    });
    return index;
}

// Power of the boundary between adjacent runs [a,b) and [b,c). It is the
// first binary digit at which the normalized run midpoints differ. Lower
// powers belong higher in the alphabetic merge tree.
inline unsigned powerSortBoundaryPower(
    std::size_t a,
    std::size_t b,
    std::size_t c,
    std::size_t n
) {
    __extension__ typedef unsigned __int128 Wide;
    const Wide denominator = static_cast<Wide>(n) * 2;
    Wide leftMidNumerator = static_cast<Wide>(a) + b;
    Wide rightMidNumerator = static_cast<Wide>(b) + c;

    for (unsigned power = 1; power < 128; ++power) {
        leftMidNumerator *= 2;
        rightMidNumerator *= 2;
        const Wide leftDigit = leftMidNumerator / denominator;
        const Wide rightDigit = rightMidNumerator / denominator;
        if (leftDigit != rightDigit) return power;
        leftMidNumerator %= denominator;
        rightMidNumerator %= denominator;
    }
    return 127;
}

inline std::size_t powerSortLeafCopyCost(
    const std::vector<PowerSortMergeNode>& nodes,
    std::size_t nodeIndex,
    bool outputToArr
) {
    const PowerSortMergeNode& node = nodes[nodeIndex];
    if (node.leaf) return outputToArr ? node.end - node.begin : 0;
    return powerSortLeafCopyCost(nodes, node.left, !outputToArr) +
           powerSortLeafCopyCost(nodes, node.right, !outputToArr);
}

template <typename T, typename Less>
void materializePowerSortTree(
    const std::vector<PowerSortMergeNode>& nodes,
    std::size_t nodeIndex,
    bool outputToArr,
    std::vector<T>& tmp,
    std::vector<T>& arr,
    Less less
) {
    const PowerSortMergeNode& node = nodes[nodeIndex];
    if (node.leaf) {
        if (outputToArr) {
            std::move(tmp.begin() + static_cast<std::ptrdiff_t>(node.begin),
                      tmp.begin() + static_cast<std::ptrdiff_t>(node.end),
                      arr.begin() + static_cast<std::ptrdiff_t>(node.begin));
        }
        return;
    }

    materializePowerSortTree(nodes, node.left, !outputToArr, tmp, arr, less);
    materializePowerSortTree(nodes, node.right, !outputToArr, tmp, arr, less);

    std::vector<T>& src = outputToArr ? tmp : arr;
    std::vector<T>& dst = outputToArr ? arr : tmp;
    mergeTwoAdjacentRunsToDest(
        src, dst, node.begin, node.mid, node.end, less);
}

// PowerSort's tree construction and irregular materialization are worthwhile
// only for run sets where the predicted merge-tree improvement is substantial.
// Selection uses the raw reconstructed run sizes so non-selected inputs pay
// only one short metadata scan and remain on the sequential adjacent baseline.
inline bool shouldUsePowerSortMerge(
    const std::vector<std::size_t>& runStart,
    std::size_t n
) {
    if (n < 32768 || runStart.size() <= 2) return false;

    const std::size_t runCount = runStart.size() - 1;
    std::size_t maxLength = 0;
    for (std::size_t r = 0; r < runCount; ++r) {
        maxLength = std::max(
            maxLength, runStart[r + 1] - runStart[r]);
    }

    __extension__ typedef unsigned __int128 Wide;

    // Sparse-noise shape: one dominant run, but raw average run length remains
    // large enough that tree construction does not dominate. This admits the
    // measured 1-5% noise cases and rejects dense 10-20% noise.
    const bool sparseDominant =
        static_cast<Wide>(runCount) * 35 <= n &&
        static_cast<Wide>(maxLength) * runCount >=
            static_cast<Wide>(8) * n;

    // Compact block shape: dozens of uneven runs. Very small run counts remain
    // on adjacent merging because they do not amortize the tree path reliably.
    const bool compactImbalanced =
        runCount >= 16 && runCount <= 128 &&
        static_cast<Wide>(maxLength) * runCount * 4 >=
            static_cast<Wide>(9) * n;

    return sparseDominant || compactImbalanced;
}

template <typename T, typename Less>
void mergeRunsPowerSortStyle(
    std::vector<T>& tmp,
    std::vector<T>& arr,
    std::vector<std::size_t> runStart,
    Less less
) {
    const std::size_t n = tmp.size();
    if (n == 0) {
        arr.clear();
        return;
    }

    // Match the baseline's initial ordered-boundary elimination pass before
    // assigning powers to the surviving boundaries.
    if (runStart.size() > 2) {
        std::size_t write = 1;
        for (std::size_t r = 1; r + 1 < runStart.size(); ++r) {
            const std::size_t boundary = runStart[r];
            if (less(tmp[boundary], tmp[boundary - 1]))
                runStart[write++] = boundary;
        }
        runStart[write++] = n;
        runStart.resize(write);
    }

    const std::size_t runCount = runStart.size() - 1;
    if (runCount == 0) {
        arr.clear();
        return;
    }
    if (runCount == 1) {
        arr.swap(tmp);
        return;
    }

    std::vector<PowerSortMergeNode> nodes;
    nodes.reserve(runCount * 2 - 1);
    std::vector<std::size_t> leaves;
    leaves.reserve(runCount);
    for (std::size_t r = 0; r < runCount; ++r) {
        leaves.push_back(nodes.size());
        nodes.push_back(PowerSortMergeNode{
            runStart[r], runStart[r], runStart[r + 1], 0, 0, true});
    }

    std::vector<std::size_t> stackNodes;
    std::vector<unsigned> stackPowers;
    stackNodes.reserve(std::bit_width(runCount) + 2);
    stackPowers.reserve(std::bit_width(runCount) + 2);

    std::size_t current = leaves[0];
    for (std::size_t r = 1; r < runCount; ++r) {
        const unsigned power = powerSortBoundaryPower(
            runStart[r - 1], runStart[r], runStart[r + 1], n);

        while (!stackPowers.empty() && stackPowers.back() > power) {
            current = makePowerSortMergedNode(
                nodes, stackNodes.back(), current);
            stackNodes.pop_back();
            stackPowers.pop_back();
        }

        stackNodes.push_back(current);
        stackPowers.push_back(power);
        current = leaves[r];
    }

    while (!stackNodes.empty()) {
        current = makePowerSortMergedNode(
            nodes, stackNodes.back(), current);
        stackNodes.pop_back();
        stackPowers.pop_back();
    }

    const std::size_t copyToArr =
        powerSortLeafCopyCost(nodes, current, true);
    const std::size_t copyToTmp =
        powerSortLeafCopyCost(nodes, current, false);
    const bool outputToArr = copyToArr <= copyToTmp;

    materializePowerSortTree(
        nodes, current, outputToArr, tmp, arr, less);
    if (!outputToArr) arr.swap(tmp);
}

} // namespace detail

enum class MergeSchedule {
    adjacent_pairs,
    defer_dominant_first_endpoint
};

template <typename T, typename Less = std::less<T>>
void mergeRunsFromTmpToArr(
    std::vector<T>& tmp,
    std::vector<T>& arr,
    std::vector<std::size_t> runStart,
    Less less = Less{},
    MergeSchedule schedule = MergeSchedule::adjacent_pairs,
    bool branchlessRandomMerge = false,
    unsigned gallopTrigger = 7,
    bool bidirectionalBranchlessMerge = false
) {

    const std::size_t n = tmp.size();

    if (n == 0) {
        arr.clear();
        return;
    }

    const std::size_t initialRuns = runStart.size() - 1;

    if (initialRuns == 0) {
        arr.clear();
        return;
    }

    // If there is only one run, tmp is already sorted.
    // Make arr the owner of the sorted data in O(1).
    if (initialRuns == 1) {
        arr.swap(tmp);
        return;
    }

    std::vector<std::size_t> currentStarts = std::move(runStart);
    std::vector<std::size_t> nextStarts;
    nextStarts.reserve((initialRuns + 1) / 2 + 1);

    // First pass reads tmp and writes arr.
    std::vector<T>* src = &tmp;
    std::vector<T>* dst = &arr;

    bool probeOrderedBoundaries = true;
    while (true) {
        if (probeOrderedBoundaries && currentStarts.size() > 2) {
            const std::size_t oldRuns = currentStarts.size() - 1;
            std::size_t write = 1;
            for (std::size_t r = 1; r + 1 < currentStarts.size(); ++r) {
                const std::size_t boundary = currentStarts[r];
                if (less((*src)[boundary], (*src)[boundary - 1]))
                    currentStarts[write++] = boundary;
            }
            currentStarts[write++] = n;
            currentStarts.resize(write);
            const std::size_t newRuns = currentStarts.size() - 1;
            const std::size_t removed = oldRuns - newRuns;
            probeOrderedBoundaries = newRuns <= 64 || removed * 4 >= oldRuns;
        }
        if (currentStarts.size() <= 2) break;
        const std::size_t numRuns = currentStarts.size() - 1;

        assert(dst->size() == n);

        nextStarts.clear();
        nextStarts.push_back(currentStarts[0]);

        std::size_t firstPairRun = 0;
        std::size_t pairedRunLimit = numRuns;

        if (schedule == MergeSchedule::defer_dominant_first_endpoint &&
            (numRuns & 1u) != 0u && numRuns >= 3) {
            const std::size_t firstLen = currentStarts[1] - currentStarts[0];
            const std::size_t secondLen = currentStarts[2] - currentStarts[1];
            const std::size_t lastLen = currentStarts[numRuns] - currentStarts[numRuns - 1];
            if (firstLen > lastLen && firstLen >= 4 * secondLen) {
                const std::size_t begin = currentStarts[0];
                const std::size_t end = currentStarts[1];
                std::move(src->begin() + static_cast<std::ptrdiff_t>(begin),
                          src->begin() + static_cast<std::ptrdiff_t>(end),
                          dst->begin() + static_cast<std::ptrdiff_t>(begin));
                nextStarts.push_back(end);
                firstPairRun = 1;
            } else {
                pairedRunLimit = numRuns - 1;
            }
        } else if ((numRuns & 1u) != 0u) {
            pairedRunLimit = numRuns - 1;
        }

        for (std::size_t r = firstPairRun; r < pairedRunLimit; r += 2) {
            const std::size_t begin = currentStarts[r];
            const std::size_t mid = currentStarts[r + 1];
            const std::size_t end = currentStarts[r + 2];
            if (branchlessRandomMerge) {
                if (bidirectionalBranchlessMerge)
                    mergeTwoAdjacentRunsToDestBranchlessBidirectional(*src, *dst, begin, mid, end, less);
                else
                    mergeTwoAdjacentRunsToDestBranchless(*src, *dst, begin, mid, end, less);
            } else {
                mergeTwoAdjacentRunsToDest(
                    *src, *dst, begin, mid, end, less, gallopTrigger);
            }
            nextStarts.push_back(end);
        }

        if (pairedRunLimit != numRuns) {
            const std::size_t begin = currentStarts[numRuns - 1];
            const std::size_t end = currentStarts[numRuns];
            std::move(src->begin() + static_cast<std::ptrdiff_t>(begin),
                      src->begin() + static_cast<std::ptrdiff_t>(end),
                      dst->begin() + static_cast<std::ptrdiff_t>(begin));
            nextStarts.push_back(end);
        }

        currentStarts.swap(nextStarts);
        std::swap(src, dst);
    }

    // After the loop, src points to the buffer containing the fully sorted run.
    // Ensure arr is the final output buffer.
    if (src != &arr) {
        arr.swap(tmp);
    }

}

// =========================================================
// Combined simulation + reconstruction wrapper
// =========================================================
//
// After this returns:
//   tmp[runStart[r] ... runStart[r + 1]) is one ascending run.
//
// The first merge pass should read from tmp and write to arr.

// Sentinel-free convenience wrapper.
template <typename T, typename Less = std::less<T>>
ReconstructedRuns simulateAndReconstructRunsToTemp(
    const std::vector<T>& arr,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(arr, less, false, 0, 64, true);

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintNormalizedForAvx2(
            arr,
            std::move(sim.blueprint),
            std::move(sim.ascCounts),
            std::move(sim.descCounts),
            tmp,
            false,
            true,
            &sim.probeOverflowSorted,
            sim.probeOverflowTargetAscPile,
            less
        );

    return ReconstructedRuns{
        std::move(runStart),
        true
    };
}

// =========================================================
// Public sort entry point
// =========================================================
//
// Sorts arr in place. avx2 falls back to the scalar search path when its narrow
// AVX2 exact-eight-tail specialization does not apply.

template <typename T, typename Less = std::less<T>>
void sortWithProbePileCap(std::vector<T>& arr, std::size_t probePileCap, std::size_t probeValues, Less less, bool bidirectionalBranchlessMerge, bool normalizedReconstruction = true, bool naturalRunRoute = false, bool enableWindowAvx = false, bool enableSpecializedRoutes = false, bool enableCoherentValuePileCache = true, bool enableRawBulkSpanDecode = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_avx2_legacy::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_avx2_legacy::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) {
        return;
    }

    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (enableSpecializedRoutes) {
            bool prefixMayMix = true;
            if (arr.size() >= 4) {
                const bool asc = less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
                const bool desc = less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
                prefixMayMix = !(asc || desc);
            }
            if (prefixMayMix && jessesort::simulated_legacy::trySpecializedPrePatienceRoutes(arr, less)) return;
        }
    }

    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(arr, less, true, probePileCap, probeValues, naturalRunRoute, enableWindowAvx, enableCoherentValuePileCache);

    if (sim.alreadySortedAscending) {
        return;
    }

    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    const std::size_t finalPileCount =
        sim.ascCounts.size() + sim.descCounts.size();
    __extension__ typedef unsigned __int128 Wide;
    const Wide finalPileSquare =
        static_cast<Wide>(finalPileCount) * finalPileCount;
    // E053: the branchless/general merge crossover shifts with n.  A single
    // normalized pile-density cutoff over-routes general merging at 10k and
    // branchless merging at 1M.  Keep the rule deliberately coarse and cheap:
    // ~6.0 at <=20k, ~6.75 through the mid-size regime, and ~7.5 at >=500k.
    bool randomDensitySelectsBranchless = false;
    if (arr.size() <= 20000) {
        randomDensitySelectsBranchless =
            finalPileSquare >= static_cast<Wide>(6) * arr.size();
    } else if (arr.size() < 500000) {
        randomDensitySelectsBranchless =
            static_cast<Wide>(4) * finalPileSquare >=
            static_cast<Wide>(27) * arr.size();
    } else {
        randomDensitySelectsBranchless =
            static_cast<Wide>(2) * finalPileSquare >=
            static_cast<Wide>(15) * arr.size();
    }
    const bool useRandomBranchlessMerge =
        arr.size() >= 10000 &&
        sim.earlyRandomLike &&
        randomDensitySelectsBranchless &&
        std::is_trivially_copyable_v<T> &&
        sizeof(T) <= 96;

    std::vector<T> tmp;

    std::vector<std::size_t> runStart;
    if (normalizedReconstruction) {
        runStart = reconstructTaggedBlueprintNormalizedForAvx2(
            arr,
            std::move(sim.blueprint),
            std::move(sim.ascCounts),
            std::move(sim.descCounts),
            tmp,
            false, // ascending-game piles preserve encounter order
            true,  // descending-game piles reverse to become ascending
            &sim.probeOverflowSorted,
            sim.probeOverflowTargetAscPile,
            less,
            enableRawBulkSpanDecode
        );
    } else {
        runStart = reconstructTaggedBlueprintToTemp_WithSplitCounts(
            arr,
            sim.blueprint,
            std::move(sim.ascCounts),
            std::move(sim.descCounts),
            tmp,
            false, // ascending-game piles preserve encounter order
            true,  // descending-game piles reverse to become ascending
            &sim.probeOverflowSorted,
            sim.probeOverflowTargetAscPile,
            less
        );
    }

    // E045: similarly sized moderate run sets benefit from entering gallop mode
    // slightly earlier. This is derived only from final run metadata: 12-128
    // runs, with the largest run no more than 1.25x the mean. Other layouts
    // retain the more conservative 7-win trigger.
    unsigned gallopTrigger = 7;
    const std::size_t runCount = runStart.size() - 1;
    if (!useRandomBranchlessMerge && runCount >= 12 && runCount <= 128) {
        std::size_t maxRun = 0;
        for (std::size_t r = 0; r < runCount; ++r)
            maxRun = std::max(maxRun, runStart[r + 1] - runStart[r]);
        __extension__ typedef unsigned __int128 RouteWide;
        if (static_cast<RouteWide>(maxRun) * runCount * 4 <=
            static_cast<RouteWide>(5) * arr.size()) {
            gallopTrigger = 5;
        }
    }

    if (detail::shouldUsePowerSortMerge(runStart, arr.size())) {
        detail::mergeRunsPowerSortStyle(
            tmp,
            arr,
            std::move(runStart),
            less);
    } else {
        mergeRunsFromTmpToArr(
            tmp,
            arr,
            std::move(runStart),
            less,
            MergeSchedule::adjacent_pairs,
            useRandomBranchlessMerge,
            gallopTrigger,
            bidirectionalBranchlessMerge);
    }
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    // E076: default avx2 deliberately uses the 8-pile / 128-element capped
    // probe so the validated single-register AVX2 lookup is exercised
    // regularly on Random-like inputs. E075 measured this at ~0.32% slower
    // than uncapped avx2, so this is retained as a SIMD-use demonstration,
    // not as a claimed speed optimization.
    sortWithProbePileCap(arr, 8, 128, less, true, true, true, true, true, true, true);
}

} // namespace jessesort::simulated_avx2_legacy
#endif
