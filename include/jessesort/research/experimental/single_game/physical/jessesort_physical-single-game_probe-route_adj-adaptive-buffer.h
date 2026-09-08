#ifndef JESSESORT_EXPERIMENTAL_PHYSICAL_SINGLE_GAME_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_EXPERIMENTAL_PHYSICAL_SINGLE_GAME_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/detail/common/tiny_sort.h>
#include <jessesort/detail/pipelines/reference/simulated.h>
#include <jessesort/detail/pipelines/reference/simulated_direct.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <bit>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::physical_single_game {

// Experimental single-game physical Patience baseline. This is intentionally a
// minimal semantic refactor of the maintained physical owner: retain its mature
// pre-routing, insertion-search optimizations, flattening, and merge backend, but
// route every Patience insertion into the ascending game. No descending game,
// reverse traversal, or front insertion is allowed in this baseline.

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

// Public physical entry point: physical patience piles followed by run merging.
template <class T, class Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less, bool bidirectionalBranchlessMerge,
              bool naturalRunRoute = false, bool adjacentEqualFastPath = true,
              bool enableSpecializedRoutes = false, bool enableCoherentValuePileCache = false) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::physical_single_game::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::physical_single_game::sort requires movable values for pile flattening and merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    const std::size_t n = arr.size();
    if (n < 2) return;

    constexpr std::size_t MinPrefixPileLength = 32;
    enum class PrefixDirection { Unknown, Ascending, Descending };

    PrefixDirection prefixDirection = PrefixDirection::Unknown;
    std::size_t prefixEnd = 1;

    // E145: establish the monotone direction once, then test only the edge
    // that can invalidate it. Equivalent edges do not end the prefix.
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
        if (prefixDirection == PrefixDirection::Descending) {
            std::reverse(arr.begin(), arr.end());
        }
        return;
    }

    // E264: intentionally inline mirror of the owning simulated specialized router.
    // A shared noinline call regressed structured canonical inputs by >1%; keep this
    // source synchronized with the owner and audit drift explicitly.
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        if (enableSpecializedRoutes) {
            bool specialValuePrefixMayMix = true;
            if (n >= 4) {
                const bool firstThreeAscending =
                    less(arr[0], arr[1]) && less(arr[1], arr[2]) && less(arr[2], arr[3]);
                const bool firstThreeDescending =
                    less(arr[1], arr[0]) && less(arr[2], arr[1]) && less(arr[3], arr[2]);
                specialValuePrefixMayMix = !(firstThreeAscending || firstThreeDescending);
            }
            if (specialValuePrefixMayMix) {
                T dominant = arr[0];
                if (jessesort::simulated_legacy::dominantValueSampleCandidate(arr, dominant, less)) {
                    jessesort::simulated_legacy::highEntropyQuickSort(
                        arr.data(), arr.size(),
                        2 * static_cast<int>(std::bit_width(arr.size())), less,
                        false, T{}, false, true, nullptr, false);
                    return;
                }

                if (jessesort::simulated_legacy::lowCardinalityDirectionGate(arr, less) &&
                    jessesort::simulated_legacy::lowCardinalitySampleCandidate(arr, less) &&
                    jessesort::simulated_legacy::trySortLowCardinalityDirectConfirmed(arr, less)) {
                    return;
                }

                bool highEntropyPrefixAlternates = false;
                if (n >= 8) {
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
                    jessesort::simulated_legacy::trySortHighEntropyPartitionDirect(arr, less)) {
                    return;
                }
            }
        }
    }

    // E143: physical-pile propagation of E142.  Classify before any source
    // values are moved into physical piles; the continuation cache itself is
    // maintained coherently by invalidating entries when baseArray tails change.
    const bool coherentValuePileCacheCandidate = enableCoherentValuePileCache &&
        jessesort::simulated_legacy::coherentValuePileCacheSampleCandidate(arr, less, n >= 50000);

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
    std::size_t probeAdjacentEquivalent = 0;

    // E181: cold confirmation for short same-direction prefix handoff.
    const bool confirmedShortSameDirectionPrefix =
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd < MinPrefixPileLength &&
        jessesort::simulated_legacy::confirmedShortSameDirectionPrefix(
            arr, prefixEnd, prefixDirection == PrefixDirection::Descending, less,
            MinPrefixPileLength);
    const bool materializePrefix =
        prefixDirection == PrefixDirection::Ascending &&
        (prefixEnd >= MinPrefixPileLength || confirmedShortSameDirectionPrefix);

    if (materializePrefix) {
        processStart = prefixEnd;
        descendingMode = false;

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
        const bool initialDescendingCandidate =
            prefixDirection == PrefixDirection::Descending;
        previousValue = insertNormal(arr[0], false);
        descendingMode = false;
        (void)initialDescendingCandidate;
        lastAscPile = 0;
        processStart = 1;
    }

    // E145 retained: after a materialized natural prefix,
    // override only the first post-prefix game decision.  Do not preinsert the
    // value: the natural-run batcher must retain ownership of pile creation and
    // reservation so long suffix runs stay allocation-efficient.
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

    auto routeGame = [&](T& value) -> bool {
        if (postPrefixDirectionOverride) {
            descendingMode = false;
            postPrefixDirectionOverride = false;
            return false;
        }
        if (less(*previousValue, value)) { descendingMode = false; return false; }
        if (less(value, *previousValue)) { descendingMode = false; return false; }
        return true;
    };

    bool naturalHandled = false;
    bool earlyRandomLike = false;
    const bool naturalRunCandidate = false;
    (void)naturalRunRoute;
    if (naturalRunCandidate && processStart < n) {
        std::size_t i = processStart;
        auto insertOneNatural = [&](std::size_t j) {
            T& value = arr[j];
            routeGame(value);
            previousValue = insertNormal(value, descendingMode);
            if (descendingMode) lastDescPile = descPiles.size() ? findDescendingPile(descBaseArray, *previousValue, less) : 0;
            else lastAscPile = ascPiles.size() ? findAscendingPile(ascBaseArray, *previousValue, less) : 0;
        };
        while (i < n) {
            bool asc;
            bool desc;
            if (postPrefixDirectionOverride) {
                asc = !postPrefixOverrideDescending;
                desc = postPrefixOverrideDescending;
            } else {
                asc = less(*previousValue, arr[i]);
                desc = !asc && less(arr[i], *previousValue);
            }
            if (!asc && !desc) { insertOneNatural(i++); continue; }
            std::size_t end = i + 1;
            if (asc) while (end < n && less(arr[end - 1], arr[end])) ++end;
            else while (end < n && less(arr[end], arr[end - 1])) ++end;
            const std::size_t len = end - i;
            bool batched = false;
            if (len >= 8) {
                auto& piles = asc ? ascPiles : descPiles;
                auto& base = asc ? ascBaseArray : descBaseArray;
                const std::size_t firstPile = asc
                    ? findAscendingPile(base, arr[i], less)
                    : findDescendingPile(base, arr[i], less);
                const std::size_t lastPile = asc
                    ? findAscendingPile(base, arr[end - 1], less)
                    : findDescendingPile(base, arr[end - 1], less);
                if (firstPile == lastPile) {
                    if (firstPile == piles.size()) {
                        base.push_back(arr[end - 1]);
                        piles.emplace_back();
                        piles.back().reserve(len);
                    } else {
                        base[firstPile] = arr[end - 1];
                    }
                    auto& pile = piles[firstPile];
                    for (std::size_t j = i; j < end; ++j)
                        pile.push_back(std::move(arr[j]));
                    previousValue = &pile.back();
                    descendingMode = false;
                    if (asc) lastAscPile = firstPile; else lastDescPile = firstPile;
                    if (postPrefixDirectionOverride) {
                        descendingMode = false;
                        postPrefixDirectionOverride = false;
                    }
                    batched = true;
                }
            }
            if (!batched) for (std::size_t j = i; j < end; ++j) insertOneNatural(j);
            i = end;
        }
        naturalHandled = true;
    }

    if (!naturalHandled) {
    // True early probe: only the first 64 source elements may contribute.
    // Long monotone prefixes that already bypass ordinary insertion are kept
    // on the normal continuation path rather than classified retrospectively.
    if (processStart < 64) {
        const std::size_t probeEnd = std::min<std::size_t>(n, 64);
        for (std::size_t i = processStart; i < probeEnd; ++i) {
            T& value = arr[i];
            if (routeGame(value)) ++probeAdjacentEquivalent;
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

    earlyRandomLike = processStart == 64 &&
        ascPiles.size() >= 6 && descPiles.size() >= 6;

    // The same early probe now routes both sides of the locality spectrum.
    // Random-like layouts use direct bit-walk search (E046). Semi-structured
    // layouts with a high exact previous-pile hit rate use a cheap per-game
    // hint first, but only once the probe has enough piles that the hint can
    // beat a tiny direct search.
    const bool highLocality = processStart == 64 && !earlyRandomLike &&
        probeHintAttempts >= 16 && ascPiles.size() + descPiles.size() >= 4 &&
        probeHintHits * 4 >= probeHintAttempts * 3;

    // E118: physical piles pay extra state tracking only when the 64-value probe
    // actually observed adjacent equivalence. This preserves unique-random throughput.
    const bool useAdjacentEqualFastPath = adjacentEqualFastPath &&
        processStart == 64 && probeAdjacentEquivalent >= 2;

    auto appendAdjacentEquivalent = [&](T& value) -> const T* {
        auto& piles = descendingMode ? descPiles : ascPiles;
        const std::size_t pileIndex = descendingMode ? lastDescPile : lastAscPile;
        assert(pileIndex < piles.size());
        piles[pileIndex].push_back(std::move(value));
        return &piles[pileIndex].back();
    };

    auto insertNormalTracked = [&](T& value, bool useDescendingGame) -> const T* {
        auto& piles = useDescendingGame ? descPiles : ascPiles;
        auto& baseArray = useDescendingGame ? descBaseArray : ascBaseArray;
        std::size_t& lastPile = useDescendingGame ? lastDescPile : lastAscPile;
        const std::size_t pileIndex = useDescendingGame
            ? findDescendingPile(baseArray, value, less)
            : findAscendingPile(baseArray, value, less);
        if (pileIndex == piles.size()) { baseArray.push_back(value); piles.emplace_back(); }
        else baseArray[pileIndex] = value;
        piles[pileIndex].push_back(std::move(value));
        lastPile = pileIndex;
        return &piles[pileIndex].back();
    };

    auto insertBitWalkTracked = [&](T& value, bool useDescendingGame) -> const T* {
        auto& piles = useDescendingGame ? descPiles : ascPiles;
        auto& baseArray = useDescendingGame ? descBaseArray : ascBaseArray;
        std::size_t& lastPile = useDescendingGame ? lastDescPile : lastAscPile;
        const std::size_t pileIndex = useDescendingGame
            ? findDescendingPileBitWalk(baseArray, value, less)
            : findAscendingPileBitWalk(baseArray, value, less);
        if (pileIndex == piles.size()) { baseArray.push_back(value); piles.emplace_back(); }
        else baseArray[pileIndex] = value;
        piles[pileIndex].push_back(std::move(value));
        lastPile = pileIndex;
        return &piles[pileIndex].back();
    };

    const std::size_t e183ProbePiles = ascPiles.size() + descPiles.size();
    const bool e183BoundaryCandidate = processStart == 64 && !earlyRandomLike &&
        e183ProbePiles >= 3 && e183ProbePiles <= 12;

    if (e183BoundaryCandidate) {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            bool equivalent = false;
            if (previousValue != nullptr) {
                if (less(*previousValue, value)) descendingMode = false;
                else if (less(value, *previousValue)) descendingMode = false;
                else equivalent = true;
            }
            if (descendingMode && i + 1 < n && less(value, arr[i + 1])) {
                const bool wouldCreateDesc = descBaseArray.empty() || less(descBaseArray.back(), value);
                const bool canContinueAsc = !ascBaseArray.empty() && !less(value, ascBaseArray.back());
                if (wouldCreateDesc && canContinueAsc) descendingMode = false;
            }
            if (useAdjacentEqualFastPath && equivalent)
                previousValue = appendAdjacentEquivalent(value);
            else
                previousValue = insertNormalTracked(value, descendingMode);
        }
    } else if (coherentValuePileCacheCandidate) {
        if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
            struct CacheEntry {
                std::uint64_t key = 0;
                std::uint32_t pile = 0;
                bool valid = false;
            };
            std::array<CacheEntry, 128> ascCache{}, descCache{};
            auto keyOf = [](const T& value) -> std::uint64_t {
                return static_cast<std::uint64_t>(
                    static_cast<std::make_unsigned_t<T>>(value));
            };
            auto bucket = [&](const T& value) -> std::size_t {
                return static_cast<std::size_t>(
                    (keyOf(value) * 11400714819323198485ULL) >> 57);
            };
            auto seed = [&](const std::vector<T>& base, auto& cache) {
                for (std::size_t p = 0; p < base.size(); ++p) {
                    const std::size_t b = bucket(base[p]);
                    cache[b] = CacheEntry{keyOf(base[p]),
                        static_cast<std::uint32_t>(p), true};
                }
            };
            seed(ascBaseArray, ascCache);
            seed(descBaseArray, descCache);

            for (std::size_t i = processStart; i < n; ++i) {
                T& value = arr[i];
                const bool equivalent = routeGame(value);
                if (useAdjacentEqualFastPath && equivalent) {
                    previousValue = appendAdjacentEquivalent(value);
                    continue;
                }

                auto& piles = descendingMode ? descPiles : ascPiles;
                auto& baseArray = descendingMode ? descBaseArray : ascBaseArray;
                auto& cache = descendingMode ? descCache : ascCache;
                std::size_t& lastPile = descendingMode ? lastDescPile : lastAscPile;
                const std::uint64_t key = keyOf(value);
                const std::size_t b = bucket(value);
                auto& entry = cache[b];
                std::size_t pileIndex;
                if (entry.valid && entry.key == key && entry.pile < piles.size()) {
                    pileIndex = entry.pile;
                    piles[pileIndex].push_back(std::move(value));
                } else {
                    pileIndex = descendingMode
                        ? findDescendingPile(baseArray, value, less)
                        : findAscendingPile(baseArray, value, less);
                    if (pileIndex == piles.size()) {
                        baseArray.push_back(value);
                        piles.emplace_back();
                    } else {
                        const T old = baseArray[pileIndex];
                        const std::size_t oldBucket = bucket(old);
                        auto& oldEntry = cache[oldBucket];
                        if (oldEntry.valid && oldEntry.key == keyOf(old) &&
                            oldEntry.pile == pileIndex) {
                            oldEntry.valid = false;
                        }
                        baseArray[pileIndex] = value;
                    }
                    piles[pileIndex].push_back(std::move(value));
                    cache[b] = CacheEntry{key, static_cast<std::uint32_t>(pileIndex), true};
                }
                lastPile = pileIndex;
                previousValue = &piles[pileIndex].back();
            }
        }
    } else if (earlyRandomLike) {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            const bool equivalent = routeGame(value);
            if (useAdjacentEqualFastPath && equivalent)
                previousValue = appendAdjacentEquivalent(value);
            else
                previousValue = useAdjacentEqualFastPath
                    ? insertBitWalkTracked(value, descendingMode)
                    : insertBitWalk(value, descendingMode);
        }
    } else if (highLocality) {
        for (std::size_t i = processStart; i < n; ++i) {
            T& value = arr[i];
            const bool equivalent = routeGame(value);
            if (useAdjacentEqualFastPath && equivalent) {
                previousValue = appendAdjacentEquivalent(value);
                continue;
            }
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
            const bool equivalent = routeGame(value);
            if (useAdjacentEqualFastPath && equivalent)
                previousValue = appendAdjacentEquivalent(value);
            else
                previousValue = useAdjacentEqualFastPath
                    ? insertNormalTracked(value, descendingMode)
                    : insertNormal(value, descendingMode);
        }
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
    // E109: use the same earlyRandomLike probe prerequisite as simulated/simulated-inplace.
    __extension__ typedef unsigned __int128 Wide;
    const Wide finalPileSquare =
        static_cast<Wide>(finalPileCount) * finalPileCount;
    // E109 convergence baseline: physical/simulated/simulated-inplace share the same non-freezing
    // branchless/general density thresholds: 3.5 / 4.25 / 5.0.  physical also
    // uses the same earlyRandomLike prerequisite as simulated/simulated-inplace.
    bool densitySelectsBranchless = false;
    if (n <= 20000) {
        densitySelectsBranchless =
            static_cast<Wide>(2) * finalPileSquare >=
            static_cast<Wide>(7) * n;
    } else if (n < 500000) {
        densitySelectsBranchless =
            static_cast<Wide>(4) * finalPileSquare >=
            static_cast<Wide>(17) * n;
    } else {
        densitySelectsBranchless =
            finalPileSquare >= static_cast<Wide>(5) * n;
    }
    const bool useRandomBranchlessMerge = n >= 10000 &&
        earlyRandomLike &&
        densitySelectsBranchless &&
        std::is_trivially_copyable_v<T> &&
        sizeof(T) <= 96;

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
    // E115: physical/simulated/simulated-inplace share one post-flatten adj-pair merge driver.
    // physical naturally stores run ends, so no run-boundary conversion is needed.
    // E379: for compact, strongly size-imbalanced physical run sets, use the
    // retained sliding stride-3 best-2-of-3 tree. The gate is derived from
    // physical-owner geometry and is intentionally independent of input names.
    bool useStride3 = false;
    if (ends.size() >= 8 && ends.size() <= 32) {
        std::vector<std::size_t> gateEnds; gateEnds.reserve(ends.size());
        for (std::size_t r = 0; r < ends.size(); ++r) {
            const std::size_t b = ends[r];
            if (r + 1 == ends.size() || less(flat[b], flat[b - 1])) gateEnds.push_back(b);
        }
        const std::size_t rc = gateEnds.size();
        if (rc >= 8 && rc <= 32) {
            std::size_t prev = 0, largest = 0;
            for (std::size_t e : gateEnds) { largest = std::max(largest, e - prev); prev = e; }
            useStride3 = static_cast<unsigned long long>(largest) * rc * 10ULL >
                         static_cast<unsigned long long>(n) * 21ULL;
        }
    }
    if (useStride3) {
        std::vector<std::size_t> starts; starts.reserve(ends.size() + 1);
        starts.push_back(0);
        starts.insert(starts.end(), ends.begin(), ends.end());
        jessesort::simulated_direct_merge::detail::mergeRunsE375Tree(
            flat, buffer, std::move(starts), less,
            jessesort::simulated_direct_merge::detail::E375Schedule::sliding_stride3);
        arr = std::move(buffer);
    } else {
        jessesort::simulated_legacy::mergeRunsAdjacentPairsEnds(
            flat, buffer, ends, less, useRandomBranchlessMerge, bidirectionalBranchlessMerge);
        arr = std::move(flat);
    }
}

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    sortImpl(arr, less, true, true, true, true, true);
}

} // namespace jessesort::physical_single_game
#endif
