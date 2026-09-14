#ifndef JESSESORT_BENCH_LAYOUT_HEADER
#define JESSESORT_BENCH_LAYOUT_HEADER "../generated/benchmark_layouts.h"
#endif
#include JESSESORT_BENCH_LAYOUT_HEADER
#include "../infrastructure/benchmark_seed.h"
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
        std::cerr << "usage: benchmark-ood FAMILY [N=100000] [BLOCK_TRIALS=50] [WARMUPS=2] [TRIAL_OFFSET=0] [FORK_INDEX=0] [TOTAL_TRIALS=500]\n";
        return 2;
    }
    const std::string family_name = argv[1];
    const std::size_t n = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100000;
    const int trials = argc > 3 ? std::max(1, std::atoi(argv[3])) : 500;
    const int warmups = argc > 4 ? std::max(0, std::atoi(argv[4])) : 2;
    const int trial_offset = argc > 5 ? std::max(0, std::atoi(argv[5])) : 0;
    const int fork_index = argc > 6 ? std::max(0, std::atoi(argv[6])) : 0;
    const int total_trials = argc > 7 ? std::max(trials, std::atoi(argv[7])) : trials;
    const auto* family = find_family(family_name);
    if (!family) {
        std::cerr << "unknown OOD family: " << family_name << '\n';
        return 3;
    }

    const auto all = jessesort::bench::ood::families();
    std::size_t family_index = 0;
    while (family_index < all.size() && all[family_index].name != family_name) ++family_index;

    const auto timed_seed = [family_index](int global_trial) {
        return 0xE679000000000000ULL ^ (std::uint64_t(family_index) << 32)
             ^ (std::uint64_t(global_trial + 2) * 0x9E3779B97F4A7C15ULL);
    };
    std::vector<std::uint64_t> warmup_seeds;
    warmup_seeds.reserve(static_cast<std::size_t>(warmups));
    for (int w = 0; w < warmups; ++w) {
        std::uint64_t seed = jessesort::bench::warmup_seed64_for_fork(
            n, static_cast<std::uint64_t>(family_index), fork_index, w);
        for (;;) {
            bool collision = false;
            for (int t = 0; t < total_trials; ++t) {
                if (seed == timed_seed(t)) { collision = true; break; }
            }
            if (!collision && std::find(warmup_seeds.begin(), warmup_seeds.end(), seed) == warmup_seeds.end())
                break;
            seed = jessesort::bench::mix64(seed + 0x756ULL);
        }
        warmup_seeds.push_back(seed);
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

    std::cout << "family,n,seed,trial,algorithm,time_us\n" << std::setprecision(17);
    for (int t = 0; t < trials; ++t) {
        const int global_trial = trial_offset + t;
        const std::uint64_t seed = timed_seed(global_trial);
        std::mt19937_64 rng(seed);
        std::vector<int> source(n);
        family->generate(source, rng);
        auto values = source;
        const auto start = std::chrono::steady_clock::now();
        jessesort_benchmark_config::run_algorithm(0, values);
        const auto stop = std::chrono::steady_clock::now();
        if (!std::is_sorted(values.begin(), values.end())) {
            std::cerr << "validation failure: family=" << family_name << " trial=" << global_trial << '\n';
            return 5;
        }
        const double us = std::chrono::duration<double, std::micro>(stop - start).count();
        std::cout << family_name << ',' << n << ',' << seed << ',' << global_trial << ','
                  << jessesort_benchmark_config::kAlgorithmSpecs[0].short_name << ',' << us << '\n';
    }
    return 0;
}
