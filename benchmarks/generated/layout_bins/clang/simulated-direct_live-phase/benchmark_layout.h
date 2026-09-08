// GENERATED FILE. Edit benchmarks/config/*.json, not this file.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <jessesort/jessesort_backbone.h>

namespace jessesort_benchmark_config {
struct AlgorithmSpec {
    std::string_view short_name;
    std::string_view base_pipeline;
    std::string_view header_file;
    std::string_view module_layout;
    bool default_selected;
};
inline constexpr std::string_view kLayoutSha256 = "defeb3b4d63020242145e437c92b31abe2b97c0e67d426540563eee8b3c3e6b6";
inline constexpr std::array<AlgorithmSpec, 1> kAlgorithmSpecs{{
    {"simulated-direct_live-phase", "simulated-direct_live-phase", "jessesort/jessesort_backbone.h", "lineage=E691\u2192E701\u2192E706\u2192E723\u2192E726\u2192E732\u2192E733-public-promotion;role=public-default-reference", true},
}};
inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();
inline void run_algorithm(std::size_t id, std::vector<int>& values) {
    switch (id) {
        case 0: jessesort::backbone::sort(values); break;
        default: std::abort();
    }
}
} // namespace jessesort_benchmark_config
