#ifndef JESSESORT_DETAIL_SIMULATED_FROZEN_EARLY_FREEZE_CORE_H
#define JESSESORT_DETAIL_SIMULATED_FROZEN_EARLY_FREEZE_CORE_H

#include <jessesort/tiny_sort.h>
#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::simulated_early_freeze_legacy {

// frozen-deferred freezes the simulated pile structure early, collects later values into
// small unsorted overflow bands, then sorts those bands during reconstruction.
// The shared early-freeze insertion machinery is also reused by frozen-single and frozen-live.

// Shared blueprint encoding and merge logic are maintained in v2_simulated.h.
// This header keeps only the compact early-freeze pile search plus the
// freeze/overflow-specific pipeline.
using simulated_legacy::OVERFLOW_TAG;
using simulated_legacy::isAscTag;
using simulated_legacy::isDescTag;
using simulated_legacy::localPileId;

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
        assert(pileIndex >= 0 && static_cast<uint32_t>(pileIndex) <= simulated_legacy::PILE_MASK);
        tails.push_back(value);
        descCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = simulated_legacy::makeDescTag(static_cast<uint32_t>(pileIndex));
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
        assert(pileIndex >= 0 && static_cast<uint32_t>(pileIndex) <= simulated_legacy::PILE_MASK);
        tails.push_back(value);
        ascCounts.push_back(1);
    }
    lastPileIndex = pileIndex;
    blueprint[originalIndex] = simulated_legacy::makeAscTag(static_cast<uint32_t>(pileIndex));
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
        // frozen-single gains overwhelmingly from low-pile structured inputs. Avoid the
        // two/three-pile cases, which are commonly already cheap alternating
        // or rotated layouts and showed no reliable benefit from extension.
        return pileCount == 1 || (pileCount >= 4 && pileCount <= 32);
    }

    // frozen-deferred/frozen-live: always extend very small run sets. For larger inputs, extend when
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

inline std::size_t calculateEarlyFreezePoint(
    std::size_t n, unsigned freezePercent = 50
) {
    if (n == 0) return 0;

    const long double sampled =
        static_cast<long double>(n) * static_cast<long double>(freezePercent) / 100.0L;
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

    // Optional E177/E179 overflow-shape sample. Populated only when
    // sampleOverflowShape=true; production behavior is unchanged otherwise.
    std::size_t overflowSampleCount = 0;
    std::size_t overflowSampleAscAdj = 0;
    std::size_t overflowSampleDescAdj = 0;
    std::size_t overflowSampleEqAdj = 0;
    std::size_t overflowSampleDirectionChanges = 0;
    std::size_t overflowSampleLongest = 0;
    std::size_t overflowSampleLongValues32 = 0;
};

template <bool UseHint, bool EnableValleyRescue = false, typename T, typename Less = std::less<T>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreezeImpl(
    const std::vector<T>& arr,
    Less less = Less{},
    FreezePolicy freezePolicy = FreezePolicy::fixed_half,
    bool naturalRunRoute = false,
    bool naturalRequireFirstContinuationLong = false,
    bool enableCoherentValuePileCache = false,
    unsigned freezePercent = 50,
    bool sampleOverflowShape = false
) {
    constexpr std::size_t MinPrefixPileLength = 32;

    const std::size_t n = arr.size();
    FrozenInsertionResult<T> result;

    enum class OverflowSampleDirection : unsigned char { unknown, ascending, descending };
    OverflowSampleDirection overflowSampleDirection = OverflowSampleDirection::unknown;
    std::size_t overflowSampleRunStart = 0;
    std::optional<T> previousOverflowSample;
    bool overflowSamplingEnabled = sampleOverflowShape;
    auto observeOverflowSample = [&](const T& value) {
        if (!overflowSamplingEnabled || result.overflowSampleCount >= 32) return;
        const std::size_t k = result.overflowSampleCount;
        if (previousOverflowSample) {
            const T& previous = *previousOverflowSample;
            int rel = 0;
            if (less(previous, value)) { ++result.overflowSampleAscAdj; rel = 1; }
            else if (less(value, previous)) { ++result.overflowSampleDescAdj; rel = -1; }
            else ++result.overflowSampleEqAdj;

            if (overflowSampleDirection == OverflowSampleDirection::unknown) {
                if (rel > 0) overflowSampleDirection = OverflowSampleDirection::ascending;
                else if (rel < 0) overflowSampleDirection = OverflowSampleDirection::descending;
            } else {
                const bool continues = overflowSampleDirection == OverflowSampleDirection::ascending
                    ? rel >= 0 : rel <= 0;
                if (!continues) {
                    ++result.overflowSampleDirectionChanges;
                    result.overflowSampleLongest = std::max(
                        result.overflowSampleLongest, k - overflowSampleRunStart);
                    overflowSampleRunStart = k;
                    overflowSampleDirection = OverflowSampleDirection::unknown;
                }
            }
        }
        previousOverflowSample = value;
        ++result.overflowSampleCount;
        result.overflowSampleLongest = std::max(
            result.overflowSampleLongest, result.overflowSampleCount - overflowSampleRunStart);
        if (result.overflowSampleCount == 32 && result.overflowSampleLongest >= 32)
            result.overflowSampleLongValues32 = 32;
    };
    result.freezePoint = calculateEarlyFreezePoint(n, freezePercent);
    if (n == 0) return result;

    // Inspect the initial monotonic run before allocating the O(n) blueprint.
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

    const std::size_t reservePiles = simulated_legacy::estimatePileReserve(result.freezePoint);
    result.ascCounts.reserve(reservePiles);
    result.descCounts.reserve(reservePiles);
    result.ascTails.reserve(reservePiles);
    result.descTails.reserve(reservePiles);

    int lastPileIndexAscending = 0;
    int lastPileIndexDescending = 0;
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
            std::fill_n(result.blueprint.begin(), prefixEnd, simulated_legacy::makeAscTag(0));
            descendingMode = false;
        } else {
            result.descTails.push_back(arr[prefixEnd - 1]);
            result.descCounts.push_back(prefixEnd);
            std::fill_n(result.blueprint.begin(), prefixEnd, simulated_legacy::makeDescTag(0));
            descendingMode = true;
        }
    } else {
        if (prefixDirection == PrefixDirection::Descending) {
            result.descTails.push_back(arr[0]);
            result.descCounts.push_back(1);
            result.blueprint[0] = simulated_legacy::makeDescTag(0);
            descendingMode = true;
        } else {
            result.ascTails.push_back(arr[0]);
            result.ascCounts.push_back(1);
            result.blueprint[0] = simulated_legacy::makeAscTag(0);
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

    auto updateDirection = [&](const T& previous, const T& value) {
        if (postPrefixDirectionOverride) {
            descendingMode = postPrefixOverrideDescending;
            postPrefixDirectionOverride = false;
        } else if (less(previous, value)) descendingMode = false;
        else if (less(value, previous)) descendingMode = true;
    };

    auto processUnfrozen = [&](const T& previous, const T& value, std::size_t i) {
        updateDirection(previous, value);
        if constexpr (EnableValleyRescue) {
            if (descendingMode && i + 1 < n && less(value, arr[i + 1])) {
                const bool wouldCreateDesc = result.descTails.empty() || less(result.descTails.back(), value);
                const bool canContinueAsc = !result.ascTails.empty() && !less(value, result.ascTails.back());
                if (wouldCreateDesc && canContinueAsc) descendingMode = false;
            }
        }
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
        if constexpr (EnableValleyRescue) {
            if (descendingMode && i + 1 < n && less(value, arr[i + 1])) {
                const bool wouldCreateDesc = result.descTails.empty() || less(result.descTails.back(), value);
                const bool canContinueAsc = !result.ascTails.empty() && !less(value, result.ascTails.back());
                if (wouldCreateDesc && canContinueAsc) descendingMode = false;
            }
        }
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
            observeOverflowSample(value);
            result.blueprint[i] = simulated_legacy::OVERFLOW_TAG;
            ++result.overflowCount;
        }
    };

    const std::size_t halfPoint = std::min(result.freezePoint, n);
    const std::size_t unfrozenEnd = std::max(processStart, halfPoint);

    auto pileCount = [&]() {
        return result.ascCounts.size() + result.descCounts.size();
    };
    auto configureOverflowSampling = [&]() {
        if (!sampleOverflowShape) { overflowSamplingEnabled = false; return; }
        const std::size_t piles = pileCount();
        const std::size_t ascPiles = result.ascCounts.size();
        const std::size_t descPiles = result.descCounts.size();
        const bool lowPileDirectionObvious = piles <= 64 &&
            ((ascPiles >= 3 * std::max<std::size_t>(descPiles, 1)) ||
             (descPiles >= 3 * std::max<std::size_t>(ascPiles, 1)));
        overflowSamplingEnabled = !lowPileDirectionObvious &&
            (piles <= 64 ||
             static_cast<long double>(piles) <= 0.006L * static_cast<long double>(n));
    };

    bool naturalRunCandidate = naturalRunRoute &&
        prefixDirection != PrefixDirection::Unknown &&
        prefixEnd >= MinPrefixPileLength && prefixEnd * 8 >= n;
    if (naturalRunCandidate && naturalRequireFirstContinuationLong && processStart < n) {
        const bool firstAsc = postPrefixDirectionOverride
            ? !postPrefixOverrideDescending
            : less(arr[processStart - 1], arr[processStart]);
        const bool firstDesc = postPrefixDirectionOverride
            ? postPrefixOverrideDescending
            : (!firstAsc && less(arr[processStart], arr[processStart - 1]));
        std::size_t firstEnd = processStart + 1;
        if (firstAsc) {
            while (firstEnd < n && less(arr[firstEnd - 1], arr[firstEnd])) ++firstEnd;
        } else if (firstDesc) {
            while (firstEnd < n && less(arr[firstEnd], arr[firstEnd - 1])) ++firstEnd;
        }
        naturalRunCandidate = (firstAsc || firstDesc) && firstEnd - processStart >= 8;
    }
    if (naturalRunCandidate && processStart < n) {
        auto processRangeNatural = [&](std::size_t begin, std::size_t end, bool frozen) {
            std::size_t i = begin;
            auto insertOne = [&](std::size_t j) {
                if (frozen) processFrozen(arr[j - 1], arr[j], j);
                else processUnfrozen(arr[j - 1], arr[j], j);
            };
            while (i < end) {
                bool asc;
                bool desc;
                if (postPrefixDirectionOverride) {
                    asc = !postPrefixOverrideDescending;
                    desc = postPrefixOverrideDescending;
                } else {
                    asc = less(arr[i - 1], arr[i]);
                    desc = !asc && less(arr[i], arr[i - 1]);
                }
                if (!asc && !desc) { insertOne(i++); continue; }
                std::size_t runEnd = i + 1;
                if (asc) while (runEnd < end && less(arr[runEnd - 1], arr[runEnd])) ++runEnd;
                else while (runEnd < end && less(arr[runEnd], arr[runEnd - 1])) ++runEnd;
                const std::size_t len = runEnd - i;
                bool batched = false;
                if (len >= 8) {
                    if (asc) {
                        int h1 = result.ascTails.empty() ? 0 : std::min<int>(lastPileIndexAscending, static_cast<int>(result.ascTails.size()) - 1);
                        int h2 = h1;
                        const int firstPile = result.ascTails.empty() ? 0 : findAscendingPileWithTails<false>(result.ascTails, h1, arr[i], less);
                        const int lastPile = result.ascTails.empty() ? 0 : findAscendingPileWithTails<false>(result.ascTails, h2, arr[runEnd - 1], less);
                        const bool allowed = firstPile == lastPile && (!frozen || firstPile < static_cast<int>(result.ascTails.size()));
                        if (allowed) {
                            if (firstPile < static_cast<int>(result.ascTails.size())) {
                                result.ascTails[static_cast<std::size_t>(firstPile)] = arr[runEnd - 1];
                                result.ascCounts[static_cast<std::size_t>(firstPile)] += len;
                            } else {
                                result.ascTails.push_back(arr[runEnd - 1]);
                                result.ascCounts.push_back(len);
                            }
                            std::fill(result.blueprint.begin() + static_cast<std::ptrdiff_t>(i),
                                      result.blueprint.begin() + static_cast<std::ptrdiff_t>(runEnd),
                                      simulated_legacy::makeAscTag(static_cast<uint32_t>(firstPile)));
                            lastPileIndexAscending = firstPile;
                            descendingMode = false;
                            postPrefixDirectionOverride = false;
                            batched = true;
                        }
                    } else {
                        int h1 = result.descTails.empty() ? 0 : std::min<int>(lastPileIndexDescending, static_cast<int>(result.descTails.size()) - 1);
                        int h2 = h1;
                        const int firstPile = result.descTails.empty() ? 0 : findDescendingPileWithTails<false>(result.descTails, h1, arr[i], less);
                        const int lastPile = result.descTails.empty() ? 0 : findDescendingPileWithTails<false>(result.descTails, h2, arr[runEnd - 1], less);
                        const bool allowed = firstPile == lastPile && (!frozen || firstPile < static_cast<int>(result.descTails.size()));
                        if (allowed) {
                            if (firstPile < static_cast<int>(result.descTails.size())) {
                                result.descTails[static_cast<std::size_t>(firstPile)] = arr[runEnd - 1];
                                result.descCounts[static_cast<std::size_t>(firstPile)] += len;
                            } else {
                                result.descTails.push_back(arr[runEnd - 1]);
                                result.descCounts.push_back(len);
                            }
                            std::fill(result.blueprint.begin() + static_cast<std::ptrdiff_t>(i),
                                      result.blueprint.begin() + static_cast<std::ptrdiff_t>(runEnd),
                                      simulated_legacy::makeDescTag(static_cast<uint32_t>(firstPile)));
                            lastPileIndexDescending = firstPile;
                            descendingMode = true;
                            postPrefixDirectionOverride = false;
                            batched = true;
                        }
                    }
                }
                if (!batched) for (std::size_t j = i; j < runEnd; ++j) insertOne(j);
                i = runEnd;
            }
        };

        std::size_t i = processStart;
        if (i < unfrozenEnd) {
            processRangeNatural(i, unfrozenEnd, false);
            i = unfrozenEnd;
        }
        const std::size_t pilesAtHalf = pileCount();
        if (shouldExtendToPowerOfTwo(freezePolicy, n, pilesAtHalf)) {
            const std::size_t pileCap = nextPowerOfTwoStrict(pilesAtHalf);
            for (; i < n && pileCount() < pileCap; ++i)
                processUnfrozen(arr[i - 1], arr[i], i);
            result.freezePoint = i;
        }
        configureOverflowSampling();
        if (i < n) processRangeNatural(i, n, true);
        return result;
    }

    const bool coherentCacheCandidate = enableCoherentValuePileCache &&
        simulated_legacy::coherentValuePileCacheSampleCandidate(arr, less, arr.size() >= 50000);
    if (coherentCacheCandidate) {
        if constexpr (simulated_legacy::specializedIntegralEligible<T, Less>) {
            struct CacheEntry {
                std::uint64_t key = 0;
                std::uint32_t pile = 0;
                bool valid = false;
            };
            std::array<CacheEntry, 128> ascCache{}, descCache{};
            auto keyOf = [](const T& value) -> std::uint64_t {
                return static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(value));
            };
            auto bucket = [&](const T& value) -> std::size_t {
                return static_cast<std::size_t>((keyOf(value) * 11400714819323198485ULL) >> 57);
            };
            auto seed = [&](const std::vector<T>& tails, auto& cache) {
                for (std::size_t p = 0; p < tails.size(); ++p) {
                    const std::size_t b = bucket(tails[p]);
                    cache[b] = CacheEntry{keyOf(tails[p]), static_cast<std::uint32_t>(p), true};
                }
            };
            seed(result.ascTails, ascCache);
            seed(result.descTails, descCache);

            auto processCached = [&](const T& previous, const T& value,
                                     std::size_t originalIndex, bool frozen) {
                updateDirection(previous, value);
                auto& tails = descendingMode ? result.descTails : result.ascTails;
                auto& counts = descendingMode ? result.descCounts : result.ascCounts;
                auto& cache = descendingMode ? descCache : ascCache;
                int& lastPile = descendingMode ? lastPileIndexDescending : lastPileIndexAscending;
                const std::uint64_t key = keyOf(value);
                const std::size_t b = bucket(value);
                auto& entry = cache[b];
                if (entry.valid && entry.key == key && entry.pile < tails.size()) {
                    const std::size_t p = entry.pile;
                    ++counts[p];
                    lastPile = static_cast<int>(p);
                    result.blueprint[originalIndex] = descendingMode
                        ? simulated_legacy::makeDescTag(static_cast<std::uint32_t>(p))
                        : simulated_legacy::makeAscTag(static_cast<std::uint32_t>(p));
                    return;
                }

                int hint = tails.empty() ? 0 : std::clamp(lastPile, 0, static_cast<int>(tails.size()) - 1);
                int pileIndex = 0;
                if (!tails.empty()) {
                    pileIndex = descendingMode
                        ? findDescendingPileWithTails<false>(tails, hint, value, less)
                        : findAscendingPileWithTails<false>(tails, hint, value, less);
                }
                if (pileIndex == static_cast<int>(tails.size()) && frozen) {
                    observeOverflowSample(value);
                    result.blueprint[originalIndex] = simulated_legacy::OVERFLOW_TAG;
                    ++result.overflowCount;
                    return;
                }
                const std::size_t p = static_cast<std::size_t>(pileIndex);
                if (p < tails.size()) {
                    const T old = tails[p];
                    const std::size_t ob = bucket(old);
                    auto& oldEntry = cache[ob];
                    if (oldEntry.valid && oldEntry.key == keyOf(old) && oldEntry.pile == p)
                        oldEntry.valid = false;
                    tails[p] = value;
                    ++counts[p];
                } else {
                    tails.push_back(value);
                    counts.push_back(1);
                }
                cache[b] = CacheEntry{key, static_cast<std::uint32_t>(p), true};
                lastPile = pileIndex;
                result.blueprint[originalIndex] = descendingMode
                    ? simulated_legacy::makeDescTag(static_cast<std::uint32_t>(p))
                    : simulated_legacy::makeAscTag(static_cast<std::uint32_t>(p));
            };

            std::size_t i = processStart;
            T previous = arr[processStart - 1];
            for (; i < unfrozenEnd; ++i) {
                const T value = arr[i];
                processCached(previous, value, i, false);
                previous = value;
            }
            const std::size_t pilesAtHalf = pileCount();
            if (shouldExtendToPowerOfTwo(freezePolicy, n, pilesAtHalf)) {
                const std::size_t pileCap = nextPowerOfTwoStrict(pilesAtHalf);
                for (; i < n && pileCount() < pileCap; ++i) {
                    const T value = arr[i];
                    processCached(previous, value, i, false);
                    previous = value;
                }
                result.freezePoint = i;
            }
            configureOverflowSampling();
            for (; i < n; ++i) {
                const T value = arr[i];
                processCached(previous, value, i, true);
                previous = value;
            }
            return result;
        }
    }

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

        configureOverflowSampling();
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

        configureOverflowSampling();
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
    std::size_t minPilesPerGame = 6,
    std::size_t* totalPilesOut = nullptr
) {
    if (totalPilesOut) *totalPilesOut = 0;
    if (arr.size() < prefixElements) return false;
    if constexpr (!(std::is_trivially_copyable_v<T> &&
                    std::is_default_constructible_v<T> &&
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
        if (totalPilesOut) *totalPilesOut = ascCount + descCount;
        return ascCount >= minPilesPerGame && descCount >= minPilesPerGame;
    }
}

template <typename T, typename Less = std::less<T>>
FrozenInsertionResult<T> simulatePatienceInsertionBlueprintEarlyFreeze(
    const std::vector<T>& arr,
    Less less = Less{},
    FreezePolicy freezePolicy = FreezePolicy::fixed_half,
    bool naturalRunRoute = false,
    bool naturalRequireFirstContinuationLong = false,
    bool enableCoherentValuePileCache = false,
    unsigned freezePercent = 50,
    bool sampleOverflowShape = false,
    bool enableValleyRescue = false
) {
    std::size_t prefixPiles = 0;
    const bool noHintRoute =
        arr.size() >= 10000 && insertionPrefixLooksRandomLike(arr, less, 64, 6, &prefixPiles);
    const bool valleyRoute = enableValleyRescue && arr.size() >= 10000 && prefixPiles >= 3 && prefixPiles <= 12;
    if (noHintRoute) {
        if (valleyRoute) return simulatePatienceInsertionBlueprintEarlyFreezeImpl<false, true>(
            arr, less, freezePolicy, naturalRunRoute, naturalRequireFirstContinuationLong, enableCoherentValuePileCache, freezePercent, sampleOverflowShape);
        return simulatePatienceInsertionBlueprintEarlyFreezeImpl<false, false>(
            arr, less, freezePolicy, naturalRunRoute, naturalRequireFirstContinuationLong, enableCoherentValuePileCache, freezePercent, sampleOverflowShape);
    }
    if (valleyRoute) return simulatePatienceInsertionBlueprintEarlyFreezeImpl<true, true>(
        arr, less, freezePolicy, naturalRunRoute, naturalRequireFirstContinuationLong, enableCoherentValuePileCache, freezePercent, sampleOverflowShape);
    return simulatePatienceInsertionBlueprintEarlyFreezeImpl<true, false>(
        arr, less, freezePolicy, naturalRunRoute, naturalRequireFirstContinuationLong, enableCoherentValuePileCache, freezePercent, sampleOverflowShape);
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
        if (tag == simulated_legacy::OVERFLOW_TAG) continue;

        const std::size_t pilesSeen =
            static_cast<std::size_t>(simulated_legacy::localPileId(tag)) + 1;
        if (simulated_legacy::isDescTag(tag)) {
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



} // namespace jessesort::simulated_early_freeze_legacy

#endif
