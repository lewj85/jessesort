#ifndef JESSESORT_EXPERIMENTAL_PHYSICAL_SINGLE_GAME_TWO_ENDED_RAINBOW_H
#define JESSESORT_EXPERIMENTAL_PHYSICAL_SINGLE_GAME_TWO_ENDED_RAINBOW_H

#include <jessesort/detail/common/tiny_sort.h>
#include <jessesort/detail/pipelines/reference/simulated.h>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace jessesort::physical_single_game_two_ended {

template<class T, class Less = std::less<T>>
void sortImpl(std::vector<T>& arr, Less less = Less{}) {
    static_assert(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>);
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>);
    const std::size_t n = arr.size();
    if (n < 2) return;

    // Preserve the maintained physical owner's monotone early exit.
    std::size_t prefixEnd = 1;
    int dir = 0;
    while (prefixEnd < n && dir == 0) {
        if (less(arr[prefixEnd-1], arr[prefixEnd])) dir = 1;
        else if (less(arr[prefixEnd], arr[prefixEnd-1])) dir = -1;
        ++prefixEnd;
    }
    if (dir > 0) {
        while (prefixEnd < n && !less(arr[prefixEnd], arr[prefixEnd-1])) ++prefixEnd;
    } else if (dir < 0) {
        while (prefixEnd < n && !less(arr[prefixEnd-1], arr[prefixEnd])) ++prefixEnd;
    }
    if (prefixEnd == n) {
        if (dir < 0) std::reverse(arr.begin(), arr.end());
        return;
    }

    // Preserve the maintained specialized direct routes before physical materialization.
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less>) {
        bool prefixMayMix = true;
        if (n >= 4) {
            const bool a = less(arr[0],arr[1]) && less(arr[1],arr[2]) && less(arr[2],arr[3]);
            const bool d = less(arr[1],arr[0]) && less(arr[2],arr[1]) && less(arr[3],arr[2]);
            prefixMayMix = !(a || d);
        }
        if (prefixMayMix) {
            T dominant = arr[0];
            if (jessesort::simulated_legacy::dominantValueSampleCandidate(arr, dominant, less)) {
                jessesort::simulated_legacy::highEntropyQuickSort(
                    arr.data(), arr.size(), 2 * static_cast<int>(std::bit_width(arr.size())), less,
                    false, T{}, false, true, nullptr, false);
                return;
            }
            if (jessesort::simulated_legacy::lowCardinalityDirectionGate(arr, less) &&
                jessesort::simulated_legacy::lowCardinalitySampleCandidate(arr, less) &&
                jessesort::simulated_legacy::trySortLowCardinalityDirectConfirmed(arr, less)) return;
            bool alternating = false;
            if (n >= 8) {
                int prev = 0; alternating = true;
                for (std::size_t i=1;i<8;++i) {
                    int now = less(arr[i-1],arr[i]) ? 1 : (less(arr[i],arr[i-1]) ? -1 : 0);
                    if (now == 0 || (prev != 0 && now == prev)) { alternating = false; break; }
                    prev = now;
                }
            }
            if (!alternating && jessesort::simulated_legacy::trySortHighEntropyPartitionDirect(arr, less)) return;
        }
    }

    struct Pile { std::deque<T> values; };
    std::vector<Pile> piles;
    piles.reserve(32);

    using EndpointMap = std::multimap<T, std::size_t, Less>;
    EndpointMap heads(less), tails(less);
    using It = typename EndpointMap::iterator;
    std::vector<It> headIt, tailIt;
    headIt.reserve(32); tailIt.reserve(32);

    auto makePile = [&](T& value) -> std::size_t {
        const std::size_t id = piles.size();
        piles.emplace_back();
        piles.back().values.push_back(std::move(value));
        headIt.push_back(heads.emplace(piles[id].values.front(), id));
        tailIt.push_back(tails.emplace(piles[id].values.back(), id));
        return id;
    };

    auto tailCandidate = [&](const T& value) -> std::size_t {
        auto it = tails.upper_bound(value); // first tail > value
        if (it == tails.begin()) return static_cast<std::size_t>(-1);
        --it; // largest tail <= value
        return it->second;
    };
    auto headCandidate = [&](const T& value) -> std::size_t {
        auto it = heads.lower_bound(value); // first head >= value under Less ordering
        if (it == heads.end()) return static_cast<std::size_t>(-1);
        return it->second;
    };

    auto appendBack = [&](std::size_t id, T& value) {
        tails.erase(tailIt[id]);
        piles[id].values.push_back(std::move(value));
        tailIt[id] = tails.emplace(piles[id].values.back(), id);
    };
    auto appendFront = [&](std::size_t id, T& value) {
        heads.erase(headIt[id]);
        piles[id].values.push_front(std::move(value));
        headIt[id] = heads.emplace(piles[id].values.front(), id);
    };

    makePile(arr[0]);
    T* previous = &piles[0].values.back();
    for (std::size_t i=1;i<n;++i) {
        T& value = arr[i];
        const bool rising = less(*previous, value);
        const bool falling = less(value, *previous);
        const std::size_t back = tailCandidate(value);
        const std::size_t front = headCandidate(value);
        std::size_t chosen = static_cast<std::size_t>(-1);
        bool useFront = false;
        if (falling && front != static_cast<std::size_t>(-1)) { chosen = front; useFront = true; }
        else if (rising && back != static_cast<std::size_t>(-1)) chosen = back;
        else if (back != static_cast<std::size_t>(-1)) chosen = back;
        else if (front != static_cast<std::size_t>(-1)) { chosen = front; useFront = true; }

        if (chosen == static_cast<std::size_t>(-1)) {
            chosen = makePile(value);
            previous = &piles[chosen].values.back();
        } else if (useFront) {
            appendFront(chosen, value);
            previous = &piles[chosen].values.front();
        } else {
            appendBack(chosen, value);
            previous = &piles[chosen].values.back();
        }
    }

    std::vector<T> flat; flat.reserve(n);
    std::vector<std::size_t> ends; ends.reserve(piles.size());
    for (auto& p : piles) {
        for (auto& v : p.values) flat.push_back(std::move(v));
        ends.push_back(flat.size());
    }
    std::vector<T> buffer;
    if constexpr (std::is_default_constructible_v<T>) buffer.resize(n); else buffer = flat;
    jessesort::simulated_legacy::mergeRunsAdjacentPairsEnds(flat, buffer, ends, less, false, true);
    arr = std::move(flat);
}

template<class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (jessesort::detail::tryTinyInsertionSort(arr, less)) return;
    sortImpl(arr, less);
}

} // namespace jessesort::physical_single_game_two_ended
#endif
