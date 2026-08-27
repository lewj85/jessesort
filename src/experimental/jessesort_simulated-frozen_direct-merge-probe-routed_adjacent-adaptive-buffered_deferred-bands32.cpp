#include <jessesort/experimental/jessesort_simulated-frozen_direct-merge-probe-routed_adjacent-adaptive-buffered_deferred-bands32.h>
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v5_deferred_bands_direct() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_early_freeze_direct_merge::sort(values);
}
} // namespace jessesort_minimal_compile_check
