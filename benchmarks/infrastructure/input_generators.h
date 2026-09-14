#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace jessesort::bench::inputs {

inline std::size_t structured_scale(std::size_t n) {
    return std::max<std::size_t>(8, static_cast<std::size_t>(
        std::pow(static_cast<double>(n), 2.0 / 3.0)));
}

template <class URBG>
void random(std::vector<int>& v, URBG& rng) {
    std::uniform_int_distribution<int> d(
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    for (int& x : v) x = d(rng);
}

inline void sorted(std::vector<int>& v) {
    std::iota(v.begin(), v.end(), 0);
}

inline void reverse(std::vector<int>& v) {
    std::iota(v.rbegin(), v.rend(), 0);
}

template <class URBG>
void sorted_noise(std::vector<int>& v, URBG& rng, double rate) {
    sorted(v);
    if (v.empty()) return;
    std::bernoulli_distribution change(rate);
    std::uniform_int_distribution<int> replacement(0, static_cast<int>(v.size()) - 1);
    for (int& x : v) if (change(rng)) x = replacement(rng);
}

template <class URBG>
void reverse_noise(std::vector<int>& v, URBG& rng, double rate) {
    reverse(v);
    if (v.empty()) return;
    std::bernoulli_distribution change(rate);
    std::uniform_int_distribution<int> replacement(0, static_cast<int>(v.size()) - 1);
    for (int& x : v) if (change(rng)) x = replacement(rng);
}

template <class URBG>
void random_cardinality(std::vector<int>& v, URBG& rng, int cardinality) {
    std::uniform_int_distribution<int> d(0, cardinality - 1);
    for (int& x : v) x = d(rng);
}

inline void diverging(std::vector<int>& v) {
    // Two interleaved monotone lanes move away from one another.
    int lo = 0;
    int hi = static_cast<int>(v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = (i % 2 == 0) ? lo-- : hi++;
}

inline void converging(std::vector<int>& v) {
    // Directional inverse of Diverging: the lanes approach the same midpoint.
    int lo = 0;
    int hi = static_cast<int>(v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = (i % 2 == 0) ? lo++ : hi--;
}

inline void sawtooth(std::vector<int>& v) {
    const std::size_t period = structured_scale(v.size());
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<int>(i % period);
}

template <class URBG>
void mixed_direction_runs(std::vector<int>& v, URBG& rng) {
    const std::size_t n = v.size();
    const std::size_t target = structured_scale(n);
    const std::size_t min_len = std::max<std::size_t>(4, target / 2);
    const std::size_t max_len = std::max(min_len, target + target / 2);
    std::uniform_int_distribution<std::size_t> len_dist(min_len, max_len);
    std::bernoulli_distribution descending(0.5);
    std::uniform_int_distribution<int> step_dist(1, 7);
    const long long span = std::min<long long>(
        static_cast<long long>(std::numeric_limits<int>::max()) / 8,
        std::max<long long>(1024, static_cast<long long>(n) * 8));
    std::uniform_int_distribution<long long> start_dist(-span, span);

    for (std::size_t pos = 0; pos < n;) {
        const std::size_t len = std::min(len_dist(rng), n - pos);
        const int step = step_dist(rng);
        const bool desc = descending(rng);
        long long start = start_dist(rng);
        const long long run_span = static_cast<long long>(len - 1) * step;
        if (desc && start - run_span < std::numeric_limits<int>::min())
            start = static_cast<long long>(std::numeric_limits<int>::min()) + run_span;
        if (!desc && start + run_span > std::numeric_limits<int>::max())
            start = static_cast<long long>(std::numeric_limits<int>::max()) - run_span;
        for (std::size_t k = 0; k < len; ++k) {
            const long long delta = static_cast<long long>(k) * step;
            v[pos + k] = static_cast<int>(desc ? start - delta : start + delta);
        }
        pos += len;
    }
}

template <class URBG>
void alternating_direction_runs(std::vector<int>& v, URBG& rng) {
    const std::size_t n = v.size();
    const std::size_t target = structured_scale(n);
    const std::size_t min_len = std::max<std::size_t>(4, target / 2);
    const std::size_t max_len = std::max(min_len, target + target / 2);
    std::uniform_int_distribution<std::size_t> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> step_dist(1, 7);
    const long long span = std::min<long long>(
        static_cast<long long>(std::numeric_limits<int>::max()) / 8,
        std::max<long long>(1024, static_cast<long long>(n) * 8));
    std::uniform_int_distribution<long long> start_dist(-span, span);
    std::bernoulli_distribution first_descending(0.5);
    bool desc = first_descending(rng);

    for (std::size_t pos = 0; pos < n; desc = !desc) {
        const std::size_t len = std::min(len_dist(rng), n - pos);
        const int step = step_dist(rng);
        long long start = start_dist(rng);
        const long long run_span = static_cast<long long>(len - 1) * step;
        if (desc && start - run_span < std::numeric_limits<int>::min())
            start = static_cast<long long>(std::numeric_limits<int>::min()) + run_span;
        if (!desc && start + run_span > std::numeric_limits<int>::max())
            start = static_cast<long long>(std::numeric_limits<int>::max()) - run_span;
        for (std::size_t k = 0; k < len; ++k) {
            const long long delta = static_cast<long long>(k) * step;
            v[pos + k] = static_cast<int>(desc ? start - delta : start + delta);
        }
        pos += len;
    }
}

template <class URBG>
void block_sorted(std::vector<int>& v, URBG& rng) {
    const std::size_t n = v.size();
    sorted(v);
    const std::size_t target = structured_scale(n);
    const std::size_t min_len = std::max<std::size_t>(4, target / 2);
    const std::size_t max_len = std::max(min_len, target + target / 2);
    std::uniform_int_distribution<std::size_t> len_dist(min_len, max_len);
    struct Block { std::size_t begin; std::size_t len; };
    std::vector<Block> blocks;
    for (std::size_t pos = 0; pos < n;) {
        const std::size_t len = std::min(len_dist(rng), n - pos);
        blocks.push_back({pos, len});
        pos += len;
    }
    std::shuffle(blocks.begin(), blocks.end(), rng);
    const auto source = v;
    std::size_t dest = 0;
    for (const Block& block : blocks) {
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(block.begin),
                    block.len, v.begin() + static_cast<std::ptrdiff_t>(dest));
        dest += block.len;
    }
}

inline void organ_pipe(std::vector<int>& v) {
    const std::size_t n = v.size();
    for (std::size_t i = 0; i < n; ++i)
        v[i] = static_cast<int>(i <= n / 2 ? i : n - i);
}

template <class URBG>
void rotated(std::vector<int>& v, URBG& rng) {
    sorted(v);
    if (v.empty()) return;
    std::uniform_int_distribution<std::size_t> d(0, v.size() - 1);
    std::rotate(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(d(rng)), v.end());
}

template <class URBG>
void mixed_phase3(std::vector<int>& v, URBG& rng) {
    std::size_t begin = 0;
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const std::size_t end = v.size() * (phase + 1) / 3;
        std::vector<int> part(end - begin);
        if (phase == 0) sorted_noise(part, rng, 0.05);
        else if (phase == 1) mixed_direction_runs(part, rng);
        else random(part, rng);
        std::copy(part.begin(), part.end(),
                  v.begin() + static_cast<std::ptrdiff_t>(begin));
        begin = end;
    }
}

} // namespace jessesort::bench::inputs
