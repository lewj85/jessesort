#ifndef JESSESORT_ACTUAL_PILES_HPP
#define JESSESORT_ACTUAL_PILES_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <bit>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::actual_piles {

// V1 is the direct physical-pile formulation: values are appended to real
// patience piles during insertion, then those piles are flattened and merged.
// It intentionally remains structurally separate from the simulated family.

template <class T, class Less>
std::size_t findAscendingPile(const std::vector<T>& baseArray,
                              const T& value, Less less) {
    std::size_t lo = 0, hi = baseArray.size();
    // Ascending piles have descending tails. Find first tail <= value.
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (!less(value, baseArray[mid])) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

template <class T, class Less>
std::size_t findDescendingPile(const std::vector<T>& baseArray,
                               const T& value, Less less) {
    std::size_t lo = 0, hi = baseArray.size();
    // Descending piles have ascending tails. Find first tail >= value.
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (!less(baseArray[mid], value)) hi = mid;
        else lo = mid + 1;
    }
    return lo;
}

template <class T, class Less>
std::size_t findAscendingPileBitWalk(const std::vector<T>& tails, const T& value, Less less) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;
    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(value, tails[next])) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <class T, class Less>
std::size_t findDescendingPileBitWalk(const std::vector<T>& tails, const T& value, Less less) {
    const std::size_t n = tails.size();
    if (n == 0) return 0;
    std::ptrdiff_t idx = -1;
    std::size_t step = std::size_t{1} << (std::bit_width(n) - 1);
    for (; step != 0; step >>= 1) {
        const std::size_t next = static_cast<std::size_t>(idx + static_cast<std::ptrdiff_t>(step));
        if (next < n && less(tails[next], value)) idx = static_cast<std::ptrdiff_t>(next);
    }
    return static_cast<std::size_t>(idx + 1);
}

template <class T, class Less>
void mergeRuns(std::vector<T>& src, std::vector<T>& dst,
               std::vector<std::size_t>& ends, Less less, bool branchlessRandomMerge) {
    if (ends.size() <= 1) return;
    unsigned gallopTrigger = 7;
    if (!branchlessRandomMerge) {
        const std::size_t runCount = ends.size();
        std::size_t largestRun = 0, prevEnd = 0;
        for (std::size_t end : ends) {
            largestRun = std::max(largestRun, end - prevEnd);
            prevEnd = end;
        }
        // V2 E045 selector, tested independently here for V1 physical piles.
        if (runCount >= 12 && runCount <= 128 &&
            static_cast<unsigned long long>(largestRun) * runCount * 4 <=
                static_cast<unsigned long long>(ends.back()) * 5) {
            gallopTrigger = 5;
        }
    }
    bool sourceIsSrc = true;
    std::vector<std::size_t> next;
    next.reserve((ends.size() + 1) / 2);

    while (ends.size() > 1) {
        auto& in = sourceIsSrc ? src : dst;
        auto& out = sourceIsSrc ? dst : src;

        // Elide boundaries between adjacent runs that are already globally
        // ordered. This is a metadata-only merge and avoids copying those
        // runs through the alternate buffer solely to remove the boundary.
        next.clear();
        for (std::size_t r = 0; r + 1 < ends.size(); ++r) {
            const std::size_t boundary = ends[r];
            if (less(in[boundary], in[boundary - 1]))
                next.push_back(boundary);
        }
        next.push_back(ends.back());
        ends.swap(next);
        if (ends.size() <= 1) break;

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
                T* lp = in.data() + left; T* const le = in.data() + mid;
                T* rp = in.data() + mid; T* const re = in.data() + right;
                T* op = out.data() + left;
                auto takeOne = [&] { const bool tr = less(*rp,*lp); *op++ = std::move(tr ? *rp : *lp); rp += (std::ptrdiff_t)tr; lp += (std::ptrdiff_t)!tr; };
                while (lp + 4 <= le && rp + 4 <= re) { takeOne(); takeOne(); takeOne(); takeOne(); }
                while (lp < le && rp < re) takeOne();
                i = (std::size_t)(lp - in.data()); j = (std::size_t)(rp - in.data()); k = (std::size_t)(op - out.data());
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

// Public V1 entry point: physical patience piles followed by run merging.
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::actual_piles::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::actual_piles::sort requires movable values for pile flattening and merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    const std::size_t n = arr.size();
    if (n < 2) return;

    constexpr std::size_t MinPrefixPileLength = 32;
    enum class PrefixDirection { Unknown, Ascending, Descending };

    PrefixDirection prefixDirection = PrefixDirection::Unknown;
    std::size_t prefixEnd = 1;
    for (; prefixEnd < n; ++prefixEnd) {
        const T& previous = arr[prefixEnd - 1];
        const T& value = arr[prefixEnd];
        if (less(previous, value)) {
            if (prefixDirection == PrefixDirection::Descending) break;
            prefixDirection = PrefixDirection::Ascending;
        } else if (less(value, previous)) {
            if (prefixDirection == PrefixDirection::Ascending) break;
            prefixDirection = PrefixDirection::Descending;
        }
    }

    if (prefixEnd == n) {
        if (prefixDirection == PrefixDirection::Descending) {
            std::reverse(arr.begin(), arr.end());
        }
        return;
    }

    std::vector<std::vector<T>> ascPiles;
    std::vector<std::vector<T>> descPiles;

    // Compact copies of the current pile tails. Binary search operates on
    // these contiguous arrays instead of repeatedly dereferencing
    // piles[mid].back(). Each entry always mirrors its pile's last value.
    std::vector<T> ascBaseArray;
    std::vector<T> descBaseArray;

    constexpr std::size_t reservePiles = 32;
    ascPiles.reserve(reservePiles);
    descPiles.reserve(reservePiles);
    ascBaseArray.reserve(reservePiles);
    descBaseArray.reserve(reservePiles);

    bool descendingMode = false;
    std::size_t processStart = 0;
    const T* previousValue = nullptr;
    std::size_t lastAscPile = static_cast<std::size_t>(-1);
    std::size_t lastDescPile = static_cast<std::size_t>(-1);
    std::size_t probeHintAttempts = 0;
    std::size_t probeHintHits = 0;

    if (prefixEnd >= MinPrefixPileLength &&
        prefixDirection != PrefixDirection::Unknown) {
        processStart = prefixEnd;
        descendingMode = prefixDirection == PrefixDirection::Descending;

        auto& piles = descendingMode ? descPiles : ascPiles;
        auto& baseArray = descendingMode ? descBaseArray : ascBaseArray;
        piles.emplace_back();
        piles.back().reserve(prefixEnd);
        for (std::size_t i = 0; i < prefixEnd; ++i) {
            piles.back().push_back(std::move(arr[i]));
        }
        baseArray.push_back(piles.back().back());
        previousValue = &piles.back().back();
        if (descendingMode) lastDescPile = 0; else lastAscPile = 0;
    }

    auto insertNormal = [&](T& value, bool useDescendingGame) -> const T* {
        auto& piles = useDescendingGame ? descPiles : ascPiles;
        auto& baseArray = useDescendingGame ? descBaseArray : ascBaseArray;
        const std::size_t pileIndex = useDescendingGame
            ? findDescendingPile(baseArray, value, less)
            : findAscendingPile(baseArray, value, less);
        if (pileIndex == piles.size()) {
            baseArray.push_back(value);
            piles.emplace_back();
        } else {
            baseArray[pileIndex] = value;
        }
        piles[pileIndex].push_back(std::move(value));
        return &piles[pileIndex].back();
    };

    auto insertBitWalk = [&](T& value, bool useDescendingGame) -> const T* {
        auto& piles = useDescendingGame ? descPiles : ascPiles;
        auto& baseArray = useDescendingGame ? descBaseArray : ascBaseArray;
        const std::size_t pileIndex = useDescendingGame
            ? findDescendingPileBitWalk(baseArray, value, less)
            : findAscendingPileBitWalk(baseArray, value, less);
        if (pileIndex == piles.size()) {
            baseArray.push_back(value);
            piles.emplace_back();
        } else {
            baseArray[pileIndex] = value;
        }
        piles[pileIndex].push_back(std::move(value));
        return &piles[pileIndex].back();
    };

    if (processStart == 0) {
        previousValue = insertNormal(arr[0], false);
        lastAscPile = 0;
        processStart = 1;
    }

    auto routeGame = [&](T& value) {
        if (less(*previousValue, value)) descendingMode = false;
        else if (less(value, *previousValue)) descendingMode = true;
    };

    // True early probe: only the first 64 source elements may contribute.
    // Long monotone prefixes that already bypass ordinary insertion are kept
    // on the normal continuation path rather than classified retrospectively.
    if (processStart < 64) {
        const std::size_t probeEnd = std::min<std::size_t>(n, 64);
        for (std::size_t i = processStart; i < probeEnd; ++i) {
            T& value = arr[i];
            routeGame(value);
            auto& piles = descendingMode ? descPiles : ascPiles;
            auto& baseArray = descendingMode ? descBaseArray : ascBaseArray;
            std::size_t& lastPile = descendingMode ? lastDescPile : lastAscPile;
            const std::size_t pileIndex = descendingMode
                ? findDescendingPile(baseArray, value, less)
                : findAscendingPile(baseArray, value, less);
            if (lastPile != static_cast<std::size_t>(-1)) {
                ++probeHintAttempts;
                if (pileIndex == lastPile) ++probeHintHits;
            }
            if (pileIndex == piles.size()) {
                baseArray.push_back(value);
                piles.emplace_back();
            } else {
                baseArray[pileIndex] = value;
            }
            piles[pileIndex].push_back(std::move(value));
            lastPile = pileIndex;
            previousValue = &piles[pileIndex].back();
        }
        processStart = probeEnd;
    }

    const bool earlyRandomLike = processStart == 64 &&
        ascPiles.size() >= 6 && descPiles.size() >= 6;

    // The same early probe now routes both sides of the locality spectrum.
    // Random-like layouts use direct bit-walk search (E046). Semi-structured
    // layouts with a high exact previous-pile hit rate use a cheap per-game
    // hint first, but only once the probe has enough piles that the hint can
    // beat a tiny direct search.
    const bool highLocality = processStart == 64 && !earlyRandomLike &&
        probeHintAttempts >= 16 && ascPiles.size() + descPiles.size() >= 4 &&
        probeHintHits * 4 >= probeHintAttempts * 3;

    if (earlyRandomLike) {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            routeGame(value);
            previousValue = insertBitWalk(value, descendingMode);
        }
    } else if (highLocality) {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            routeGame(value);
            auto& piles = descendingMode ? descPiles : ascPiles;
            auto& baseArray = descendingMode ? descBaseArray : ascBaseArray;
            std::size_t& hint = descendingMode ? lastDescPile : lastAscPile;
            std::size_t pileIndex;
            const bool hintValid = hint < baseArray.size() && (descendingMode
                ? (!less(baseArray[hint], value) &&
                   (hint == 0 || less(baseArray[hint - 1], value)))
                : (!less(value, baseArray[hint]) &&
                   (hint == 0 || less(value, baseArray[hint - 1]))));
            if (hintValid) {
                pileIndex = hint;
            } else {
                pileIndex = descendingMode
                    ? findDescendingPile(baseArray, value, less)
                    : findAscendingPile(baseArray, value, less);
            }
            if (pileIndex == piles.size()) {
                baseArray.push_back(value);
                piles.emplace_back();
            } else {
                baseArray[pileIndex] = value;
            }
            piles[pileIndex].push_back(std::move(value));
            hint = pileIndex;
            previousValue = &piles[pileIndex].back();
        }
    } else {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            routeGame(value);
            previousValue = insertNormal(value, descendingMode);
        }
    }

    // Random-like routing without a separate prefix probe. Full-range random
    // inputs produce a large, balanced population of piles across both games.
    // Low-cardinality random inputs are balanced but fail the density gate,
    // while directional/semi-sorted inputs can be dense but fail the balance
    // gate. This preserves the insertion hot loop unchanged.
    const std::size_t ascCount = ascPiles.size();
    const std::size_t descCount = descPiles.size();
    const std::size_t finalPileCount = ascCount + descCount;
    const std::size_t minorityPileCount = std::min(ascCount, descCount);
    __extension__ typedef unsigned __int128 Wide;
    const bool useRandomBranchlessMerge = n >= 10000 &&
        static_cast<Wide>(finalPileCount) * finalPileCount >= static_cast<Wide>(5) * n &&
        minorityPileCount * 4 >= finalPileCount &&
        std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*);

    std::vector<T> flat;
    flat.reserve(n);
    std::vector<std::size_t> ends;
    ends.reserve(ascPiles.size() + descPiles.size());
    for (auto& pile : descPiles) {
        for (auto it = pile.rbegin(); it != pile.rend(); ++it)
            flat.push_back(std::move(*it));
        ends.push_back(flat.size());
    }
    for (auto& pile : ascPiles) {
        for (auto& value : pile) flat.push_back(std::move(value));
        ends.push_back(flat.size());
    }

    std::vector<T> buffer;
    if constexpr (std::is_default_constructible_v<T>) {
        buffer.resize(n);
    } else {
        // Generic fallback: construct valid destination objects without
        // imposing a default-constructor requirement. This branch is not
        // instantiated for the ordinary numeric fast path.
        buffer = flat;
    }
    mergeRuns(flat, buffer, ends, less, useRandomBranchlessMerge);
    arr = std::move(flat);
}

} // namespace jessesort::actual_piles
#endif
