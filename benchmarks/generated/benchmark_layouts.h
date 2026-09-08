// GENERATED FILE. Edit benchmarks/config/*.json, not this file.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <jessesort/jessesort_backbone.h>
#include <jessesort/jessesort_noalloc.h>
#include <algorithm>

namespace jessesort_benchmark_config {
struct AlgorithmSpec {
    std::string_view short_name;
    std::string_view base_pipeline;
    std::string_view header_file;
    std::string_view module_layout;
    bool default_selected;
};
inline constexpr std::string_view kLayoutSha256 = "783ceede4e958ef31b5a026c03678618e542ac23cc1418b166d64fffa791c074";
inline constexpr std::array<AlgorithmSpec, 4> kAlgorithmSpecs{{
    {"simulated-direct_live-phase", "simulated-direct_live-phase", "jessesort/jessesort_backbone.h", "lineage=E691\u2192E701\u2192E706\u2192E723\u2192E726\u2192E732\u2192E733-public-promotion;role=public-default-reference", true},
    {"noalloc", "performance-noalloc", "jessesort/jessesort_noalloc.h", "lineage=E728-live-phase-adaptive-noalloc;E729-global-natural-run-prepass-retained;role=performance-noalloc", true},
    {"strict-noalloc", "strict-noalloc", "jessesort/jessesort_noalloc.h", "lineage=E728-strict-live-phase;E731-E729-prepass-retained;E735-strict-live-phase-production-promotion;role=strict-bounded-noalloc", true},
    {"std::sort", "std::sort", "algorithm", "backends=std::sort", true},
}};
inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();
inline void run_algorithm(std::size_t id, std::vector<int>& values) {
    switch (id) {
        case 0: jessesort::backbone::sort(values); break;
        case 1: jessesort::noalloc::sort_adaptive(values); break;
        case 2: jessesort::sort_unstable_noalloc(values); break;
        case 3: std::sort(values.begin(), values.end()); break;
        default: std::abort();
    }
}
} // namespace jessesort_benchmark_config
