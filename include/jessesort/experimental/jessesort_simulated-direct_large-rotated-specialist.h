#ifndef JESSESORT_E351_SIMULATED_DIRECT_LARGE_ROTATED_SPECIALIST_H
#define JESSESORT_E351_SIMULATED_DIRECT_LARGE_ROTATED_SPECIALIST_H

#include <jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace jessesort::simulated_direct_large_rotated_specialist {

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void fallbackDirect(std::vector<T>& arr, Less less) {
    jessesort::simulated_direct_merge::sort(arr, less);
}

template <typename T, typename Less>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
bool tryLargeRotated(std::vector<T>& arr, Less less) {
    const std::size_t n = arr.size();
    if (n < 65536) return false;
    if (!less(arr.back(), arr.front())) return false;

    unsigned coarseDrops = 0;
    std::size_t previous = 0;
    for (std::size_t sample = 1; sample <= 8; ++sample) {
        const std::size_t index = ((n - 1) * sample) / 8;
        coarseDrops += static_cast<unsigned>(less(arr[index], arr[previous]));
        previous = index;
    }
    if (coarseDrops != 1) return false;

    for (std::size_t i = 1; i < 64; ++i)
        if (less(arr[i], arr[i - 1])) return false;

    std::size_t drop = 0;
    for (std::size_t i = 64; i < n; ++i) {
        if (less(arr[i], arr[i - 1])) {
            if (drop != 0) return false;
            drop = i;
        }
    }
    if (drop == 0) return false;

    // With both runs nondecreasing and last < first, every element in the
    // right run is ordered before every element in the left run.
    std::rotate(arr.begin(), arr.begin() + static_cast<std::ptrdiff_t>(drop), arr.end());
    return true;
}

template <typename T, typename Less = std::less<T>>
void sort(std::vector<T>& arr, Less less = Less{}) {
    if (tryLargeRotated(arr, less)) return;
    fallbackDirect(arr, less);
}

} // namespace jessesort::simulated_direct_large_rotated_specialist
#endif
