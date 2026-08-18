#include <jessesort/jessesort_simulated-frozen_probe-routed_adjacent-timsort-adaptive-buffered_live-bands32.h>
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v6_live_bands_legacy() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_early_freeze_live_legacy::sort(values);
}
} // namespace jessesort_minimal_compile_check
