#include <jessesort/experimental/jessesort_simulated-avx2_direct-merge-probe-routed_adjacent-adaptive-buffered_capped-probe8x128.h>
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v7_avx2_direct() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_avx2_direct_merge::sort(values);
}
} // namespace jessesort_minimal_compile_check
