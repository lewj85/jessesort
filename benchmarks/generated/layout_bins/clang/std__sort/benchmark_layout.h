// GENERATED FILE. Edit benchmarks/config/*.json, not this file.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <algorithm>

namespace jessesort_benchmark_config {
struct AlgorithmSpec {
    std::string_view short_name;
    std::string_view base_pipeline;
    std::string_view header_file;
    std::string_view module_layout;
    bool default_selected;
};
inline constexpr std::string_view kLayoutSha256 = "1412b971890679e6a63b880a72833356229091e67069b9fff41ed15e18d01da0";
inline constexpr std::array<AlgorithmSpec, 1> kAlgorithmSpecs{{
    {"std::sort", "std::sort", "algorithm", "backends=std::sort", true},
}};
inline constexpr std::size_t kAlgorithmCount = kAlgorithmSpecs.size();
inline void run_algorithm(std::size_t id, std::vector<int>& values) {
    switch (id) {
        case 0: std::sort(values.begin(), values.end()); break;
        default: std::abort();
    }
}
} // namespace jessesort_benchmark_config
