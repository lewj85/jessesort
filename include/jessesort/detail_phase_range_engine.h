#ifndef JESSESORT_E251_PHASE_RANGE_ENGINE_H
#define JESSESORT_E251_PHASE_RANGE_ENGINE_H

#include <jessesort/tiny_sort.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <functional>
#include <type_traits>
#include <utility>
#include <climits>
#include <cassert>
#include <bit>
#include <array>
#include <span>


namespace jessesort::simulated_phase_range_engine {

#if defined(__GNUC__) || defined(__clang__)
#define JESSESORT_PREFIX_CONFIRM_NOINLINE __attribute__((noinline))
#else
#define JESSESORT_PREFIX_CONFIRM_NOINLINE
#endif

template <class T, class Less>
JESSESORT_PREFIX_CONFIRM_NOINLINE bool confirmedShortSameDirectionPrefix(
    std::span<const T> arr, std::size_t prefixEnd,
    bool prefixDescending, Less less, std::size_t confirmationLength = 32) {
    if (prefixEnd < 2 || prefixEnd >= confirmationLength ||
        prefixEnd + confirmationLength > arr.size()) return false;
    for (std::size_t j = prefixEnd + 1; j < prefixEnd + confirmationLength; ++j) {
        if (prefixDescending) {
            if (less(arr[j - 1], arr[j])) return false;
        } else {
            if (less(arr[j], arr[j - 1])) return false;
        }
    }
    return true;
}

#undef JESSESORT_PREFIX_CONFIRM_NOINLINE
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
    Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        assert(hint < n);
        if (!less(tails[hint], value) &&
            (hint == 0 || less(tails[hint - 1], value))) {
            return hint;
        }
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
    Less less = Less{}
) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;

    if (n <= 256) {
        assert(hint < n);
        if (!less(value, tails[hint]) &&
            (hint == 0 || less(value, tails[hint - 1]))) {
            return hint;
        }
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
    Less less = Less{}
) {
    const std::size_t pileIndex = findDescendingPileWithTails(
        tails, lastPileIndex, value, less);
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
    Less less = Less{}
) {
    const std::size_t pileIndex = findAscendingPileWithTails(
        tails, lastPileIndex, value, less);
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

// E117: adjacent equivalent values remain in the same game/pile as their
// predecessor.  Reuse that destination rather than repeating the tail search.
template <typename T>
inline void simulateInsertAdjacentEquivalent(
    SimulatedInsertionResult<T>& result,
    bool descendingMode,
    std::size_t lastPileIndexAscending,
    std::size_t lastPileIndexDescending,
    std::size_t originalIndex
) {
    if (descendingMode) {
        assert(lastPileIndexDescending < result.descCounts.size());
        ++result.descCounts[lastPileIndexDescending];
        result.blueprint[originalIndex] =
            makeDescTag(static_cast<uint32_t>(lastPileIndexDescending));
    } else {
        assert(lastPileIndexAscending < result.ascCounts.size());
        ++result.ascCounts[lastPileIndexAscending];
        result.blueprint[originalIndex] =
            makeAscTag(static_cast<uint32_t>(lastPileIndexAscending));
    }
}

// E183 candidate: on a structurally gated mostly-sorted input, rescue a strict local
// valley only when ordinary descending ownership would create a new pile and the
// value can instead continue an existing ascending pile. The gate is selected
// outside this hot continuation loop.
template <bool EnableAdjacentEqualFastPath, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionValleyRescue(
    std::span<const T> arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less
) {
    auto process = [&](const T& previous, const T& value, std::size_t i) {
        bool desc = less(value, previous);
        bool asc = !desc && less(previous, value);
        if (desc && i + 1 < arr.size() && less(value, arr[i + 1])) {
            const bool wouldCreateDesc = result.descTails.empty() || less(result.descTails.back(), value);
            const bool canContinueAsc = !result.ascTails.empty() && !less(value, result.ascTails.back());
            if (wouldCreateDesc && canContinueAsc) { desc = false; asc = true; }
        }
        if (asc) descendingMode = false;
        else if (desc) descendingMode = true;
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(result, descendingMode, lastPileIndexAscending,
                                              lastPileIndexDescending, i);
            return;
        }
        if (descendingMode) {
            simulateInsertValueDescendingPiles(result.descTails, lastPileIndexDescending, value, i,
                                               result.blueprint, result.descCounts, less);
        } else {
            simulateInsertValueAscendingPiles(result.ascTails, lastPileIndexAscending, value, i,
                                              result.blueprint, result.ascCounts, less);
        }
    };
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[start - 1];
        for (std::size_t i = start; i < arr.size(); ++i) { const T value = arr[i]; process(previous, value, i); previous = value; }
    } else {
        for (std::size_t i = start; i < arr.size(); ++i) process(arr[i - 1], arr[i], i);
    }
}

template <bool EnableAdjacentEqualFastPath, bool UseSplit, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertion(
    std::span<const T> arr,
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
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(
                result, descendingMode, lastPileIndexAscending,
                lastPileIndexDescending, i);
            return;
        }
        if (descendingMode) {
            if constexpr (UseSplit) {
                simulateInsertValueDescendingPilesSplit(
                    result.descTails, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, less);
            } else {
                simulateInsertValueDescendingPiles(
                    result.descTails, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, less);
            }
        } else {
            if constexpr (UseSplit) {
                simulateInsertValueAscendingPilesSplit(
                    result.ascTails, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, less);
            } else {
                simulateInsertValueAscendingPiles(
                    result.ascTails, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, less);
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




// E142: coherent direct-mapped value->pile cache for uniform low/moderate
// cardinality Patience insertion. Cache entries are invalidated when their
// pile tail changes, so a matching cache hit needs no live-tail validation.
template <typename T, typename Less>
inline bool coherentValuePileCacheSampleCandidate(std::span<const T> arr, Less less, bool allowModerateUniform = true) {
    if constexpr (!(std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint64_t) && std::is_same_v<std::remove_cv_t<std::remove_reference_t<Less>>, std::less<T>>)) {
        (void)arr; (void)less; return false;
    } else {
        if (arr.size() < 10000) return false;
        struct Slot { std::uint64_t key = 0; unsigned char count = 0; bool valid = false; };
        std::array<Slot, 128> table{};
        std::size_t distinct = 0, maxFreq = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) {
            const std::uint64_t key = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(arr[i]));
            std::size_t slot = static_cast<std::size_t>((key * 11400714819323198485ULL) >> 57);
            for (;;) {
                auto& e = table[slot];
                if (!e.valid) { e.valid = true; e.key = key; e.count = 1; ++distinct; maxFreq = std::max<std::size_t>(maxFreq, 1); break; }
                if (e.key == key) { if (e.count != 255) ++e.count; maxFreq = std::max<std::size_t>(maxFreq, e.count); break; }
                slot = (slot + 1) & 127u;
            }
        }
        if (distinct < 8) return false;
        return distinct <= 32 || (allowModerateUniform && distinct <= 50 && maxFreq <= 4);
    }
}

template <bool EnableAdjacentEqualFastPath, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionCoherentCache(
    std::span<const T> arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less
) {
    static_assert((std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint64_t) && std::is_same_v<std::remove_cv_t<std::remove_reference_t<Less>>, std::less<T>>));
    struct Entry { std::uint64_t key = 0; std::uint32_t pile = 0; bool valid = false; };
    std::array<Entry, 128> ascCache{}, descCache{};
    auto bucket = [&](const T& v) -> std::size_t {
        return static_cast<std::size_t>((static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(v)) * 11400714819323198485ULL) >> 57);
    };
    auto seed = [&](const std::vector<T>& tails, auto& cache) {
        for (std::size_t p = 0; p < tails.size(); ++p) {
            const std::size_t b = bucket(tails[p]);
            cache[b] = Entry{static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(tails[p])), static_cast<std::uint32_t>(p), true};
        }
    };
    seed(result.ascTails, ascCache); seed(result.descTails, descCache);

    auto process = [&](const T& previous, const T& value, std::size_t i) {
        if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(result, descendingMode,
                lastPileIndexAscending, lastPileIndexDescending, i);
            return;
        }
        auto& tails = descendingMode ? result.descTails : result.ascTails;
        auto& counts = descendingMode ? result.descCounts : result.ascCounts;
        auto& cache = descendingMode ? descCache : ascCache;
        const std::uint64_t key = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(value));
        const std::size_t b = bucket(value);
        auto& e = cache[b];
        if (e.valid && e.key == key && e.pile < tails.size()) {
            const std::size_t p = e.pile;
            ++counts[p];
            if (descendingMode) { lastPileIndexDescending = p; result.blueprint[i] = makeDescTag(static_cast<std::uint32_t>(p)); }
            else { lastPileIndexAscending = p; result.blueprint[i] = makeAscTag(static_cast<std::uint32_t>(p)); }
            return;
        }
        const std::size_t p = descendingMode
            ? findDescendingPileWithTailsNoHint(tails, value, less)
            : findAscendingPileWithTailsNoHint(tails, value, less);
        if (p < tails.size()) {
            const T old = tails[p];
            const std::size_t ob = bucket(old);
            auto& oe = cache[ob];
            if (oe.valid && oe.key == static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(old)) && oe.pile == p) oe.valid = false;
            tails[p] = value; ++counts[p];
        } else {
            tails.push_back(value); counts.push_back(1);
        }
        cache[b] = Entry{key, static_cast<std::uint32_t>(p), true};
        if (descendingMode) { lastPileIndexDescending = p; result.blueprint[i] = makeDescTag(static_cast<std::uint32_t>(p)); }
        else { lastPileIndexAscending = p; result.blueprint[i] = makeAscTag(static_cast<std::uint32_t>(p)); }
    };

    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[start - 1];
        for (std::size_t i = start; i < arr.size(); ++i) { const T value = arr[i]; process(previous, value, i); previous = value; }
    } else {
        for (std::size_t i = start; i < arr.size(); ++i) process(arr[i - 1], arr[i], i);
    }
}

template <bool EnableAdjacentEqualFastPath, typename T, typename Less>
JESSESORT_CONTINUE_NOINLINE void continuePatienceInsertionNoHint(
    std::span<const T> arr,
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
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(
                result, descendingMode, lastPileIndexAscending,
                lastPileIndexDescending, i);
            return;
        }

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

#undef JESSESORT_CONTINUE_NOINLINE

// E098 experimental narrow natural-run continuation. Entered only after a
// very long initial monotone prefix. Later strict monotone source runs are
// bulk-assigned only when both endpoints resolve to the same current pile;
// otherwise that run falls back to ordinary per-element hinted insertion.
template <bool EnableAdjacentEqualFastPath, typename T, typename Less>
inline void continuePatienceInsertionNaturalRuns(
    std::span<const T> arr,
    std::size_t start,
    SimulatedInsertionResult<T>& result,
    std::size_t& lastPileIndexAscending,
    std::size_t& lastPileIndexDescending,
    bool& descendingMode,
    Less less,
    std::size_t minRunLength = 8,
    bool initialDirectionOverride = false,
    bool initialOverrideDescending = false
) {
    const std::size_t n = arr.size();
    std::size_t i = start;
    bool overridePending = initialDirectionOverride;

    auto insertOne = [&](std::size_t j) {
        const T& previous = arr[j - 1];
        const T& value = arr[j];
        if (overridePending) {
            descendingMode = initialOverrideDescending;
            overridePending = false;
        } else if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(
                result, descendingMode, lastPileIndexAscending,
                lastPileIndexDescending, j);
            return;
        }
        if (descendingMode) {
            simulateInsertValueDescendingPiles(
                result.descTails, lastPileIndexDescending, value, j,
                result.blueprint, result.descCounts, less);
        } else {
            simulateInsertValueAscendingPiles(
                result.ascTails, lastPileIndexAscending, value, j,
                result.blueprint, result.ascCounts, less);
        }
    };

    while (i < n) {
        bool asc;
        bool desc;
        if (overridePending) {
            asc = !initialOverrideDescending;
            desc = initialOverrideDescending;
        } else {
            asc = less(arr[i - 1], arr[i]);
            desc = !asc && less(arr[i], arr[i - 1]);
        }
        if (!asc && !desc) {
            insertOne(i++);
            continue;
        }

        std::size_t end = i + 1;
        if (asc) {
            while (end < n && less(arr[end - 1], arr[end])) ++end;
        } else {
            while (end < n && less(arr[end], arr[end - 1])) ++end;
        }
        const std::size_t len = end - i;
        bool batched = false;

        if (len >= minRunLength) {
            if (asc) {
                const std::size_t firstPile =
                    findAscendingPileWithTailsNoHint(result.ascTails, arr[i], less);
                const std::size_t lastPile =
                    findAscendingPileWithTailsNoHint(result.ascTails, arr[end - 1], less);
                if (firstPile == lastPile) {
                    if (firstPile < result.ascTails.size()) {
                        result.ascTails[firstPile] = arr[end - 1];
                        result.ascCounts[firstPile] += len;
                    } else {
                        assert(firstPile <= PILE_MASK);
                        result.ascTails.push_back(arr[end - 1]);
                        result.ascCounts.push_back(len);
                    }
                    const uint32_t tag = makeAscTag(static_cast<uint32_t>(firstPile));
                    std::fill(result.blueprint.begin() + static_cast<std::ptrdiff_t>(i),
                              result.blueprint.begin() + static_cast<std::ptrdiff_t>(end), tag);
                    lastPileIndexAscending = firstPile;
                    descendingMode = false;
                    overridePending = false;
                    batched = true;
                }
            } else {
                const std::size_t firstPile =
                    findDescendingPileWithTailsNoHint(result.descTails, arr[i], less);
                const std::size_t lastPile =
                    findDescendingPileWithTailsNoHint(result.descTails, arr[end - 1], less);
                if (firstPile == lastPile) {
                    if (firstPile < result.descTails.size()) {
                        result.descTails[firstPile] = arr[end - 1];
                        result.descCounts[firstPile] += len;
                    } else {
                        assert(firstPile <= PILE_MASK);
                        result.descTails.push_back(arr[end - 1]);
                        result.descCounts.push_back(len);
                    }
                    const uint32_t tag = makeDescTag(static_cast<uint32_t>(firstPile));
                    std::fill(result.blueprint.begin() + static_cast<std::ptrdiff_t>(i),
                              result.blueprint.begin() + static_cast<std::ptrdiff_t>(end), tag);
                    lastPileIndexDescending = firstPile;
                    descendingMode = true;
                    overridePending = false;
                    batched = true;
                }
            }
        }
        if (!batched) {
            for (std::size_t j = i; j < end; ++j) insertOne(j);
        }
        i = end;
    }
}

template <bool EnableAdjacentEqualFastPath, typename T, typename Less = std::less<T>>
SimulatedInsertionResult<T> simulatePatienceInsertionBlueprintImpl(
    std::span<const T> arr,
    Less less = Less{},
    bool enableEarlyRandomInsertionRoute = false,
    bool enableNaturalRunRoute = false,
    bool enableCoherentValuePileCache = false
) {
    constexpr std::size_t MinPrefixPileLength = 32;
    const std::size_t n = arr.size();
    SimulatedInsertionResult<T> result;
    if (n == 0) return result;

    enum class PrefixDirection { Unknown, Ascending, Descending };
    PrefixDirection prefixDirection = PrefixDirection::Unknown;
    std::size_t prefixEnd = 1;
    for (; prefixEnd < n; ++prefixEnd) {
        const T& previous = arr[prefixEnd - 1];
        const T& value = arr[prefixEnd];
        if (prefixDirection == PrefixDirection::Ascending) {
            if (less(value, previous)) break;
        } else if (prefixDirection == PrefixDirection::Descending) {
            if (less(previous, value)) break;
        } else if (less(previous, value)) {
            prefixDirection = PrefixDirection::Ascending;
        } else if (less(value, previous)) {
            prefixDirection = PrefixDirection::Descending;
        }
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

    // E181: only materialize a short initial run when a cold 32-value
    // confirmation proves the post-boundary continuation has the same direction.
    const bool confirmedShortSameDirectionPrefix =
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd < MinPrefixPileLength &&
        jessesort::simulated_phase_range_engine::confirmedShortSameDirectionPrefix(
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

    const bool naturalRunCandidate =
        enableNaturalRunRoute &&
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd >= MinPrefixPileLength &&
        prefixEnd * 8 >= n;
    if (naturalRunCandidate && processStart < n) {
        continuePatienceInsertionNaturalRuns<EnableAdjacentEqualFastPath>(
            arr, processStart, result,
            lastPileIndexAscending, lastPileIndexDescending,
            descendingMode, less, 8,
            postPrefixDirectionOverride, postPrefixOverrideDescending);
        return result;
    }

    auto processValueSample = [&](const T& previous, const T& value, std::size_t i) {
        if (postPrefixDirectionOverride) {
            descendingMode = postPrefixOverrideDescending;
            postPrefixDirectionOverride = false;
        } else if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
        else if constexpr (EnableAdjacentEqualFastPath) {
            simulateInsertAdjacentEquivalent(
                result, descendingMode, lastPileIndexAscending,
                lastPileIndexDescending, i);
            return true;
        }
        bool hit;
        if (descendingMode) {
            const bool hadPiles = !result.descTails.empty();
            const std::size_t oldHint = lastPileIndexDescending;
            simulateInsertValueDescendingPiles(
                result.descTails, lastPileIndexDescending, value, i,
                result.blueprint, result.descCounts, less);
            hit = hadPiles && oldHint == lastPileIndexDescending;
        } else {
            const bool hadPiles = !result.ascTails.empty();
            const std::size_t oldHint = lastPileIndexAscending;
            simulateInsertValueAscendingPiles(
                result.ascTails, lastPileIndexAscending, value, i,
                result.blueprint, result.ascCounts, less);
            hit = hadPiles && oldHint == lastPileIndexAscending;
        }
        return hit;
    };

    // Sample once, then dispatch to a specialized continuation loop. A >=75%
    // exact-hint rate favors the split hinted path. Sustained high-pile Random-like
    // structure instead uses the restored no-hint continuation (E065); other inputs
    // retain the ordinary inline search. The decision is made outside the hot loop.
    constexpr std::size_t SampleValues = 64;
    constexpr std::size_t StructureProbeValues = 64;
    const std::size_t sampleEnd = std::min(n, processStart + SampleValues);
    std::size_t sampleHits = 0;
    std::size_t sampleCount = 0;
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[processStart - 1];
        std::size_t i = processStart;
        for (; i < sampleEnd; ++i) {
            const T value = arr[i];
            sampleHits += processValueSample(previous, value, i) ? 1u : 0u;
            ++sampleCount;
            if ((processStart >= StructureProbeValues && sampleCount == StructureProbeValues) ||
                (processStart < StructureProbeValues && i + 1 == StructureProbeValues)) {
                result.earlyRandomLike =
                    result.ascCounts.size() >= 6 && result.descCounts.size() >= 6;
            }
            previous = value;
        }
        const bool earlyRandomInsertion =
            enableEarlyRandomInsertionRoute &&
            n >= 10000 &&
            result.earlyRandomLike &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 2 * sizeof(void*);
        const bool useCoherentCache = enableCoherentValuePileCache &&
            result.earlyRandomLike &&
            coherentValuePileCacheSampleCandidate(arr, less);
        const bool useSplit = sampleCount != 0 && sampleHits * 4 >= sampleCount * 3;
        const std::size_t e183ProbePiles = result.ascCounts.size() + result.descCounts.size();
        const bool useValleyRescue = n >= 10000 && e183ProbePiles >= 3 && e183ProbePiles <= 12;
        if (i < n) {
            if (useValleyRescue) continuePatienceInsertionValleyRescue<EnableAdjacentEqualFastPath>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useCoherentCache) {
                if constexpr ((std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint64_t) && std::is_same_v<std::remove_cv_t<std::remove_reference_t<Less>>, std::less<T>>))
                    continuePatienceInsertionCoherentCache<EnableAdjacentEqualFastPath>(
                        arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            } else if (earlyRandomInsertion) continuePatienceInsertionNoHint<EnableAdjacentEqualFastPath>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useSplit) continuePatienceInsertion<EnableAdjacentEqualFastPath, true>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else continuePatienceInsertion<EnableAdjacentEqualFastPath, false>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
        }
    } else {
        std::size_t i = processStart;
        for (; i < sampleEnd; ++i) {
            sampleHits += processValueSample(arr[i - 1], arr[i], i) ? 1u : 0u;
            ++sampleCount;
            if ((processStart >= StructureProbeValues && sampleCount == StructureProbeValues) ||
                (processStart < StructureProbeValues && i + 1 == StructureProbeValues)) {
                result.earlyRandomLike =
                    result.ascCounts.size() >= 6 && result.descCounts.size() >= 6;
            }
        }
        const bool earlyRandomInsertion =
            enableEarlyRandomInsertionRoute &&
            n >= 10000 &&
            result.earlyRandomLike &&
            std::is_trivially_copyable_v<T> &&
            sizeof(T) <= 2 * sizeof(void*);
        const bool useCoherentCache = enableCoherentValuePileCache &&
            result.earlyRandomLike &&
            coherentValuePileCacheSampleCandidate(arr, less);
        const bool useSplit = sampleCount != 0 && sampleHits * 4 >= sampleCount * 3;
        const std::size_t e183ProbePiles = result.ascCounts.size() + result.descCounts.size();
        const bool useValleyRescue = n >= 10000 && e183ProbePiles >= 3 && e183ProbePiles <= 12;
        if (i < n) {
            if (useValleyRescue) continuePatienceInsertionValleyRescue<EnableAdjacentEqualFastPath>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useCoherentCache) {
                if constexpr ((std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint64_t) && std::is_same_v<std::remove_cv_t<std::remove_reference_t<Less>>, std::less<T>>))
                    continuePatienceInsertionCoherentCache<EnableAdjacentEqualFastPath>(
                        arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            } else if (earlyRandomInsertion) continuePatienceInsertionNoHint<EnableAdjacentEqualFastPath>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else if (useSplit) continuePatienceInsertion<EnableAdjacentEqualFastPath, true>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
            else continuePatienceInsertion<EnableAdjacentEqualFastPath, false>(
                arr, i, result, lastPileIndexAscending, lastPileIndexDescending, descendingMode, less);
        }
    }
    return result;
}

// Production route: E117 is enabled for the simulated blueprint path.
template <typename T, typename Less = std::less<T>>
SimulatedInsertionResult<T> simulatePatienceInsertionBlueprint(
    std::span<const T> arr,
    Less less = Less{},
    bool enableEarlyRandomInsertionRoute = false,
    bool enableNaturalRunRoute = false,
    bool enableCoherentValuePileCache = false
) {
    return simulatePatienceInsertionBlueprintImpl<true>(
        arr, less, enableEarlyRandomInsertionRoute, enableNaturalRunRoute,
        enableCoherentValuePileCache);
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

template <typename T>
std::vector<std::size_t> reconstructTaggedBlueprintToTemp_WithSplitCounts(
    std::span<const T> arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::vector<T>& tmp,
    bool reverseAscRuns = false,
    bool reverseDescRuns = true
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
        tmp.assign(arr.begin(), arr.end());
    }

    // E079: use one global cursor table with a signed per-run step. This removes
    // the unpredictable ascending/descending branch from the per-element scatter
    // loop. Random reconstruction is a cache-sensitive counting-sort-style scatter,
    // so keeping the hot loop to one cursor lookup/update materially reduces cost.
    std::vector<std::size_t> cursor(numRuns);
    std::vector<std::ptrdiff_t> step(numRuns, 1);
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        cursor[p] = reverseDescRuns ? start[p + 1] - 1 : start[p];
        step[p] = reverseDescRuns ? -1 : 1;
    }
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t r = numDescPiles + p;
        cursor[r] = reverseAscRuns ? start[r + 1] - 1 : start[r];
        step[r] = reverseAscRuns ? -1 : 1;
    }

    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        const bool desc = isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(localPileId(tag));
        const std::size_t r = desc ? local : numDescPiles + local;
        const std::size_t pos = cursor[r];
        cursor[r] = static_cast<std::size_t>(
            static_cast<std::ptrdiff_t>(cursor[r]) + step[r]);
        tmp[pos] = arr[i];
    }
    return start;
}

// E081 simulated-specific reconstruction path.  The blueprint is moved into this helper
// so its packed local tags can be normalized in place without another allocation.
// Other variations keep the generic reconstruction routine unchanged.
template <typename T>
std::vector<std::size_t> reconstructTaggedBlueprintNormalizedForSimulated(
    std::span<const T> arr,
    std::vector<uint32_t> blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::vector<T>& tmp,
    bool reverseAscRuns = false,
    bool reverseDescRuns = true,
    bool enableBulkTagSpans = true,
    bool enableOnTheFlyBulkDecode = false
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
    if constexpr (std::is_default_constructible_v<T>) tmp.resize(n);
    else tmp.assign(arr.begin(), arr.end());

    std::vector<std::size_t> cursor(numRuns);
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        cursor[p] = reverseDescRuns ? start[p + 1] - 1 : start[p];
    }
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        const std::size_t r = numDescPiles + p;
        cursor[r] = reverseAscRuns ? start[r + 1] - 1 : start[r];
    }
    // E086: deriving direction from the normalized run ID loses at small/mid n,
    // but wins once cursor+step metadata pressure grows. Keep E079's explicit
    // step table below 500k and eliminate it only in the large-n regime.
    const bool deriveStepFromRunId = n >= 500000;
    std::vector<std::ptrdiff_t> step;
    if (!deriveStepFromRunId) {
        step.assign(numRuns, 1);
        for (std::size_t p = 0; p < numDescPiles; ++p)
            step[p] = reverseDescRuns ? -1 : 1;
        for (std::size_t p = 0; p < numAscPiles; ++p) {
            const std::size_t r = numDescPiles + p;
            step[r] = reverseAscRuns ? -1 : 1;
        }
    }

    std::size_t tagAdjacencyHits = 0;
    const std::size_t tagProbeEnd = std::min<std::size_t>(n, 64);
    for (std::size_t i = 1; i < tagProbeEnd; ++i)
        tagAdjacencyHits += blueprint[i] == blueprint[i - 1] ? 1u : 0u;
    const bool useBulkTagSpans = enableBulkTagSpans && tagProbeEnd >= 16 &&
        tagAdjacencyHits * 4 >= (tagProbeEnd - 1) * 3;

    const bool decodeBulkTagsOnTheFly =
        enableOnTheFlyBulkDecode && useBulkTagSpans;

    if (!decodeBulkTagsOnTheFly) {
        for (std::size_t i = 0; i < n; ++i) {
            const uint32_t tag = blueprint[i];
            const std::size_t local = static_cast<std::size_t>(localPileId(tag));
            blueprint[i] = static_cast<uint32_t>(
                isDescTag(tag) ? local : numDescPiles + local);
        }
    }

    if (useBulkTagSpans) {
        std::size_t i = 0;
        while (i < n) {
            const uint32_t tag = blueprint[i];
            const std::size_t r = decodeBulkTagsOnTheFly
                ? (isDescTag(tag)
                    ? static_cast<std::size_t>(localPileId(tag))
                    : numDescPiles + static_cast<std::size_t>(localPileId(tag)))
                : static_cast<std::size_t>(tag);
            std::size_t j = i + 1;
            while (j < n && blueprint[j] == tag) ++j;
            const std::size_t len = j - i;
            const bool reverseRun = r < numDescPiles ? reverseDescRuns : reverseAscRuns;
            if (!reverseRun) {
                const std::size_t pos = cursor[r];
                std::copy(arr.begin() + static_cast<std::ptrdiff_t>(i),
                          arr.begin() + static_cast<std::ptrdiff_t>(j),
                          tmp.begin() + static_cast<std::ptrdiff_t>(pos));
                cursor[r] += len;
            } else {
                const std::size_t pos = cursor[r];
                const std::size_t first = pos + 1 - len;
                std::reverse_copy(arr.begin() + static_cast<std::ptrdiff_t>(i),
                                  arr.begin() + static_cast<std::ptrdiff_t>(j),
                                  tmp.begin() + static_cast<std::ptrdiff_t>(first));
                cursor[r] -= len;
            }
            i = j;
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t r = static_cast<std::size_t>(blueprint[i]);
            const std::size_t pos = cursor[r];
            if (deriveStepFromRunId) {
                cursor[r] += (r < numDescPiles) ? static_cast<std::size_t>(-1) : 1u;
            } else {
                cursor[r] = static_cast<std::size_t>(
                    static_cast<std::ptrdiff_t>(cursor[r]) + step[r]);
            }
            tmp[pos] = arr[i];
        }
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
    // E082: use raw contiguous pointers in the high-entropy branchless kernel.
    // std::vector indexing in this extremely hot loop was measurably more
    // expensive even after inlining.  The merge decisions, four-selection
    // unroll, fast paths, stability behavior, and scheduler are unchanged.
    T* const srcBase = src.data();
    T* const dstBase = dst.data();

    if (!less(srcBase[mid], srcBase[mid - 1])) {
        std::move(srcBase + begin, srcBase + end, dstBase + begin);
        return;
    }
    if (!less(srcBase[begin], srcBase[end - 1])) {
        const std::size_t rightLen = end - mid;
        std::move(srcBase + mid, srcBase + end, dstBase + begin);
        std::move(srcBase + begin, srcBase + mid, dstBase + begin + rightLen);
        return;
    }

    T* left = srcBase + begin;
    T* const leftEnd = srcBase + mid;
    T* right = srcBase + mid;
    T* const rightEnd = srcBase + end;
    T* out = dstBase + begin;

    auto takeOne = [&]() {
        const bool takeRight = less(*right, *left);
        *out++ = std::move(takeRight ? *right : *left);
        right += static_cast<std::ptrdiff_t>(takeRight);
        left += static_cast<std::ptrdiff_t>(!takeRight);
    };

    while (left + 4 <= leftEnd && right + 4 <= rightEnd) {
        takeOne();
        takeOne();
        takeOne();
        takeOne();
    }
    while (left < leftEnd && right < rightEnd) {
        takeOne();
    }
    if (left < leftEnd) {
        std::move(left, leftEnd, out);
    } else if (right < rightEnd) {
        std::move(right, rightEnd, out);
    }
}

// E085 retained for simulated: preserve the same two-run merge and comparison semantics as the
// E082 raw-pointer kernel, but consume from both ends of the runs at once.
// The low-end and high-end selections are independent dependency chains, giving
// an out-of-order CPU useful comparison/move work to overlap without adding the
// winner-selection overhead that made E082's general 4-run fusion regress.
//
// This is deliberately a separate kernel.  simulated-inplace through frozen-live share mergeRunsFromTmpToArr(),
// so simulated can use E085 without silently changing simulated-inplace through frozen-live before propagation is tested.
template <typename T, typename Less = std::less<T>>
void mergeTwoAdjacentRunsToDestBranchlessBidirectional(
    std::vector<T>& src,
    std::vector<T>& dst,
    std::size_t begin,
    std::size_t mid,
    std::size_t end,
    Less less = Less{}
) {
    T* const srcBase = src.data();
    T* const dstBase = dst.data();

    if (!less(srcBase[mid], srcBase[mid - 1])) {
        std::move(srcBase + begin, srcBase + end, dstBase + begin);
        return;
    }
    if (!less(srcBase[begin], srcBase[end - 1])) {
        const std::size_t rightLen = end - mid;
        std::move(srcBase + mid, srcBase + end, dstBase + begin);
        std::move(srcBase + begin, srcBase + mid, dstBase + begin + rightLen);
        return;
    }

    // Remaining source ranges are [leftLo,leftHi) and [rightLo,rightHi).
    // Output is filled simultaneously through [outLo,outHi).
    T* leftLo = srcBase + begin;
    T* leftHi = srcBase + mid;
    T* rightLo = srcBase + mid;
    T* rightHi = srcBase + end;
    T* outLo = dstBase + begin;
    T* outHi = dstBase + end;

    while (leftHi - leftLo >= 2 && rightHi - rightLo >= 2) {
        // Forward merge: ties select the left run, preserving stable semantics.
        const bool frontTakeRight = less(*rightLo, *leftLo);
        *outLo++ = std::move(frontTakeRight ? *rightLo : *leftLo);
        rightLo += static_cast<std::ptrdiff_t>(frontTakeRight);
        leftLo += static_cast<std::ptrdiff_t>(!frontTakeRight);

        // Reverse merge: ties must select the right run because this end is
        // written backwards; that leaves equal left-run values before equal
        // right-run values in the final output.
        T* const leftBack = leftHi - 1;
        T* const rightBack = rightHi - 1;
        const bool backTakeLeft = less(*rightBack, *leftBack);
        *--outHi = std::move(backTakeLeft ? *leftBack : *rightBack);
        leftHi -= static_cast<std::ptrdiff_t>(backTakeLeft);
        rightHi -= static_cast<std::ptrdiff_t>(!backTakeLeft);
    }

    // Finish the small residual middle region with the proven E082-style
    // forward branchless merge.  On balanced high-entropy pairs this is only a
    // handful of values; on skewed pairs it safely handles the longer tail.
    while (leftLo < leftHi && rightLo < rightHi) {
        const bool takeRight = less(*rightLo, *leftLo);
        *outLo++ = std::move(takeRight ? *rightLo : *leftLo);
        rightLo += static_cast<std::ptrdiff_t>(takeRight);
        leftLo += static_cast<std::ptrdiff_t>(!takeRight);
    }
    if (leftLo < leftHi) {
        std::move(leftLo, leftHi, outLo);
    } else if (rightLo < rightHi) {
        std::move(rightLo, rightHi, outLo);
    }
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
    std::span<T> arr,
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
    std::span<T> arr,
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

// Common physical/simulated/simulated-inplace adjacent-pair merge driver selected by E115.
template <class T, class Less>
void mergeRunsAdjacentPairsEnds(std::vector<T>& src, std::vector<T>& dst,
               std::vector<std::size_t>& ends, Less less, bool branchlessRandomMerge,
               bool bidirectionalBranchlessMerge = false) {
    if (ends.size() <= 1) return;
    unsigned gallopTrigger = 7;
    if (!branchlessRandomMerge) {
        const std::size_t runCount = ends.size();
        std::size_t largestRun = 0, prevEnd = 0;
        for (std::size_t end : ends) {
            largestRun = std::max(largestRun, end - prevEnd);
            prevEnd = end;
        }
        // simulated E045 selector, tested independently here for physical physical piles.
        if (runCount >= 12 && runCount <= 128 &&
            static_cast<unsigned long long>(largestRun) * runCount * 4 <=
                static_cast<unsigned long long>(ends.back()) * 5) {
            gallopTrigger = 5;
        }
    }
    bool sourceIsSrc = true;
    bool probeOrderedBoundaries = true;
    std::vector<std::size_t> next;
    next.reserve((ends.size() + 1) / 2);

    while (ends.size() > 1) {
        auto& in = sourceIsSrc ? src : dst;
        auto& out = sourceIsSrc ? dst : src;

        // E109 convergence baseline: match simulated/simulated-inplace's adaptive ordered-boundary
        // probing. Keep probing while the run set is small or the scan removes
        // at least 25% of boundaries; otherwise stop paying for repeated scans.
        if (probeOrderedBoundaries) {
            const std::size_t oldRuns = ends.size();
            next.clear();
            for (std::size_t r = 0; r + 1 < ends.size(); ++r) {
                const std::size_t boundary = ends[r];
                if (less(in[boundary], in[boundary - 1]))
                    next.push_back(boundary);
            }
            next.push_back(ends.back());
            ends.swap(next);
            const std::size_t newRuns = ends.size();
            const std::size_t removed = oldRuns - newRuns;
            probeOrderedBoundaries = newRuns <= 64 || removed * 4 >= oldRuns;
            if (ends.size() <= 1) break;
        }

        next.clear();
        std::size_t left = 0;
        for (std::size_t r = 0; r < ends.size(); r += 2) {
            const std::size_t mid = ends[r];
            if (r + 1 == ends.size()) {
                std::move(in.begin() + left, in.begin() + mid, out.begin() + left);
                next.push_back(mid);
                left = mid;
                continue;
            }
            const std::size_t right = ends[r + 1];

            // Fast path 1: the adjacent runs are already globally ordered.
            // left.max <= right.min
            if (!less(in[mid], in[mid - 1])) {
                std::move(in.begin() + left, in.begin() + right,
                          out.begin() + left);
                next.push_back(right);
                left = right;
                continue;
            }

            // Fast path 2: the runs are disjoint in reverse order, so the
            // merged result is simply right followed by left.
            // right.max <= left.min
            if (!less(in[left], in[right - 1])) {
                const std::size_t rightLen = right - mid;
                std::move(in.begin() + mid, in.begin() + right,
                          out.begin() + left);
                std::move(in.begin() + left, in.begin() + mid,
                          out.begin() + left + rightLen);
                next.push_back(right);
                left = right;
                continue;
            }

            std::size_t i = left, j = mid, k = left;
            if (branchlessRandomMerge) {
                if (bidirectionalBranchlessMerge) {
                    T* llo = in.data() + left; T* lhi = in.data() + mid;
                    T* rlo = in.data() + mid; T* rhi = in.data() + right;
                    T* olo = out.data() + left; T* ohi = out.data() + right;
                    while (lhi - llo >= 2 && rhi - rlo >= 2) {
                        const bool fr = less(*rlo, *llo);
                        *olo++ = std::move(fr ? *rlo : *llo);
                        rlo += (std::ptrdiff_t)fr; llo += (std::ptrdiff_t)!fr;
                        T* lb = lhi - 1; T* rb = rhi - 1;
                        const bool bl = less(*rb, *lb);
                        *--ohi = std::move(bl ? *lb : *rb);
                        lhi -= (std::ptrdiff_t)bl; rhi -= (std::ptrdiff_t)!bl;
                    }
                    while (llo < lhi && rlo < rhi) {
                        const bool tr = less(*rlo, *llo);
                        *olo++ = std::move(tr ? *rlo : *llo);
                        rlo += (std::ptrdiff_t)tr; llo += (std::ptrdiff_t)!tr;
                    }
                    if (llo < lhi) std::move(llo, lhi, olo);
                    else if (rlo < rhi) std::move(rlo, rhi, olo);
                    i = mid; j = right; k = right;
                } else {
                    T* lp = in.data() + left; T* const le = in.data() + mid;
                    T* rp = in.data() + mid; T* const re = in.data() + right;
                    T* op = out.data() + left;
                    auto takeOne = [&] { const bool tr = less(*rp,*lp); *op++ = std::move(tr ? *rp : *lp); rp += (std::ptrdiff_t)tr; lp += (std::ptrdiff_t)!tr; };
                    while (lp + 4 <= le && rp + 4 <= re) { takeOne(); takeOne(); takeOne(); takeOne(); }
                    while (lp < le && rp < re) takeOne();
                    i = (std::size_t)(lp - in.data()); j = (std::size_t)(rp - in.data()); k = (std::size_t)(op - out.data());
                }
            } else {
                unsigned leftWins = 0, rightWins = 0;
                const unsigned GALLOP_TRIGGER = gallopTrigger;
                while (i < mid && j < right) {
                    if (less(in[j], in[i])) {
                        out[k++] = std::move(in[j++]);
                        ++rightWins; leftWins = 0;
                        if (rightWins >= GALLOP_TRIGGER && i < mid && j < right) {
                            std::size_t step = 1;
                            while (j + step < right && less(in[j + step], in[i]))
                                step <<= 1;
                            std::size_t lo = j;
                            std::size_t hi = std::min(right, j + step + 1);
                            while (lo < hi) {
                                const std::size_t m = lo + (hi - lo) / 2;
                                if (less(in[m], in[i])) lo = m + 1;
                                else hi = m;
                            }
                            while (j < lo) out[k++] = std::move(in[j++]);
                            rightWins = 0;
                        }
                    } else {
                        out[k++] = std::move(in[i++]);
                        ++leftWins; rightWins = 0;
                        if (leftWins >= GALLOP_TRIGGER && i < mid && j < right) {
                            std::size_t step = 1;
                            while (i + step < mid && !less(in[j], in[i + step]))
                                step <<= 1;
                            std::size_t lo = i;
                            std::size_t hi = std::min(mid, i + step + 1);
                            while (lo < hi) {
                                const std::size_t m = lo + (hi - lo) / 2;
                                if (!less(in[j], in[m])) lo = m + 1;
                                else hi = m;
                            }
                            while (i < lo) out[k++] = std::move(in[i++]);
                            leftWins = 0;
                        }
                    }
                }
            }
            while (i < mid) out[k++] = std::move(in[i++]);
            while (j < right) out[k++] = std::move(in[j++]);
            next.push_back(right);
            left = right;
        }
        ends.swap(next);
        sourceIsSrc = !sourceIsSrc;
    }
    if (!sourceIsSrc) src.swap(dst);
}


template <class T, class Less>
void mergeRunsAdjacentPairsEndsToSpan(std::vector<T>& src, std::span<T> dst,
               std::vector<std::size_t>& ends, Less less, bool branchlessRandomMerge,
               bool bidirectionalBranchlessMerge = false) {
    if (ends.size() <= 1) return;
    unsigned gallopTrigger = 7;
    if (!branchlessRandomMerge) {
        const std::size_t runCount = ends.size();
        std::size_t largestRun = 0, prevEnd = 0;
        for (std::size_t end : ends) {
            largestRun = std::max(largestRun, end - prevEnd);
            prevEnd = end;
        }
        // simulated E045 selector, tested independently here for physical physical piles.
        if (runCount >= 12 && runCount <= 128 &&
            static_cast<unsigned long long>(largestRun) * runCount * 4 <=
                static_cast<unsigned long long>(ends.back()) * 5) {
            gallopTrigger = 5;
        }
    }
    bool sourceIsSrc = true;
    bool probeOrderedBoundaries = true;
    std::vector<std::size_t> next;
    next.reserve((ends.size() + 1) / 2);

    while (ends.size() > 1) {
        std::span<T> in = sourceIsSrc ? std::span<T>(src) : dst;
        std::span<T> out = sourceIsSrc ? dst : std::span<T>(src);

        // E109 convergence baseline: match simulated/simulated-inplace's adaptive ordered-boundary
        // probing. Keep probing while the run set is small or the scan removes
        // at least 25% of boundaries; otherwise stop paying for repeated scans.
        if (probeOrderedBoundaries) {
            const std::size_t oldRuns = ends.size();
            next.clear();
            for (std::size_t r = 0; r + 1 < ends.size(); ++r) {
                const std::size_t boundary = ends[r];
                if (less(in[boundary], in[boundary - 1]))
                    next.push_back(boundary);
            }
            next.push_back(ends.back());
            ends.swap(next);
            const std::size_t newRuns = ends.size();
            const std::size_t removed = oldRuns - newRuns;
            probeOrderedBoundaries = newRuns <= 64 || removed * 4 >= oldRuns;
            if (ends.size() <= 1) break;
        }

        next.clear();
        std::size_t left = 0;
        for (std::size_t r = 0; r < ends.size(); r += 2) {
            const std::size_t mid = ends[r];
            if (r + 1 == ends.size()) {
                std::move(in.begin() + left, in.begin() + mid, out.begin() + left);
                next.push_back(mid);
                left = mid;
                continue;
            }
            const std::size_t right = ends[r + 1];

            // Fast path 1: the adjacent runs are already globally ordered.
            // left.max <= right.min
            if (!less(in[mid], in[mid - 1])) {
                std::move(in.begin() + left, in.begin() + right,
                          out.begin() + left);
                next.push_back(right);
                left = right;
                continue;
            }

            // Fast path 2: the runs are disjoint in reverse order, so the
            // merged result is simply right followed by left.
            // right.max <= left.min
            if (!less(in[left], in[right - 1])) {
                const std::size_t rightLen = right - mid;
                std::move(in.begin() + mid, in.begin() + right,
                          out.begin() + left);
                std::move(in.begin() + left, in.begin() + mid,
                          out.begin() + left + rightLen);
                next.push_back(right);
                left = right;
                continue;
            }

            std::size_t i = left, j = mid, k = left;
            if (branchlessRandomMerge) {
                if (bidirectionalBranchlessMerge) {
                    T* llo = in.data() + left; T* lhi = in.data() + mid;
                    T* rlo = in.data() + mid; T* rhi = in.data() + right;
                    T* olo = out.data() + left; T* ohi = out.data() + right;
                    while (lhi - llo >= 2 && rhi - rlo >= 2) {
                        const bool fr = less(*rlo, *llo);
                        *olo++ = std::move(fr ? *rlo : *llo);
                        rlo += (std::ptrdiff_t)fr; llo += (std::ptrdiff_t)!fr;
                        T* lb = lhi - 1; T* rb = rhi - 1;
                        const bool bl = less(*rb, *lb);
                        *--ohi = std::move(bl ? *lb : *rb);
                        lhi -= (std::ptrdiff_t)bl; rhi -= (std::ptrdiff_t)!bl;
                    }
                    while (llo < lhi && rlo < rhi) {
                        const bool tr = less(*rlo, *llo);
                        *olo++ = std::move(tr ? *rlo : *llo);
                        rlo += (std::ptrdiff_t)tr; llo += (std::ptrdiff_t)!tr;
                    }
                    if (llo < lhi) std::move(llo, lhi, olo);
                    else if (rlo < rhi) std::move(rlo, rhi, olo);
                    i = mid; j = right; k = right;
                } else {
                    T* lp = in.data() + left; T* const le = in.data() + mid;
                    T* rp = in.data() + mid; T* const re = in.data() + right;
                    T* op = out.data() + left;
                    auto takeOne = [&] { const bool tr = less(*rp,*lp); *op++ = std::move(tr ? *rp : *lp); rp += (std::ptrdiff_t)tr; lp += (std::ptrdiff_t)!tr; };
                    while (lp + 4 <= le && rp + 4 <= re) { takeOne(); takeOne(); takeOne(); takeOne(); }
                    while (lp < le && rp < re) takeOne();
                    i = (std::size_t)(lp - in.data()); j = (std::size_t)(rp - in.data()); k = (std::size_t)(op - out.data());
                }
            } else {
                unsigned leftWins = 0, rightWins = 0;
                const unsigned GALLOP_TRIGGER = gallopTrigger;
                while (i < mid && j < right) {
                    if (less(in[j], in[i])) {
                        out[k++] = std::move(in[j++]);
                        ++rightWins; leftWins = 0;
                        if (rightWins >= GALLOP_TRIGGER && i < mid && j < right) {
                            std::size_t step = 1;
                            while (j + step < right && less(in[j + step], in[i]))
                                step <<= 1;
                            std::size_t lo = j;
                            std::size_t hi = std::min(right, j + step + 1);
                            while (lo < hi) {
                                const std::size_t m = lo + (hi - lo) / 2;
                                if (less(in[m], in[i])) lo = m + 1;
                                else hi = m;
                            }
                            while (j < lo) out[k++] = std::move(in[j++]);
                            rightWins = 0;
                        }
                    } else {
                        out[k++] = std::move(in[i++]);
                        ++leftWins; rightWins = 0;
                        if (leftWins >= GALLOP_TRIGGER && i < mid && j < right) {
                            std::size_t step = 1;
                            while (i + step < mid && !less(in[j], in[i + step]))
                                step <<= 1;
                            std::size_t lo = i;
                            std::size_t hi = std::min(mid, i + step + 1);
                            while (lo < hi) {
                                const std::size_t m = lo + (hi - lo) / 2;
                                if (!less(in[j], in[m])) lo = m + 1;
                                else hi = m;
                            }
                            while (i < lo) out[k++] = std::move(in[i++]);
                            leftWins = 0;
                        }
                    }
                }
            }
            while (i < mid) out[k++] = std::move(in[i++]);
            while (j < right) out[k++] = std::move(in[j++]);
            next.push_back(right);
            left = right;
        }
        ends.swap(next);
        sourceIsSrc = !sourceIsSrc;
    }
    if (sourceIsSrc) std::move(src.begin(), src.end(), dst.begin());
}

enum class MergeSchedule {
    adjacent_pairs,
    defer_dominant_first_endpoint
};

template <typename T, typename Less = std::less<T>>
void mergeRunsFromTmpToArr(
    std::vector<T>& tmp,
    std::span<T> arr,
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
                if (bidirectionalBranchlessMerge) {
                    mergeTwoAdjacentRunsToDestBranchlessBidirectional(
                        *src, *dst, begin, mid, end, less);
                } else {
                    mergeTwoAdjacentRunsToDestBranchless(
                        *src, *dst, begin, mid, end, less);
                }
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
    std::span<const T> arr,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(arr, less);

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintToTemp_WithSplitCounts(
            arr,
            sim.blueprint,
            std::move(sim.ascCounts),
            std::move(sim.descCounts),
            tmp,
            false,
            true
        );

    return ReconstructedRuns{
        std::move(runStart),
        true
    };
}


// =========================================================
// E123 / E128 / E129 specialized primitive backends
// =========================================================

template <typename T, typename Less>
inline constexpr bool specializedIntegralEligible =
    std::is_integral_v<T> && sizeof(T) <= sizeof(std::uint64_t) &&
    std::is_same_v<std::remove_cv_t<std::remove_reference_t<Less>>, std::less<T>>;

template <typename T>
inline std::uint64_t specializedIntegralKey(const T& value) {
    using U = std::make_unsigned_t<T>;
    return static_cast<std::uint64_t>(static_cast<U>(value));
}

template <typename T>
inline std::uint64_t lowCardinalityHash(const T& value) {
    return static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(value)) * 11400714819323198485ULL;
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool lowCardinalityDirectionGate(std::span<const T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        if (arr.size() < 10000 || arr.size() < 8) return false;
        bool sawAsc = false, sawDesc = false;
        for (std::size_t i = 1; i < 8; ++i) {
            if (less(arr[i - 1], arr[i])) sawAsc = true;
            else if (less(arr[i], arr[i - 1])) sawDesc = true;
        }
        return sawAsc && sawDesc;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool lowCardinalitySampleCandidate(std::span<const T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        std::array<std::uint64_t, 128> keys{};
        std::array<unsigned char, 128> valid{};
        std::size_t distinct = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) {
            const std::uint64_t key = specializedIntegralKey(arr[i]);
            std::size_t slot = static_cast<std::size_t>(lowCardinalityHash(arr[i]) >> 57);
            for (;;) {
                if (!valid[slot]) { valid[slot] = 1; keys[slot] = key; ++distinct; break; }
                if (keys[slot] == key) break;
                slot = (slot + 1) & 127u;
            }
        }
        return distinct >= 8 && distinct <= 50;
    }
}

template <typename T>
struct LowCardinalityEntry {
    T key;
    std::size_t count;
};

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool trySortLowCardinalityDirectConfirmed(std::span<T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        struct Slot { T key{}; std::size_t count = 0; bool valid = false; };
        std::array<Slot, 256> table{};
        std::size_t unique = 0;
        for (const T& value : arr) {
            std::size_t slot = static_cast<std::size_t>(lowCardinalityHash(value) >> 56);
            for (;;) {
                auto& e = table[slot];
                if (!e.valid) {
                    if (unique == 128) return false;
                    e.valid = true; e.key = value; e.count = 1; ++unique; break;
                }
                if (e.key == value) { ++e.count; break; }
                slot = (slot + 1) & 255u;
            }
        }
        std::array<LowCardinalityEntry<T>, 128> entries{};
        std::size_t used = 0;
        for (const auto& e : table) if (e.valid) entries[used++] = {e.key, e.count};
        std::sort(entries.begin(), entries.begin() + static_cast<std::ptrdiff_t>(used),
                  [&](const auto& a, const auto& b) { return less(a.key, b.key); });
        std::size_t out = 0;
        for (std::size_t i = 0; i < used; ++i) {
            std::fill_n(arr.begin() + static_cast<std::ptrdiff_t>(out), entries[i].count, entries[i].key);
            out += entries[i].count;
        }
        return true;
    }
}

#include <jessesort/detail_high_entropy_backend.inc>

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool highEntropySampleCandidate(std::span<const T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        if (arr.size() < 10000) return false;
        std::array<std::uint64_t, 128> keys{};
        std::array<unsigned char, 128> valid{};
        std::size_t distinct = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) {
            const std::uint64_t key = specializedIntegralKey(arr[i]);
            std::size_t slot = static_cast<std::size_t>((key * 11400714819323198485ULL) >> 57);
            for (;;) {
                if (!valid[slot]) { valid[slot] = 1; keys[slot] = key; ++distinct; break; }
                if (keys[slot] == key) break;
                slot = (slot + 1) & 127u;
            }
        }
        return distinct >= 44;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool trySortHighEntropyPartitionDirect(std::span<T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less; return false;
    } else {
        if (!highEntropySampleCandidate(std::span<const T>(arr.data(), arr.size()), less)) return false;
        highEntropyQuickSortProduction(
            arr.data(), arr.size(),
            2 * static_cast<int>(std::bit_width(arr.size())), less,
            false, T{});
        return true;
    }
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool dominantValueSampleCandidate(std::span<const T> arr, T& dominant, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)dominant; (void)less; return false;
    } else {
        if (arr.size() < 10000) return false;
        T candidate = arr[0];
        int balance = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            if (arr[i] == candidate) ++balance;
            else if (--balance < 0) { candidate = arr[i]; balance = 1; }
        }
        std::size_t firstEight = 0;
        for (std::size_t i = 0; i < 8; ++i) firstEight += arr[i] == candidate ? 1u : 0u;
        if (firstEight < 6) return false;
        std::size_t sampleCount = 0;
        const std::size_t end = std::min<std::size_t>(64, arr.size());
        for (std::size_t i = 0; i < end; ++i) sampleCount += arr[i] == candidate ? 1u : 0u;
        if (sampleCount < 56) return false;
        dominant = candidate;
        return true;
    }
}


// Shared pre-Patience router used by physical through avx2 propagation experiments.  The
// caller is responsible for cheap structural/monotonic guards before entering
// this helper.  Returning true means the input was fully sorted by a
// specialized backend and no pile representation should be built.
template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool trySpecializedPrePatienceRoutes(std::span<T> arr, Less less) {
    if constexpr (!specializedIntegralEligible<T, Less>) {
        (void)arr; (void)less;
        return false;
    } else {
        T dominant = arr[0];
        if (dominantValueSampleCandidate(std::span<const T>(arr.data(), arr.size()), dominant, less)) {
            highEntropyQuickSort(
                arr.data(), arr.size(),
                2 * static_cast<int>(std::bit_width(arr.size())), less,
                false, T{}, false,
                true, nullptr, false);
            return true;
        }

        if (lowCardinalityDirectionGate(std::span<const T>(arr.data(), arr.size()), less) &&
            lowCardinalitySampleCandidate(std::span<const T>(arr.data(), arr.size()), less) &&
            trySortLowCardinalityDirectConfirmed(arr, less)) {
            return true;
        }

        bool highEntropyPrefixAlternates = false;
        if (arr.size() >= 8) {
            int previousDirection = 0;
            highEntropyPrefixAlternates = true;
            for (std::size_t i = 1; i < 8; ++i) {
                int direction = 0;
                if (less(arr[i - 1], arr[i])) direction = 1;
                else if (less(arr[i], arr[i - 1])) direction = -1;
                if (direction == 0 ||
                    (previousDirection != 0 && direction == previousDirection)) {
                    highEntropyPrefixAlternates = false;
                    break;
                }
                previousDirection = direction;
            }
        }
        if (!highEntropyPrefixAlternates &&
            trySortHighEntropyPartitionDirect(arr, less)) {
            return true;
        }
        return false;
    }
}

template <typename T>
inline bool shouldUseRandomBranchlessMerge(
    const SimulatedInsertionResult<T>& sim, std::size_t n
) {
    const std::size_t finalPileCount = sim.ascCounts.size() + sim.descCounts.size();
    __extension__ typedef unsigned __int128 Wide;
    const Wide finalPileSquare = static_cast<Wide>(finalPileCount) * finalPileCount;
    bool density = false;
    if (n <= 20000) {
        density = static_cast<Wide>(2) * finalPileSquare >= static_cast<Wide>(7) * n;
    } else if (n < 500000) {
        density = static_cast<Wide>(4) * finalPileSquare >= static_cast<Wide>(17) * n;
    } else {
        density = finalPileSquare >= static_cast<Wide>(5) * n;
    }
    return n >= 10000 && sim.earlyRandomLike && density &&
           std::is_trivially_copyable_v<T> && sizeof(T) <= 96;
}

// =========================================================
// Public sort entry point
// =========================================================
//
// Sorts arr in place. Internal reconstruction buffers remain an implementation detail.

template <typename T, typename Less = std::less<T>>
void sortImplCore(std::span<T> arr, Less less, bool enableNaturalRunRoute,
                  bool enableLowCardinalityDirect = true,
                  bool enableDominantValueDirect = true,
                  bool enableHighEntropyPartition = true,
                  bool enableCoherentValuePileCache = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_legacy::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_legacy::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) {
        return;
    }

    bool specialValuePrefixMayMix = true;
    if (arr.size() >= 4) {
        const bool firstThreeAscending =
            less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
        const bool firstThreeDescending =
            less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
        specialValuePrefixMayMix = !(firstThreeAscending || firstThreeDescending);
    }

    if constexpr (specializedIntegralEligible<T, Less>) {
        if (enableDominantValueDirect && specialValuePrefixMayMix) {
            T dominant = arr[0];
            if (dominantValueSampleCandidate(std::span<const T>(arr.data(), arr.size()), dominant, less)) {
                highEntropyQuickSort(
                    arr.data(), arr.size(),
                    2 * static_cast<int>(std::bit_width(arr.size())), less,
                    false, T{}, false, true, nullptr, false);
                return;
            }
        }

        if (enableLowCardinalityDirect && specialValuePrefixMayMix &&
            lowCardinalityDirectionGate(std::span<const T>(arr.data(), arr.size()), less) &&
            lowCardinalitySampleCandidate(std::span<const T>(arr.data(), arr.size()), less) &&
            trySortLowCardinalityDirectConfirmed(arr, less)) {
            return;
        }

        bool highEntropyPrefixAlternates = false;
        if (enableHighEntropyPartition && specialValuePrefixMayMix && arr.size() >= 8) {
            int previousDirection = 0;
            highEntropyPrefixAlternates = true;
            for (std::size_t i = 1; i < 8; ++i) {
                int direction = 0;
                if (less(arr[i - 1], arr[i])) direction = 1;
                else if (less(arr[i], arr[i - 1])) direction = -1;
                if (direction == 0 ||
                    (previousDirection != 0 && direction == previousDirection)) {
                    highEntropyPrefixAlternates = false;
                    break;
                }
                previousDirection = direction;
            }
        }
        if (enableHighEntropyPartition && specialValuePrefixMayMix &&
            !highEntropyPrefixAlternates &&
            trySortHighEntropyPartitionDirect(arr, less)) {
            return;
        }
    }

    SimulatedInsertionResult<T> sim =
        simulatePatienceInsertionBlueprint(std::span<const T>(arr.data(), arr.size()), less, true, enableNaturalRunRoute,
                                            enableCoherentValuePileCache);

    if (sim.alreadySortedAscending) {
        return;
    }

    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    const bool useRandomBranchlessMerge =
        shouldUseRandomBranchlessMerge(sim, arr.size());

    std::vector<T> tmp;

    std::vector<std::size_t> runStart =
        reconstructTaggedBlueprintNormalizedForSimulated(
            std::span<const T>(arr.data(), arr.size()),
            std::move(sim.blueprint),
            std::move(sim.ascCounts),
            std::move(sim.descCounts),
            tmp,
            false, // ascending-game piles preserve encounter order
            true,  // descending-game piles reverse to become ascending
            true,  // E121 bulk same-tag spans enabled
            true   // E161 decode raw bulk tags on the fly; skip normalization pass
        );

    // E115: physical/simulated/simulated-inplace now share the ends-based adjacent-pair merge driver.
    // It computes its own E045 gallop trigger from the final run lengths.
    // E111: PowerSort was revalidated across physical/simulated/simulated-inplace after convergence and
    // no longer earns its selector complexity. Use adjacent-pair scheduling.
    std::vector<std::size_t> ends(runStart.begin() + 1, runStart.end());
    mergeRunsAdjacentPairsEndsToSpan(
        tmp, arr, ends, less, useRandomBranchlessMerge, true);
}



template <typename T, typename Less = std::less<T>>
void sortImpl(std::span<T> arr, Less less, bool enableNaturalRunRoute) {
    sortImplCore(arr, less, enableNaturalRunRoute, true, true, true, true);
}


// E283: span-compatible structural pre-routes matching simulated-direct E271.
// These are intentionally local to the range engine so the mature phase-map
// implementation can route mapped subranges without copying them into vectors.
template <class T, class Less>
void mergePreparedRunsFromSpanE283(std::span<T> arr,
                                   std::vector<std::size_t>& ends,
                                   Less less) {
    if (ends.size() <= 1) return;
    const std::size_t n = arr.size();
    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) tmp.resize(n);
    else tmp.assign(arr.begin(), arr.end());

    std::vector<std::size_t> next;
    next.reserve((ends.size() + 1) / 2);
    std::size_t left = 0;
    for (std::size_t r = 0; r < ends.size(); r += 2) {
        const std::size_t mid = ends[r];
        if (r + 1 == ends.size()) {
            std::move(arr.begin() + static_cast<std::ptrdiff_t>(left),
                      arr.begin() + static_cast<std::ptrdiff_t>(mid),
                      tmp.begin() + static_cast<std::ptrdiff_t>(left));
            next.push_back(mid);
            left = mid;
            continue;
        }
        const std::size_t right = ends[r + 1];
        std::merge(std::make_move_iterator(arr.begin() + static_cast<std::ptrdiff_t>(left)),
                   std::make_move_iterator(arr.begin() + static_cast<std::ptrdiff_t>(mid)),
                   std::make_move_iterator(arr.begin() + static_cast<std::ptrdiff_t>(mid)),
                   std::make_move_iterator(arr.begin() + static_cast<std::ptrdiff_t>(right)),
                   tmp.begin() + static_cast<std::ptrdiff_t>(left), less);
        next.push_back(right);
        left = right;
    }
    ends.swap(next);
    if (ends.size() <= 1) {
        std::move(tmp.begin(), tmp.end(), arr.begin());
        return;
    }
    mergeRunsAdjacentPairsEndsToSpan(tmp, arr, ends, less, false, true);
}

template <class T, class Less>
bool tryLongAscendingNaturalRunDirectE283(std::span<T> arr, Less less,
                                           std::size_t knownPrefix = 1) {
    constexpr std::size_t kMinN = 10000, kPrefix = 64, kMaxRuns = 128;
    const std::size_t n = arr.size();
    if (n < kMinN) return false;
    for (std::size_t i = std::min(knownPrefix, kPrefix); i < kPrefix; ++i)
        if (less(arr[i], arr[i - 1])) return false;
    bool coarseDrop = false; std::size_t previous = 0;
    for (std::size_t sample = 1; sample < 9; ++sample) {
        const std::size_t index = ((n - 1) * sample) / 8;
        if (less(arr[index], arr[previous])) { coarseDrop = true; break; }
        previous = index;
    }
    if (!coarseDrop) return false;
    struct Run { std::size_t b,e; bool d; };
    std::array<Run,kMaxRuns> runs{}; std::size_t rc=0;
    std::size_t i=kPrefix;
    while(i<n && !less(arr[i],arr[i-1])) ++i;
    runs[rc++]={0,i,false};
    while(i<n){
        if(rc==kMaxRuns) return false;
        const std::size_t b=i; ++i; bool d=false;
        if(i<n){ d=less(arr[i],arr[i-1]); ++i;
            if(d) while(i<n && less(arr[i],arr[i-1])) ++i;
            else while(i<n && !less(arr[i],arr[i-1])) ++i;
        }
        runs[rc++]={b,i,d};
    }
    if(rc<=1) return false;
    std::vector<std::size_t> ends; ends.reserve(rc);
    for(std::size_t r=0;r<rc;++r){
        if(runs[r].d) std::reverse(arr.begin()+static_cast<std::ptrdiff_t>(runs[r].b),
                                   arr.begin()+static_cast<std::ptrdiff_t>(runs[r].e));
        ends.push_back(runs[r].e);
    }
    mergePreparedRunsFromSpanE283(arr, ends, less);
    return true;
}

template <class T, class Less>
bool hasSparseDistributedDisorderE283(std::span<T> arr, Less less,
                                      int knownPrefixInversions=-1) {
    constexpr std::size_t kMinN=10000,kWindow=64,kWindows=8;
    const std::size_t n=arr.size(); if(n<kMinN) return false;
    unsigned prefix=0;
    if(knownPrefixInversions>=0) prefix=(unsigned)knownPrefixInversions;
    else for(std::size_t i=1;i<8;++i) prefix+=(unsigned)less(arr[i],arr[i-1]);
    if(prefix>2) return false;
    unsigned total=0,nz=0;
    for(std::size_t w=0;w<kWindows;++w){
        const std::size_t start=((n-kWindow)*(2*w+1))/(2*kWindows);
        unsigned inv=0;
        for(std::size_t i=start+1;i<start+kWindow;++i) inv+=(unsigned)less(arr[i],arr[i-1]);
        if (inv) ++nz;
        if (inv > 16) return false;
        total += inv;
        if (total > 96) return false;
        const std::size_t rem=kWindows-(w+1); if(nz+rem<6) return false;
    }
    return nz>=6 && total>=6;
}

template <class T, class Less>
bool trySparseDistributedDisorderBlocksE283(std::span<T> arr, Less less,
                                             int knownPrefixInversions=-1) {
    if(!hasSparseDistributedDisorderE283(arr,less,knownPrefixInversions)) return false;
    constexpr std::size_t kBlock=128; const std::size_t n=arr.size();
    std::vector<std::size_t> ends; ends.reserve((n+kBlock-1)/kBlock);
    for(std::size_t b=0;b<n;b+=kBlock){
        const std::size_t e=std::min(n,b+kBlock);
        for(std::size_t i=b+1;i<e;++i){
            T value=std::move(arr[i]); std::size_t j=i;
            while(j>b && less(value,arr[j-1])){arr[j]=std::move(arr[j-1]);--j;}
            arr[j]=std::move(value);
        }
        ends.push_back(e);
    }
    mergePreparedRunsFromSpanE283(arr,ends,less); return true;
}

template <class T, class Less>
bool hasTwoLaneZigzagStructureE283(std::span<T> arr, Less less) {
    constexpr std::size_t kMinN=10000,kPrefixComparisons=8,kChecks=6;
    const std::size_t n=arr.size(); if(n<kMinN) return false;
    auto direction=[&](std::size_t a,std::size_t b){if(less(arr[a],arr[b]))return 1;if(less(arr[b],arr[a]))return -1;return 0;};
    int prev=direction(0,1); if(!prev) return false;
    for(std::size_t i=1;i<kPrefixComparisons;++i){int cur=direction(i,i+1);if(!cur||cur==prev)return false;prev=cur;}
    const int first=direction(0,1);
    for(std::size_t sample=1;sample<=kChecks;++sample){
        std::size_t i=((n-3)*sample)/(kChecks+1); if(i+2>=n)i=n-3;
        const int ex=(i&1u)?-first:first;
        if(direction(i,i+1)!=ex || direction(i+1,i+2)!=-ex) return false;
    }
    auto laneDir=[&](std::size_t f){
        for(std::size_t i=f;i+2<n;i+=2){if(less(arr[i],arr[i+2]))return 1;if(less(arr[i+2],arr[i]))return -1;}return 1;};
    const int ed=laneDir(0),od=laneDir(1);
    auto prove=[&](std::size_t f,int d){for(std::size_t i=f+2;i<n;i+=2){if(d>0){if(less(arr[i],arr[i-2]))return false;}else if(less(arr[i-2],arr[i]))return false;}return true;};
    return prove(0,ed)&&prove(1,od);
}

template <class T, class Less>
bool tryTwoLaneZigzagDirectE283(std::span<T> arr, Less less) {
    if(!hasTwoLaneZigzagStructureE283(arr,less)) return false;
    const std::size_t n=arr.size(), ec=(n+1)/2, oc=n/2;
    auto laneDir=[&](std::size_t f){for(std::size_t i=f+2;i<n;i+=2){if(less(arr[i-2],arr[i]))return 1;if(less(arr[i],arr[i-2]))return -1;}return 1;};
    const int ed=laneDir(0),od=laneDir(1);
    std::vector<T> tmp; if constexpr(std::is_default_constructible_v<T>) tmp.resize(n); else tmp.assign(arr.begin(),arr.end());
    auto mat=[&](std::size_t f,std::size_t c,int d,std::size_t out){for(std::size_t k=0;k<c;++k){const std::size_t q=d>0?k:(c-1-k);tmp[out+k]=std::move(arr[f+2*q]);}};
    mat(0,ec,ed,0); mat(1,oc,od,ec);
    std::merge(std::make_move_iterator(tmp.begin()),std::make_move_iterator(tmp.begin()+static_cast<std::ptrdiff_t>(ec)),
               std::make_move_iterator(tmp.begin()+static_cast<std::ptrdiff_t>(ec)),std::make_move_iterator(tmp.end()),
               arr.begin(),less);
    return true;
}

template <class T, class Less>
bool tryStructuralPreRoutesE271E283(std::span<T> arr, Less less) {
    unsigned inv=0; for(std::size_t i=1;i<8;++i) inv+=(unsigned)less(arr[i],arr[i-1]);
    if(inv==0){if(tryLongAscendingNaturalRunDirectE283(arr,less,8)) return true; if(trySparseDistributedDisorderBlocksE283(arr,less,0)) return true;}
    else if(inv<=2){if(trySparseDistributedDisorderBlocksE283(arr,less,(int)inv)) return true;}
    else if(inv<=4){if(tryTwoLaneZigzagDirectE283(arr,less)) return true;}
    return false;
}

template <typename T, typename Less = std::less<T>>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void sortDirect(std::span<T> arr, Less less = Less{}) {
    if(arr.size()<2) return;
    if(arr.size()<=32){
        for(std::size_t i=1;i<arr.size();++i){T value=std::move(arr[i]);std::size_t j=i;while(j>0&&less(value,arr[j-1])){arr[j]=std::move(arr[j-1]);--j;}arr[j]=std::move(value);}return;
    }
    if(arr.size()>=10000 && tryStructuralPreRoutesE271E283(arr,less)) return;
    sortImpl(arr,less,true);
}

template <typename T, typename Less = std::less<T>>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void sort(std::span<T> arr, Less less = Less{}) {
    if (arr.size() < 2) return;
    if (arr.size() <= 32) {
        for (std::size_t i = 1; i < arr.size(); ++i) {
            T value = std::move(arr[i]);
            std::size_t j = i;
            while (j > 0 && less(value, arr[j - 1])) { arr[j] = std::move(arr[j - 1]); --j; }
            arr[j] = std::move(value);
        }
        return;
    }
    sortImpl(arr, less, true);
}

} // namespace jessesort::simulated_phase_range_engine
#endif
