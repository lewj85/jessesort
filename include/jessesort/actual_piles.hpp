#ifndef JESSESORT_ACTUAL_PILES_HPP
#define JESSESORT_ACTUAL_PILES_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace jessesort::actual_piles {

// Values are physically appended to patience piles during the insertion pass.

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
void mergeRuns(std::vector<T>& src, std::vector<T>& dst,
               std::vector<std::size_t>& ends, Less less) {
    if (ends.size() <= 1) return;
    bool sourceIsSrc = true;
    std::vector<std::size_t> next;
    next.reserve((ends.size() + 1) / 2);

    while (ends.size() > 1) {
        auto& in = sourceIsSrc ? src : dst;
        auto& out = sourceIsSrc ? dst : src;
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
            std::size_t i = left, j = mid, k = left;
            while (i < mid && j < right) {
                if (less(in[j], in[i])) out[k++] = std::move(in[j++]);
                else out[k++] = std::move(in[i++]);
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

template <class T, class Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    const std::size_t n = arr.size();
    if (n < 2) return;

    bool ascending = true;
    bool descending = true;
    std::vector<std::vector<T>> ascPiles;
    std::vector<std::vector<T>> descPiles;

    // Compact copies of the current pile tails. Binary search operates on
    // these contiguous arrays instead of repeatedly dereferencing
    // piles[mid].back(). Each entry always mirrors its pile's last value.
    std::vector<T> ascBaseArray;
    std::vector<T> descBaseArray;

    ascPiles.reserve(32);
    descPiles.reserve(32);
    ascBaseArray.reserve(32);
    descBaseArray.reserve(32);

    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0) {
            if (less(arr[i], arr[i - 1])) ascending = false;
            if (less(arr[i - 1], arr[i])) descending = false;
        }
        const bool useAscendingGame = (i == 0) || !less(arr[i], arr[i - 1]);
        auto& piles = useAscendingGame ? ascPiles : descPiles;
        auto& baseArray = useAscendingGame ? ascBaseArray : descBaseArray;

        const std::size_t p = useAscendingGame
            ? findAscendingPile(baseArray, arr[i], less)
            : findDescendingPile(baseArray, arr[i], less);

        if (p == piles.size()) {
            // Copy the new tail into the compact base array before moving the
            // value into its physical pile.
            baseArray.push_back(arr[i]);
            piles.emplace_back();
        } else {
            // The selected pile receives a new tail, so keep its base-array
            // mirror synchronized before moving from the input element.
            baseArray[p] = arr[i];
        }

        piles[p].push_back(std::move(arr[i]));
    }

    if (ascending) return;
    if (descending) {
        std::reverse(arr.begin(), arr.end());
        return;
    }

    std::vector<T> flat;
    flat.reserve(n);
    std::vector<std::size_t> ends;
    ends.reserve(ascPiles.size() + descPiles.size());
    for (auto& pile : ascPiles) {
        for (auto& value : pile) flat.push_back(std::move(value));
        ends.push_back(flat.size());
    }
    for (auto& pile : descPiles) {
        for (auto it = pile.rbegin(); it != pile.rend(); ++it)
            flat.push_back(std::move(*it));
        ends.push_back(flat.size());
    }

    std::vector<T> buffer(n);
    mergeRuns(flat, buffer, ends, less);
    arr = std::move(flat);
}

} // namespace jessesort::actual_piles
#endif
