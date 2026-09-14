#ifndef JESSESORT_BENCH_LAYOUT_HEADER
#error JESSESORT_BENCH_LAYOUT_HEADER required
#endif
#include JESSESORT_BENCH_LAYOUT_HEADER
#include "benchmark_seed.h"
#include "input_generators.h"
#include "../ood/ood_families.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;
namespace inputs = jessesort::bench::inputs;

struct CanonicalSpec { const char* name; int ordinal; };
static constexpr CanonicalSpec kCanonical[] = {
    {"Random",0},{"Sorted",1},{"Reverse",2},{"Sorted+Noise(5%)",3},{"Random%25",4},
    {"Diverging",5},{"Sawtooth",6},{"MixedDirectionRuns",13},{"OrganPipe",8},{"Rotated",9}
};

static bool generate_canonical(std::vector<int>& v, const std::string& name, std::mt19937& rng, int& ordinal) {
    for (const auto& c : kCanonical) {
        if (name != c.name) continue;
        ordinal = c.ordinal;
        switch (ordinal) {
            case 0: inputs::random(v,rng); break;
            case 1: inputs::sorted(v); break;
            case 2: inputs::reverse(v); break;
            case 3: inputs::sorted_noise(v,rng,0.05); break;
            case 4: inputs::random_cardinality(v,rng,25); break;
            case 5: inputs::diverging(v); break;
            case 6: inputs::sawtooth(v); break;
            case 13: inputs::mixed_direction_runs(v,rng); break;
            case 8: inputs::organ_pipe(v); break;
            case 9: inputs::rotated(v,rng); break;
            default: return false;
        }
        return true;
    }
    return false;
}

static bool make_input(std::vector<int>& out, const std::string& suite, const std::string& name,
                       std::size_t n, int trial, int total, std::uint64_t& seed64) {
    if (trial < 0 || total <= 0 || trial >= total) return false;
    out.resize(n);
    if (suite == "canonical") {
        int ordinal=-1;
        std::mt19937 probe(0);
        if (!generate_canonical(out,name,probe,ordinal)) return false;
        const unsigned seed=jessesort::bench::trial_seed(jessesort::bench::kCanonicalBaseSeed,n,ordinal,trial);
        seed64=seed;
        std::mt19937 rng(seed);
        return generate_canonical(out,name,rng,ordinal);
    }
    if (suite == "ood") {
        const auto fams=jessesort::bench::ood::families();
        std::size_t fi=0; while (fi<fams.size() && fams[fi].name!=name) ++fi;
        if (fi==fams.size()) return false;
        seed64=0xE679000000000000ULL ^ (std::uint64_t(fi)<<32)
             ^ (std::uint64_t(trial+2)*0x9E3779B97F4A7C15ULL);
        std::mt19937_64 rng(seed64);
        fams[fi].generate(out,rng);
        return true;
    }
    return false;
}

int main(int argc, char** argv) {
    if (jessesort_benchmark_config::kAlgorithmCount != 1) return 2;
    if (argc != 10) {
        std::cerr << "usage: five_trial canonical|ood INPUT N TOTAL_TRIALS T0 T1 T2 T3 T4\n";
        return 2;
    }
    const std::string suite=argv[1], name=argv[2];
    const std::size_t n=std::strtoull(argv[3],nullptr,10);
    const int total=std::atoi(argv[4]);
    std::array<int,5> trials{};
    for (int i=0;i<5;++i) trials[i]=std::atoi(argv[5+i]);

    // Construct every source and timed working buffer before the first timing.
    // No input generation, allocation, correctness sorting, or target-sort warmup
    // occurs between the five timed calls.
    std::array<std::vector<int>,5> sources;
    std::array<std::vector<int>,5> values;
    std::array<std::uint64_t,5> seeds{};
    for (int i=0;i<5;++i) {
        if (!make_input(sources[i],suite,name,n,trials[i],total,seeds[i])) return 4;
        values[i]=sources[i];
    }

    std::array<double,5> us{};
    for (int i=0;i<5;++i) {
        const auto start=Clock::now();
        jessesort_benchmark_config::run_algorithm(0,values[i]);
        const auto stop=Clock::now();
        us[i]=std::chrono::duration<double,std::micro>(stop-start).count();
    }

    // Correctness work deliberately happens only after all five timed calls so
    // it cannot condition later timing positions.
    for (int i=0;i<5;++i) {
        auto expected=sources[i];
        std::sort(expected.begin(),expected.end());
        if (values[i]!=expected) return 5;
    }

    for (int i=0;i<5;++i) {
        std::cout << i << ',' << name << ',' << n << ',' << seeds[i] << ',' << trials[i] << ','
                  << jessesort_benchmark_config::kAlgorithmSpecs[0].short_name << ','
                  << std::setprecision(17) << us[i] << '\n';
    }
    return 0;
}
