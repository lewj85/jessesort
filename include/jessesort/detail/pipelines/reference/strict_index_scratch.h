#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include <jessesort/detail/pipelines/reference/live_phase_strict_noalloc.h>

namespace jessesort::strict_index_scratch {

template<class T, class Less>
bool repeatedOverlapCandidate(const std::vector<T>& values, Less less) {
    const std::size_t n = values.size();
    if (n < 256) return false;
    int previousDirection = 0;
    std::size_t directionChanges = 0, previousIndex = 0;
    for (std::size_t sample = 1; sample <= 8; ++sample) {
        const std::size_t index = (sample * (n - 1)) / 8;
        int direction = 0;
        if (less(values[previousIndex], values[index])) direction = 1;
        else if (less(values[index], values[previousIndex])) direction = -1;
        if (direction != 0) {
            if (previousDirection != 0 && direction != previousDirection) ++directionChanges;
            previousDirection = direction;
        }
        previousIndex = index;
    }
    if (directionChanges < 3 || !less(values[0], values[1])) return false;
    std::size_t end = 2;
    while (end < n && !less(values[end], values[end - 1])) ++end;
    if (end < 32 || end > n / 2 || end + 1 >= n) return false;
    const std::size_t secondEnd = end * 2;
    const auto equivalent = [&](const T& lhs, const T& rhs) {
        return !less(lhs, rhs) && !less(rhs, lhs);
    };
    return less(values[end], values[end - 1]) && less(values[end], values[end + 1]) &&
           equivalent(values[0], values[end]) &&
           equivalent(values[end - 1], values[secondEnd - 1]) &&
           (secondEnd >= n ||
            (less(values[secondEnd], values[secondEnd - 1]) &&
             equivalent(values[0], values[secondEnd])));
}

template<class T, class Less>
bool shouldUseScratch(const std::vector<T>& values, Less less) {
    if constexpr (sizeof(T) < 32) return false;
    return jessesort::simulated_direct_live_phase::denseLocalDirectionSwitchingE738(values, less) ||
           jessesort::detail::sparseMiddleDensityCandidate(
               std::span<const T>(values.data(), values.size()), less) ||
           repeatedOverlapCandidate(values, less);
}

template<class T>
void realizePermutation(std::vector<T>& values, std::vector<std::uint32_t>& order) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (static_cast<std::size_t>(order[i]) == i) continue;
        T saved = std::move(values[i]);
        std::size_t current = i;
        while (static_cast<std::size_t>(order[current]) != i) {
            const std::size_t source = static_cast<std::size_t>(order[current]);
            values[current] = std::move(values[source]);
            order[current] = static_cast<std::uint32_t>(current);
            current = source;
        }
        values[current] = std::move(saved);
        order[current] = static_cast<std::uint32_t>(current);
    }
}

template<class T, class Less = std::less<T>>
bool sortUnconditional(std::vector<T>& values,
                       std::vector<std::uint32_t>& scratch,
                       Less less = Less{}) {
    if (scratch.size() < values.size() ||
        values.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        return false;
    scratch.resize(values.size()); // shrinking only; caller supplied capacity.
    for (std::size_t i = 0; i < values.size(); ++i)
        scratch[i] = static_cast<std::uint32_t>(i);
    const auto indirectLess = [&](std::uint32_t lhs, std::uint32_t rhs) {
        return less(values[lhs], values[rhs]);
    };
    jessesort::simulated_phase_range_engine_e691::highEntropyQuickSort(
        scratch.data(), scratch.size(),
        2 * static_cast<int>(std::bit_width(scratch.size())), indirectLess,
        false, std::uint32_t{}, false, true, nullptr, false);
    realizePermutation(values, scratch);
    return true;
}

template<class T, class Less = std::less<T>>
bool sort(std::vector<T>& values,
          std::vector<std::uint32_t>& scratch,
          Less less = Less{}) {
    // Preserve strict's existing large natural-run fast path ahead of E740.
    // Rejection is read-only; calling sortImpl below avoids probing it twice.
    if (values.size() >= 50000 &&
        jessesort::allocation_free_live_phase_strict::tryE729PrepassStrict(values, less))
        return false;
    if (scratch.size() >= values.size() && shouldUseScratch(values, less))
        return sortUnconditional(values, scratch, less);
    jessesort::allocation_free_live_phase_adaptive::sortImpl<T, Less, false>(values, less);
    return false;
}

} // namespace jessesort::strict_index_scratch
