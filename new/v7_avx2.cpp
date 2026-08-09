#include "v7_avx2.h"
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v7_avx2() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::simulated_simd_v7::sort(values);
}
} // namespace jessesort_minimal_compile_check
