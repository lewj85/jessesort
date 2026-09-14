#pragma once

#include <cstddef>
#include <cstdint>

namespace jessesort::bench {

// Canonical starting seed for benchmark inputs and one-off A/B experiments.
// One-off harnesses should use this seed and the same trial_seed() mixing so
// trial i for a canonical workload refers to the same input population.
inline constexpr unsigned kCanonicalBaseSeed = 0x8A5CD789u;

// E756 forked-warmup policy: decision-grade timing is split across independent
// process forks.  Each fork gets its own deterministic warmup history while the
// global timed trial schedule remains unchanged.  Warmup seeds are domain-
// separated by fork and guaranteed not to collide with any timed seed in the
// complete cell population.  This prevents one arbitrary first-process allocator
// history from owning all 500 measurements.
inline constexpr unsigned kWarmupBaseSeed = 0xE756A11Du;
inline constexpr std::uint64_t kWarmupBaseSeed64 = 0xE756A11D5EED0001ULL;

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

inline unsigned warmup_seed_for_fork(unsigned timed_base_seed,
                                     std::size_t n,
                                     int input_ordinal,
                                     int fork_index,
                                     int warmup_index,
                                     int total_timed_trials) {
    unsigned candidate = trial_seed(
        kWarmupBaseSeed, n, input_ordinal,
        0x756000 + fork_index * 64 + warmup_index);
    for (;;) {
        bool collision = false;
        for (int t = 0; t < total_timed_trials; ++t) {
            if (candidate == trial_seed(timed_base_seed, n, input_ordinal, t)) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            for (int w = 0; w < warmup_index; ++w) {
                if (candidate == warmup_seed_for_fork(
                        timed_base_seed, n, input_ordinal, fork_index, w,
                        total_timed_trials)) {
                    collision = true;
                    break;
                }
            }
        }
        if (!collision) return candidate;
        candidate = static_cast<unsigned>(candidate * 1664525u + 1013904223u);
    }
}

inline std::uint64_t mix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

inline std::uint64_t warmup_seed64_for_fork(std::size_t n,
                                            std::uint64_t input_ordinal,
                                            int fork_index,
                                            int warmup_index) {
    std::uint64_t x = kWarmupBaseSeed64;
    x ^= mix64(static_cast<std::uint64_t>(n));
    x ^= mix64(input_ordinal + 0x75600000ULL);
    x ^= mix64(static_cast<std::uint64_t>(fork_index) + 0x756F0000ULL);
    x ^= mix64(static_cast<std::uint64_t>(warmup_index) + 0x7560ULL);
    return mix64(x);
}

} // namespace jessesort::bench
