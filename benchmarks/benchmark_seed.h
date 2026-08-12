#pragma once

#include <cstddef>
#include <cstdint>

namespace jessesort::bench {

// Canonical starting seed for benchmark inputs and one-off A/B experiments.
// One-off harnesses should use this seed and the same trial_seed() mixing so
// trial i for a canonical workload refers to the same input population.
inline constexpr unsigned kCanonicalBaseSeed = 0x8A5CD789u;

inline unsigned trial_seed(unsigned base_seed,
                           std::size_t n,
                           int input_ordinal,
                           int trial) {
    std::uint64_t x = base_seed;
    x ^= static_cast<std::uint64_t>(n) + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2);
    x ^= static_cast<std::uint64_t>(input_ordinal) * 0xbf58476d1ce4e5b9ULL;
    x ^= static_cast<std::uint64_t>(trial) * 0x94d049bb133111ebULL;
    return static_cast<unsigned>(x ^ (x >> 32));
}

} // namespace jessesort::bench
