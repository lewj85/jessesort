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
inline constexpr std::string_view kLayoutSha256 = "ddd85e4443d17a7b57f58e7a5ed71d48cf47b488daf3a1a6ff10e5385b4cb11f";
inline constexpr std::array<AlgorithmSpec, 1> kAlgorithmSpecs{{
    {"noalloc", "performance-noalloc", "jessesort/jessesort_noalloc.h", "lineage=E728-live-phase-adaptive-noalloc;E729-global-natural-run-prepass-retained;role=performance-noalloc", true},
}};
inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();
inline void run_algorithm(std::size_t id, std::vector<int>& values) {
    switch (id) {
        case 0: jessesort::noalloc::sort_adaptive(values); break;
        default: std::abort();
    }
}
} // namespace jessesort_benchmark_config
