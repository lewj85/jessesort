#include <jessesort/jessesort_physical_probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/jessesort_indexed_probe-routed_adjacent-adaptive-buffered_move-only.h>
#include <jessesort/jessesort_noalloc_bounded-fallback_inplace-adaptive_partition-heapsort-fallback.h>
#include <jessesort/jessesort_noalloc-low-run-merge_overlap-routed_inplace-adaptive_run-reclaim64.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace {

enum class InputType : int {
    Random = 0,
    Sorted = 1,
    Reverse = 2,
    NearlySorted = 3,
    Random25 = 4,
    Alternating = 5,
    Sawtooth = 6,
    BlockSorted = 7,
    OrganPipe = 8,
    Rotated = 9,
    SortedNoise10 = 12,
    MixedDirectionRuns = 13
};

static std::size_t structured_scale(std::size_t n) {
    return std::max<std::size_t>(8, static_cast<std::size_t>(
        std::pow(static_cast<double>(n), 2.0 / 3.0)));
}

// Order-preserving map from signed int32 ordering into unsigned u64 ordering.
// For any int32 a,b: a < b iff encode(a) < encode(b), and equality is preserved.
static std::uint64_t encode_int(int x) {
    static_assert(sizeof(int) == 4, "E189 generator fidelity requires 32-bit int");
    const std::uint32_t bits = static_cast<std::uint32_t>(x);
    return static_cast<std::uint64_t>(bits ^ 0x80000000u);
}

static void generate_int_exact(std::vector<int>& v, InputType t, std::mt19937& rng) {
    const std::size_t n = v.size();

    switch (t) {
        case InputType::Random: {
            std::uniform_int_distribution<int> d(
                std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            for (int& x : v) x = d(rng);
            break;
        }
        case InputType::Sorted:
            std::iota(v.begin(), v.end(), 0);
            break;
        case InputType::Reverse:
            std::iota(v.rbegin(), v.rend(), 0);
            break;
        case InputType::NearlySorted: {
            std::iota(v.begin(), v.end(), 0);
            std::bernoulli_distribution change(0.05);
            std::uniform_int_distribution<int> d(0, static_cast<int>(n) - 1);
            for (int& x : v) if (change(rng)) x = d(rng);
            break;
        }
        case InputType::SortedNoise10: {
            std::iota(v.begin(), v.end(), 0);
            std::bernoulli_distribution change(0.10);
            std::uniform_int_distribution<int> d(0, static_cast<int>(n) - 1);
            for (int& x : v) if (change(rng)) x = d(rng);
            break;
        }
        case InputType::Random25: {
            std::uniform_int_distribution<int> d(0, 24);
            for (int& x : v) x = d(rng);
            break;
        }
        case InputType::Alternating: {
            int lo = 0;
            int hi = static_cast<int>(n);
            for (std::size_t i = 0; i < n; ++i)
                v[i] = (i % 2 == 0) ? lo-- : hi++;
            break;
        }
        case InputType::Sawtooth: {
            const std::size_t period = structured_scale(n);
            for (std::size_t i = 0; i < n; ++i)
                v[i] = static_cast<int>(i % period);
            break;
        }
        case InputType::MixedDirectionRuns: {
            const std::size_t target = structured_scale(n);
            const std::size_t minLen = std::max<std::size_t>(4, target / 2);
            const std::size_t maxLen = std::max(minLen, target + target / 2);
            std::uniform_int_distribution<std::size_t> lenDist(minLen, maxLen);
            std::bernoulli_distribution descending(0.5);
            std::uniform_int_distribution<int> stepDist(1, 7);
            const long long span = std::min<long long>(
                static_cast<long long>(std::numeric_limits<int>::max()) / 8,
                std::max<long long>(1024, static_cast<long long>(n) * 8));
            std::uniform_int_distribution<long long> startDist(-span, span);

            std::size_t pos = 0;
            while (pos < n) {
                const std::size_t len = std::min(lenDist(rng), n - pos);
                const int step = stepDist(rng);
                const bool desc = descending(rng);
                long long startValue = startDist(rng);
                const long long runSpan = static_cast<long long>(len - 1) * step;
                if (desc && startValue - runSpan < std::numeric_limits<int>::min())
                    startValue = static_cast<long long>(std::numeric_limits<int>::min()) + runSpan;
                if (!desc && startValue + runSpan > std::numeric_limits<int>::max())
                    startValue = static_cast<long long>(std::numeric_limits<int>::max()) - runSpan;

                for (std::size_t k = 0; k < len; ++k) {
                    const long long delta = static_cast<long long>(k) * step;
                    v[pos + k] = static_cast<int>(
                        desc ? startValue - delta : startValue + delta);
                }
                pos += len;
            }
            break;
        }
        case InputType::BlockSorted: {
            std::iota(v.begin(), v.end(), 0);
            const std::size_t target = structured_scale(n);
            const std::size_t minLen = std::max<std::size_t>(4, target / 2);
            const std::size_t maxLen = std::max(minLen, target + target / 2);
            std::uniform_int_distribution<std::size_t> lenDist(minLen, maxLen);

            struct Block { std::size_t begin; std::size_t len; };
            std::vector<Block> blocks;
            for (std::size_t pos = 0; pos < n;) {
                const std::size_t len = std::min(lenDist(rng), n - pos);
                blocks.push_back({pos, len});
                pos += len;
            }
            std::shuffle(blocks.begin(), blocks.end(), rng);

            const auto source = v;
            std::size_t dest = 0;
            for (const Block& block : blocks) {
                std::copy_n(
                    source.begin() + static_cast<std::ptrdiff_t>(block.begin),
                    block.len,
                    v.begin() + static_cast<std::ptrdiff_t>(dest));
                dest += block.len;
            }
            break;
        }
        case InputType::OrganPipe:
            for (std::size_t i = 0; i < n; ++i)
                v[i] = static_cast<int>(i <= n / 2 ? i : n - i);
            break;
        case InputType::Rotated: {
            std::iota(v.begin(), v.end(), 0);
            std::uniform_int_distribution<std::size_t> d(0, n - 1);
            std::rotate(
                v.begin(),
                v.begin() + static_cast<std::ptrdiff_t>(d(rng)),
                v.end());
            break;
        }
    }
}

template <class F>
double timed_sort(const std::uint64_t* input, std::size_t n,
                  std::uint64_t* output, F&& f) {
    std::vector<std::uint64_t> values(input, input + n);
    const auto start = std::chrono::steady_clock::now();
    f(values);
    const auto end = std::chrono::steady_clock::now();
    std::copy(values.begin(), values.end(), output);
    return std::chrono::duration<double, std::micro>(end - start).count();
}

} // namespace

extern "C" {

void jesse_generate_u64(std::uint64_t* out, std::size_t n,
                        int input_type, unsigned seed) {
    std::mt19937 rng(seed);
    std::vector<int> signed_values(n);
    generate_int_exact(signed_values, static_cast<InputType>(input_type), rng);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = encode_int(signed_values[i]);
}

double jesse_physical_u64(const std::uint64_t* input, std::size_t n,
                          std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::actual_piles_legacy::sort(v);
    });
}

double jesse_simulated_u64(const std::uint64_t* input, std::size_t n,
                           std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::simulated_legacy::sort(v);
    });
}

double jesse_simulated_direct_u64(const std::uint64_t* input, std::size_t n,
                                  std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::simulated_direct_merge::sort(v);
    });
}

double jesse_indexed_u64(const std::uint64_t* input, std::size_t n,
                         std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::index_tail_legacy::sort(v);
    });
}

double jesse_noalloc_u64(const std::uint64_t* input, std::size_t n,
                         std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::allocation_free_v10::sort(v);
    });
}

double jesse_noalloc_low_run_u64(const std::uint64_t* input, std::size_t n,
                                 std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::allocation_free_v11::sort(v);
    });
}

}
