#include <jessesort/experimental/jessesort_simulated_probe-first_direct-merge-routed_adjacent-adaptive-buffered.h>
#include <vector>
namespace jessesort_minimal_compile_check {
void simulated_probe_first_direct() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_probe_first_direct::sort(values);
}
}
