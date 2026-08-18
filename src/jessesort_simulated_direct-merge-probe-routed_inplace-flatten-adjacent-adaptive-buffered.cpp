#include <jessesort/jessesort_simulated_direct-merge-probe-routed_inplace-flatten-adjacent-adaptive-buffered.h>
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v3_inplace_simulated_direct() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_inplace_flatten_direct_merge::sort(values);
}
} // namespace jessesort_minimal_compile_check
