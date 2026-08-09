#include "v1_actual_piles.h"
#include <vector>

// This translation unit intentionally contains no algorithm implementation.
// JesseSort is generic/template-based, so the implementation remains in the header.
// This function forces an int instantiation during a normal multi-file build.
namespace jessesort_minimal_compile_check {
void v1_actual_piles() {
    std::vector<int> values{3, 1, 2, 1};
    jessesort::actual_piles::sort(values);
}
} // namespace jessesort_minimal_compile_check
