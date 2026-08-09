#ifndef JESSESORT_SIMULATED_EARLY_FREEZE_HPP
#define JESSESORT_SIMULATED_EARLY_FREEZE_HPP

#include "v2_simulated.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_early_freeze {

// V5 freezes the simulated pile structure early, collects later values into
// small unsorted overflow bands, then sorts those bands during reconstruction.
// The shared early-freeze insertion machinery is also reused by V4 and V6.

// Shared blueprint encoding and merge logic are maintained in v2_simulated.h.
// This header keeps only the compact early-freeze pile search plus the
// freeze/overflow-specific pipeline.
using simulated::OVERFLOW_TAG;
using simulated::isAscTag;
using simulated::isDescTag;
using simulated::localPileId;

template <bool UseHint = true, typename T, typename Less = std::less<T>>
inline int findDescendingPileWithTails(
    const std::vector<T>& tails,
    int& hint,
    const T& value,
    Less less = Less{}
) {
    const int n = static_cast<int>(tails.size());
    assert(n > 0 && hint >= 0 && hint < n);

    if constexpr (UseHint) {
        if (!less(tails[hint], value) &&
            (hint == 0 || less(tails[hint - 1], value))) {
            return hint;
        }
    }

    int idx = -1;
    const unsigned un = static_cast<unsigned>(n);
    int step = 1 << (31 - __builtin_clz(un));
    for (; step != 0; step >>= 1) {
        const int next = idx + step;
        if (next < n && less(tails[static_cast<std::size_t>(next)], value)) {
            idx = next;
        }
    }
    hint = idx + 1;
    return hint;
}

template <bool UseHint = true, typename T, typename Less = std::less<T>>
inline int findAscendingPileWithTails(
    const std::vector<T>& tails,
    int& hint,
    const T& value,
    Less less = Less{}
) {
    const int n = static_cast<int>(tails.size());
    assert(n > 0 && hint >= 0 && hint < n);

    if constexpr (UseHint) {
        if (!less(value, tails[hint]) &&
            (hint == 0 || less(value, tails[hint - 1]))) {
            return hint;
        }
    }

    int idx = -1;
    const unsigned un = static_cast<unsigned>(n);
    int step = 1 << (31 - __builtin_clz(un));
    for (; step != 0; step >>= 1) {
        const int next = idx + step;
        if (next < n && less(value, tails[static_cast<std::size_t>(next)])) {
            idx = next;
        }
    }
    hint = idx + 1;
    return hint;
}

template <bool UseHint = true, typename T, typename Less = std::less<T>>
inline void simulateInsertValueDescendingPiles(
    std::vector<T>& tails,
    int& lastPileIndex,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& descCounts,
    Less less = Less{}
) {
    int pileIndex = 0;
    if (!descCounts.empty()) {
        pileIndex = findDescendingPileWithTails<UseHint>(
            tails, lastPileIndex, value, less);
    }

    const int pileCount = static_cast<int>(tails.size());
    if (pileIndex < pileCount) {
        tails[static_cast<std::size_t>(pileIndex)] = value;
        ++descCounts[static_cast<std::size_t>(pileIndex)];
    } else {
        assert(pileIndex >= 0 && static_cast<uint32_t>(pileIndex) <= simulated::PILE_MASK);
        tails.push_back(value);
        descCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = simulated::makeDescTag(static_cast<uint32_t>(pileIndex));
}

template <bool UseHint = true, typename T, typename Less = std::less<T>>
inline void simulateInsertValueAscendingPiles(
    std::vector<T>& tails,
    int& lastPileIndex,
    const T& value,
    std::size_t originalIndex,
    std::vector<uint32_t>& blueprint,
    std::vector<std::size_t>& ascCounts,
    Less less = Less{}
) {
    int pileIndex = 0;
    if (!ascCounts.empty()) {
        pileIndex = findAscendingPileWithTails<UseHint>(
            tails, lastPileIndex, value, less);
    }

    const int pileCount = static_cast<int>(tails.size());
    if (pileIndex < pileCount) {
        tails[static_cast<std::size_t>(pileIndex)] = value;
        ++ascCounts[static_cast<std::size_t>(pileIndex)];
    } else {
        assert(pileIndex >= 0 && static_cast<uint32_t>(pileIndex) <= simulated::PILE_MASK);
        tails.push_back(value);
        ascCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = simulated::makeAscTag(static_cast<uint32_t>(pileIndex));
}


enum class FreezePolicy {
    fixed_half,
    power2_broad,
    power2_single_overflow
};

inline std::size_t nextPowerOfTwoStrict(std::size_t x) {
    std::size_t p = 1;
    while (p <= x && p <= (std::numeric_limits<std::size_t>::max() >> 1)) p <<= 1;
    return p;
}

inline bool shouldExtendToPowerOfTwo(
    FreezePolicy policy,
    std::size_t n,
    std::size_t pileCount
) {
    if (policy == FreezePolicy::fixed_half) return false;

    if (policy == FreezePolicy::power2_single_overflow) {
        // V4 gains overwhelmingly from low-pile structured inputs. Avoid the
        // two/three-pile cases, which are commonly already cheap alternating
        // or rotated layouts and showed no reliable benefit from extension.
        return pileCount == 1 || (pileCount >= 4 && pileCount <= 32);
    }

    // V5/V6: always extend very small run sets. For larger inputs, extend when
    // pile growth is still in the usual O(sqrt(n)) regime or when the next
    // power-of-two cap is already close to the current pile count.
    if (pileCount <= 32) return true;
    if (n < 32768) return false;

    const std::size_t cap = nextPowerOfTwoStrict(pileCount);
    const long double rootLimit =
        4.0L * std::sqrt(static_cast<long double>(n));
    if (static_cast<long double>(pileCount) <= rootLimit) return true;

    __extension__ typedef unsigned __int128 Wide;
    return static_cast<Wide>(cap) * 4 <=
           static_cast<Wide>(pileCount) * 5;
}

inline std::size_t calculateEarlyFreezePoint(std::size_t n) {
    if (n == 0) return 0;

    // The half-input point is the default freeze point and the decision point for
    // optional selected power-of-two extension policies.
    const long double sampled = static_cast<long double>(n) * 0.5L;
    const std::size_t freezePoint =
        static_cast<std::size_t>(std::llround(sampled));
    return std::clamp<std::size_t>(freezePoint, 1, n);
}

template <typename T>
struct FrozenInsertionResult {
    std::vector<uint32_t> blueprint;
    std::vector<std::size_t> ascCounts;
    std::vector<std::size_t> descCounts;
    bool alreadySortedAscending = true;
    bool reverseSortedDescending = false;
    std::vector<T> ascTails;
    std::vector<T> descTails;
    std::size_t overflowCount = 0;

    // Number of input elements processed before new-pile creation is frozen.
    // Elements with indices [0, freezePoint) may create piles; elements at and
    // after freezePoint may only enter existing piles or overflow.
    std::size_t freezePoint = 0;
};

template <bool UseHint, typename T, typename Less = std::less<T>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreezeImpl(
    const std::vector<T>& arr,
    Less less = Less{},
    FreezePolicy freezePolicy = FreezePolicy::fixed_half
) {
    constexpr std::size_t MinPrefixPileLength = 32;

    const std::size_t n = arr.size();
    FrozenInsertionResult<T> result;
    result.freezePoint = calculateEarlyFreezePoint(n);
    if (n == 0) return result;

    // Inspect the initial monotonic run before allocating the O(n) blueprint.
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
            result.alreadySortedAscending = false;
            result.reverseSortedDescending = true;
        } else {
            result.alreadySortedAscending = true;
            result.reverseSortedDescending = false;
        }
        return result;
    }

    // Reaching here proves that both comparison directions occur in the input.
    // The sort can no longer be globally monotonic, so avoid maintaining these
    // flags in the dominant insertion loop.
    result.alreadySortedAscending = false;
    result.reverseSortedDescending = false;

    result.blueprint.resize(n);

    const std::size_t reservePiles = simulated::estimatePileReserve(result.freezePoint);
    result.ascCounts.reserve(reservePiles);
    result.descCounts.reserve(reservePiles);
    result.ascTails.reserve(reservePiles);
    result.descTails.reserve(reservePiles);

    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
    bool descendingMode = false;
    std::size_t processStart = 1;

    if (prefixEnd >= MinPrefixPileLength &&
        prefixDirection != PrefixDirection::Unknown) {
        processStart = prefixEnd;
        if (prefixDirection == PrefixDirection::Ascending) {
            result.ascTails.push_back(arr[prefixEnd - 1]);
            result.ascCounts.push_back(prefixEnd);
            std::fill_n(result.blueprint.begin(), prefixEnd, simulated::makeAscTag(0));
            descendingMode = false;
        } else {
            result.descTails.push_back(arr[prefixEnd - 1]);
            result.descCounts.push_back(prefixEnd);
            std::fill_n(result.blueprint.begin(), prefixEnd, simulated::makeDescTag(0));
            descendingMode = true;
        }
    } else {
        result.ascTails.push_back(arr[0]);
        result.ascCounts.push_back(1);
        result.blueprint[0] = simulated::makeAscTag(0);
    }

    auto updateDirection = [&](const T& previous, const T& value) {
        if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
    };

    auto processUnfrozen = [&](const T& previous, const T& value, std::size_t i) {
        updateDirection(previous, value);
        if (descendingMode) {
            simulateInsertValueDescendingPiles<UseHint>(
                result.descTails, lastPileIndexDescending, value, i,
                result.blueprint, result.descCounts, less);
        } else {
            simulateInsertValueAscendingPiles<UseHint>(
                result.ascTails, lastPileIndexAscending, value, i,
                result.blueprint, result.ascCounts, less);
        }
    };

    auto processFrozen = [&](const T& previous, const T& value, std::size_t i) {
        updateDirection(previous, value);
        bool overflow;
        if (descendingMode) {
            overflow = result.descTails.empty() ||
                       less(result.descTails.back(), value);
            if (!overflow) {
                simulateInsertValueDescendingPiles<UseHint>(
                    result.descTails, lastPileIndexDescending, value, i,
                    result.blueprint, result.descCounts, less);
            }
        } else {
            overflow = result.ascTails.empty() ||
                       less(value, result.ascTails.back());
            if (!overflow) {
                simulateInsertValueAscendingPiles<UseHint>(
                    result.ascTails, lastPileIndexAscending, value, i,
                    result.blueprint, result.ascCounts, less);
            }
        }
        if (overflow) {
            result.blueprint[i] = simulated::OVERFLOW_TAG;
            ++result.overflowCount;
        }
    };

    const std::size_t halfPoint = std::min(result.freezePoint, n);
    const std::size_t unfrozenEnd = std::max(processStart, halfPoint);

    auto pileCount = [&]() {
        return result.ascCounts.size() + result.descCounts.size();
    };

    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*)) {
        T previous = arr[processStart - 1];
        std::size_t i = processStart;
        for (; i < unfrozenEnd; ++i) {
            const T value = arr[i];
            processUnfrozen(previous, value, i);
            previous = value;
        }

        const std::size_t pilesAtHalf = pileCount();
        if (shouldExtendToPowerOfTwo(freezePolicy, n, pilesAtHalf)) {
            const std::size_t pileCap = nextPowerOfTwoStrict(pilesAtHalf);
            for (; i < n && pileCount() < pileCap; ++i) {
                const T value = arr[i];
                processUnfrozen(previous, value, i);
                previous = value;
            }
            result.freezePoint = i;
        }

        for (; i < n; ++i) {
            const T value = arr[i];
            processFrozen(previous, value, i);
            previous = value;
        }
    } else {
        std::size_t i = processStart;
        for (; i < unfrozenEnd; ++i) {
            processUnfrozen(arr[i - 1], arr[i], i);
        }

        const std::size_t pilesAtHalf = pileCount();
        if (shouldExtendToPowerOfTwo(freezePolicy, n, pilesAtHalf)) {
            const std::size_t pileCap = nextPowerOfTwoStrict(pilesAtHalf);
            for (; i < n && pileCount() < pileCap; ++i) {
                processUnfrozen(arr[i - 1], arr[i], i);
            }
            result.freezePoint = i;
        }

        for (; i < n; ++i) {
            processFrozen(arr[i - 1], arr[i], i);
        }
    }

    return result;
}


template <typename T, typename Less = std::less<T>>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
inline bool insertionPrefixLooksRandomLike(
    const std::vector<T>& arr,
    Less less = Less{},
    std::size_t prefixElements = 64,
    std::size_t minPilesPerGame = 6
) {
    if (arr.size() < prefixElements) return false;
    if constexpr (!(std::is_trivially_copyable_v<T> &&
                    sizeof(T) <= 2 * sizeof(void*))) {
        return false;
    } else {
        // Preserve the existing long-prefix fast path: if the first 32 values
        // are monotonic, this cannot be a high-entropy Random-like prefix and
        // there is no reason to build the full two-game probe.
        bool sawAscending = false;
        bool sawDescending = false;
        for (std::size_t i = 1; i < 32; ++i) {
            if (less(arr[i - 1], arr[i])) sawAscending = true;
            else if (less(arr[i], arr[i - 1])) sawDescending = true;
            if (sawAscending && sawDescending) break;
        }
        if (!(sawAscending && sawDescending)) return false;

        // Independent fixed-size probe: classify once, then dispatch to a fully
        // specialized hinted or no-hint insertion implementation. Keeping this
        // probe outside the production insertion loop avoids a per-element
        // policy branch and preserves compiler specialization of both paths.
        std::array<T, 64> ascTails{};
        std::array<T, 64> descTails{};
        std::size_t ascCount = 1;
        std::size_t descCount = 0;
        ascTails[0] = arr[0];
        bool descendingMode = false;

        auto descPile = [&](const T& value) {
            std::size_t lo = 0, hi = descCount;
            while (lo < hi) {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (less(descTails[mid], value)) lo = mid + 1;
                else hi = mid;
            }
            return lo;
        };
        auto ascPile = [&](const T& value) {
            std::size_t lo = 0, hi = ascCount;
            while (lo < hi) {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (less(value, ascTails[mid])) lo = mid + 1;
                else hi = mid;
            }
            return lo;
        };

        for (std::size_t i = 1; i < prefixElements; ++i) {
            const T& previous = arr[i - 1];
            const T& value = arr[i];
            if (less(previous, value)) descendingMode = false;
            else if (less(value, previous)) descendingMode = true;

            if (descendingMode) {
                const std::size_t p = descPile(value);
                descTails[p] = value;
                if (p == descCount) ++descCount;
            } else {
                const std::size_t p = ascPile(value);
                ascTails[p] = value;
                if (p == ascCount) ++ascCount;
            }
        }
        return ascCount >= minPilesPerGame && descCount >= minPilesPerGame;
    }
}

template <typename T, typename Less = std::less<T>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreeze(
    const std::vector<T>& arr,
    Less less = Less{},
    FreezePolicy freezePolicy = FreezePolicy::fixed_half
) {
    const bool noHintRoute =
        arr.size() >= 10000 && insertionPrefixLooksRandomLike(arr, less);
    if (noHintRoute) {
        return simulatePatienceInsertionBlueprintEarlyFreezeImpl<false>(
            arr, less, freezePolicy);
    }
    return simulatePatienceInsertionBlueprintEarlyFreezeImpl<true>(
        arr, less, freezePolicy);
}


inline bool blueprintPrefixLooksRandomLike(
    const std::vector<uint32_t>& blueprint,
    std::size_t prefixElements = 64,
    std::size_t minPilesPerGame = 6
) {
    if (blueprint.size() < prefixElements) return false;

    std::size_t ascPiles = 0;
    std::size_t descPiles = 0;
    for (std::size_t i = 0; i < prefixElements; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == simulated::OVERFLOW_TAG) continue;

        const std::size_t pilesSeen =
            static_cast<std::size_t>(simulated::localPileId(tag)) + 1;
        if (simulated::isDescTag(tag)) {
            descPiles = std::max(descPiles, pilesSeen);
        } else {
            ascPiles = std::max(ascPiles, pilesSeen);
        }
        if (ascPiles >= minPilesPerGame && descPiles >= minPilesPerGame) {
            return true;
        }
    }
    return false;
}


template <typename T>
inline bool frozenInsertionLooksRandomLike(
    const FrozenInsertionResult<T>& sim,
    std::size_t n
) {
    if (n < 10000) return false;

    const std::size_t finalPileCount =
        sim.ascCounts.size() + sim.descCounts.size();
    const std::size_t probeScale =
        sim.freezePoint == 0 ? n : sim.freezePoint;
    __extension__ typedef unsigned __int128 Wide;
    if (static_cast<Wide>(finalPileCount) * finalPileCount <
        static_cast<Wide>(5) * probeScale) {
        return false;
    }

    return blueprintPrefixLooksRandomLike(sim.blueprint);
}


template <typename T, typename Less = std::less<T>>
std::vector<std::size_t> reconstructFrozenBlueprintWithOverflowBands(
    const std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    std::vector<std::size_t> ascCounts,
    std::vector<std::size_t> descCounts,
    std::size_t overflowCount,
    std::vector<T>& tmp,
    Less less = Less{}
) {
    constexpr std::size_t BandSize = 32;
    const std::size_t n = arr.size();
    const std::size_t normalCount = n - overflowCount;
    const std::size_t numAscPiles = ascCounts.size();
    const std::size_t numDescPiles = descCounts.size();
    const std::size_t numNormalRuns = numAscPiles + numDescPiles;

    const std::size_t overflowRuns =
        (overflowCount + BandSize - 1) / BandSize;
    std::vector<std::size_t> runStart;
    runStart.reserve(numNormalRuns + overflowRuns + 1);
    runStart.resize(numNormalRuns + 1, 0);
    std::size_t out = 0;
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        runStart[p] = out;
        out += ascCounts[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        const std::size_t r = numAscPiles + p;
        runStart[r] = out;
        out += descCounts[p];
    }
    runStart[numNormalRuns] = out;
    assert(out == normalCount);

    if constexpr (std::is_default_constructible_v<T>) {
        tmp.resize(n);
    } else {
        // Copy-construct valid slots for non-default-constructible values.
        // Reconstruction overwrites each slot exactly once afterward.
        tmp = arr;
    }

    // Reuse moved count-vector storage as reconstruction cursors.
    for (std::size_t p = 0; p < numAscPiles; ++p) {
        ascCounts[p] = runStart[p];
    }
    for (std::size_t p = 0; p < numDescPiles; ++p) {
        descCounts[p] = runStart[numAscPiles + p + 1];
    }

    // Gather overflow values contiguously during the blueprint scan. Sorting
    // each fixed-size band afterward avoids shifting up to 31 objects for every
    // overflow insertion while preserving the same ascending run boundaries.
    std::size_t overflowSeen = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        if (tag == simulated::OVERFLOW_TAG) {
            tmp[normalCount + overflowSeen] = arr[i];
            ++overflowSeen;
            continue;
        }

        const bool desc = simulated::isDescTag(tag);
        const std::size_t local = static_cast<std::size_t>(simulated::localPileId(tag));
        if (desc) tmp[--descCounts[local]] = arr[i];
        else tmp[ascCounts[local]++] = arr[i];
    }
    assert(overflowSeen == overflowCount);

    for (std::size_t pos = normalCount; pos < n; pos += BandSize) {
        const std::size_t end = std::min(n, pos + BandSize);
        std::sort(
            tmp.begin() + static_cast<std::ptrdiff_t>(pos),
            tmp.begin() + static_cast<std::ptrdiff_t>(end),
            less);
        runStart.push_back(end);
    }
    return runStart;
}



// Public V5 entry point: early freeze with deferred-sort overflow bands.
template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>,
                  "jessesort::simulated_early_freeze::sort requires copyable values because pile tails are stored by value");
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
                  "jessesort::simulated_early_freeze::sort requires movable values for merging");
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
                  "Comparator must be callable as bool(const T&, const T&)");

    if (arr.size() < 2) return;
    FrozenInsertionResult<T> sim =
        simulatePatienceInsertionBlueprintEarlyFreeze(
            arr, less, FreezePolicy::power2_broad);
    if (sim.alreadySortedAscending) return;
    if (sim.reverseSortedDescending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }
    bool useRandomBranchlessMerge = false;
    if constexpr (std::is_trivially_copyable_v<T> &&
                  sizeof(T) <= 2 * sizeof(void*)) {
        useRandomBranchlessMerge =
            frozenInsertionLooksRandomLike(sim, arr.size());
    }

    std::vector<T> tmp;
    std::vector<std::size_t> runStart =
        reconstructFrozenBlueprintWithOverflowBands(
            arr, sim.blueprint, std::move(sim.ascCounts),
            std::move(sim.descCounts), sim.overflowCount, tmp, less);
    simulated::mergeRunsFromTmpToArr(
        tmp, arr, std::move(runStart), less,
        simulated::MergeSchedule::defer_dominant_first_endpoint,
        useRandomBranchlessMerge);
}


} // namespace jessesort::simulated_early_freeze
#endif
