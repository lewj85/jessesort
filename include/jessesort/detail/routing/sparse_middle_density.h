#pragma once

#include <cstddef>
#include <span>

namespace jessesort::detail {

// E726 middle-density distributed sparse-disorder admission policy.
//
// This is intentionally realization-agnostic so allocating and allocation-free
// production pipelines make the same generic front-end decision. A caller that
// admits the input is responsible for choosing a realization compatible with
// its allocation/complexity contract.
template<class T, class Less>
inline bool sparseMiddleDensityCandidate(std::span<const T> values, Less less) {
    constexpr std::size_t kMinN = 10000;
    constexpr std::size_t kWindow = 64;
    constexpr std::size_t kWindows = 8;
    const std::size_t n = values.size();
    if (n < kMinN) return false;

    unsigned prefixInv = 0;
    for (std::size_t i = 1; i < 8; ++i)
        prefixInv += static_cast<unsigned>(less(values[i], values[i - 1]));
    if (prefixInv > 2) return false;

    unsigned totalInv = 0;
    unsigned nonzeroWindows = 0;
    for (std::size_t w = 0; w < kWindows; ++w) {
        const std::size_t start = ((n - kWindow) * (2 * w + 1)) / (2 * kWindows);
        unsigned inv = 0;
        for (std::size_t i = start + 1; i < start + kWindow; ++i)
            inv += static_cast<unsigned>(less(values[i], values[i - 1]));
        if (inv) ++nonzeroWindows;
        if (inv > 16) return false;
        totalInv += inv;
        if (totalInv > 64) return false;
        const std::size_t remaining = kWindows - (w + 1);
        if (nonzeroWindows + remaining < 6) return false;
    }
    return nonzeroWindows >= 6 && totalInv >= 18;
}

} // namespace jessesort::detail
