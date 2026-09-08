#ifndef JESSESORT_BENCH_LAYOUT_HEADER
#define JESSESORT_BENCH_LAYOUT_HEADER "../generated/benchmark_layouts.h"
#endif
#include JESSESORT_BENCH_LAYOUT_HEADER
#include "ood_families.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const std::size_t m = values.size() / 2;
    return values.size() % 2 ? values[m] : (values[m - 1] + values[m]) / 2.0;
}

const jessesort::bench::ood::Family* find_family(const std::string& name) {
    static const auto all = jessesort::bench::ood::families();
    for (const auto& family : all) if (family.name == name) return &family;
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--list-families") {
        for (const auto& family : jessesort::bench::ood::families()) std::cout << family.name << '\n';
        return 0;
    }
    if (jessesort_benchmark_config::kAlgorithmCount != 1) {
        std::cerr << "OOD performance benchmark requires a single-layout binary.\n"
                     "Use benchmarks/ood/run.sh or benchmarks/canonical/build_layout_binaries.sh.\n";
        return 2;
    }
    if (argc < 2) {
        std::cerr << "usage: benchmark-ood FAMILY [N=100000] [TRIALS=500] [WARMUPS=2]\n";
        return 2;
    }
    const std::string family_name = argv[1];
    const std::size_t n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100000;
    const int trials = argc > 3 ? std::max(1, std::atoi(argv[3])) : 500;
    const int warmups = argc > 4 ? std::max(0, std::atoi(argv[4])) : 2;
    const auto* family = find_family(family_name);
    if (!family) {
        std::cerr << "unknown OOD family: " << family_name << '\n';
        return 3;
    }

    const auto all = jessesort::bench::ood::families();
    std::size_t family_index = 0;
    while (family_index < all.size() && all[family_index].name != family_name) ++family_index;

    for (int w = 0; w < warmups; ++w) {
        const std::uint64_t seed = 0xE679100000000000ULL ^ (std::uint64_t(family_index) << 32) ^ std::uint64_t(w + 1);
        std::mt19937_64 rng(seed);
        std::vector<int> source(n);
        family->generate(source, rng);
        auto values = source;
        jessesort_benchmark_config::run_algorithm(0, values);
        if (!std::is_sorted(values.begin(), values.end())) {
            std::cerr << "warmup validation failure: " << family_name << '\n';
            return 4;
        }
    }

    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(trials));
    for (int t = 0; t < trials; ++t) {
        const std::uint64_t seed = 0xE679000000000000ULL ^ (std::uint64_t(family_index) << 32)
                                 ^ (std::uint64_t(t + 2) * 0x9E3779B97F4A7C15ULL);
        std::mt19937_64 rng(seed);
        std::vector<int> source(n);
        family->generate(source, rng);
        auto values = source;
        const auto start = std::chrono::steady_clock::now();
        jessesort_benchmark_config::run_algorithm(0, values);
        const auto stop = std::chrono::steady_clock::now();
        if (!std::is_sorted(values.begin(), values.end())) {
            std::cerr << "validation failure: family=" << family_name << " trial=" << t << '\n';
            return 5;
        }
        times.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
    }

    std::cout << std::setprecision(17)
              << family_name << ',' << n << ',' << trials << ','
              << jessesort_benchmark_config::kAlgorithmSpecs[0].short_name << ','
              << median(times) << '\n';
    return 0;
}
