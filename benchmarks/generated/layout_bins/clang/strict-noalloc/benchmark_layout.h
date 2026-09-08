// GENERATED FILE. Edit benchmarks/config/*.json, not this file.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <jessesort/jessesort_noalloc.h>

namespace jessesort_benchmark_config {
struct AlgorithmSpec {
    std::string_view short_name;
    std::string_view base_pipeline;
    std::string_view header_file;
    std::string_view module_layout;
    bool default_selected;
};
inline constexpr std::string_view kLayoutSha256 = "70a3bd95a4cc5a39768273afe745fa9e6efe3a3214d7960ee24612c7c1d55d16";
inline constexpr std::array<AlgorithmSpec, 1> kAlgorithmSpecs{{
    {"strict-noalloc", "strict-noalloc", "jessesort/jessesort_noalloc.h", "lineage=E728-strict-live-phase;E731-E729-prepass-retained;E735-strict-live-phase-production-promotion;role=strict-bounded-noalloc", true},
}};
inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();
inline void run_algorithm(std::size_t id, std::vector<int>& values) {
    switch (id) {
        case 0: jessesort::sort_unstable_noalloc(values); break;
        default: std::abort();
    }
}
} // namespace jessesort_benchmark_config
