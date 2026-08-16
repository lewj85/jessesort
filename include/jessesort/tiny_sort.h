#ifndef JESSESORT_TINY_SORT_H
#define JESSESORT_TINY_SORT_H
#include <algorithm>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>
namespace jessesort::detail {

// E155: primitive/default-comparator whole-array smallsort.
// Tiny arrays never amortize JesseSort's pile/blueprint setup. Detect the two
// monotone cases in one short scan, otherwise use an in-place insertion kernel.
template <typename T, typename Less>
inline bool tryTinyInsertionSort(std::vector<T>& arr, Less less) {
    if constexpr (std::is_trivially_copyable_v<T> &&
                  std::is_same_v<std::remove_cv_t<Less>, std::less<T>>) {
        const std::size_t n = arr.size();
        if (n > 32) return false;
        if (n < 2) return true;

        bool nondecreasing = true;
        bool nonincreasing = true;
        for (std::size_t i = 1; i < n && (nondecreasing || nonincreasing); ++i) {
            if (less(arr[i], arr[i - 1])) nondecreasing = false;
            if (less(arr[i - 1], arr[i])) nonincreasing = false;
        }
        if (nondecreasing) return true;
        if (nonincreasing) {
            std::reverse(arr.begin(), arr.end());
            return true;
        }

        for (std::size_t i = 1; i < n; ++i) {
            T value = std::move(arr[i]);
            std::size_t j = i;
            while (j != 0 && less(value, arr[j - 1])) {
                arr[j] = std::move(arr[j - 1]);
                --j;
            }
            arr[j] = std::move(value);
        }
        return true;
    }
    return false;
}
}
#endif
