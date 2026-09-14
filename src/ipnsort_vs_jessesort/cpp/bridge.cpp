#include <jessesort/jessesort.h>
#include <jessesort/jessesort_noalloc.h>
#include "input_generators.h"
#include "ood_families.h"

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
    Diverging = 5,
    Sawtooth = 6,
    OrganPipe = 8,
    Rotated = 9,
    MixedDirectionRuns = 13
};

// Order-preserving map from signed int32 ordering into unsigned u64 ordering.
// For any int32 a,b: a < b iff encode(a) < encode(b), and equality is preserved.
static std::uint64_t encode_int(int x) {
    static_assert(sizeof(int) == 4, "benchmark generator fidelity requires 32-bit int");
    const std::uint32_t bits = static_cast<std::uint32_t>(x);
    return static_cast<std::uint64_t>(bits ^ 0x80000000u);
}

static void generate_int_exact(std::vector<int>& v, InputType t, std::mt19937& rng) {
    namespace inputs = jessesort::bench::inputs;
    switch (t) {
        case InputType::Random: inputs::random(v, rng); break;
        case InputType::Sorted: inputs::sorted(v); break;
        case InputType::Reverse: inputs::reverse(v); break;
        case InputType::NearlySorted: inputs::sorted_noise(v, rng, 0.05); break;
        case InputType::Random25: inputs::random_cardinality(v, rng, 25); break;
        case InputType::Diverging: inputs::diverging(v); break;
        case InputType::Sawtooth: inputs::sawtooth(v); break;
        case InputType::MixedDirectionRuns: inputs::mixed_direction_runs(v, rng); break;
        case InputType::OrganPipe: inputs::organ_pipe(v); break;
        case InputType::Rotated: inputs::rotated(v, rng); break;
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

std::size_t jesse_ood_family_count() {
    static const auto families = jessesort::bench::ood::families();
    return families.size();
}

const char* jesse_ood_family_name(std::size_t family_index) {
    static const auto families = jessesort::bench::ood::families();
    return family_index < families.size() ? families[family_index].name.c_str() : nullptr;
}

void jesse_generate_ood_u64(std::uint64_t* out, std::size_t n,
                            std::size_t family_index, std::uint64_t seed) {
    static const auto families = jessesort::bench::ood::families();
    if (family_index >= families.size()) return;

    std::mt19937_64 rng(seed);
    std::vector<int> signed_values(n);
    families[family_index].generate(signed_values, rng);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = encode_int(signed_values[i]);
}

double jesse_production_u64(const std::uint64_t* input, std::size_t n,
                            std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::sort(v);
    });
}

double jesse_noalloc_u64(const std::uint64_t* input, std::size_t n,
                         std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::noalloc::sort_adaptive(v);
    });
}

double jesse_strict_noalloc_u64(const std::uint64_t* input, std::size_t n,
                                std::uint64_t* output) {
    return timed_sort(input, n, output, [](auto& v) {
        jessesort::sort_unstable_noalloc(v);
    });
}

}
