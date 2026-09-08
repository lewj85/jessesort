#pragma once

#include <functional>
#include <vector>
#include <jessesort/detail/pipelines/reference/phase_map_mature.h>
#include <jessesort/detail/pipelines/reference/simulated_direct_dense_ghost_e656.h>

namespace jessesort::experimental::e661_legacy {

// Preserved former public production path.
// Lineage: E601 -> E656/E659 local adaptive third-run fusion -> E661
// correctness repair. Retained as an experimental legacy speed-specialist
// reference and donor after E733 promoted simulated-direct_live-phase (E732)
// to the public/default API.
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& values, Less less = Less{}) {
    simulated_direct_phase_map_mature::sortWithFallback(values, less, [&](std::vector<T>& x) {
        simulated_direct_dense_ghost_e656_local_fusion::sort(x, less);
    });
}

} // namespace jessesort::experimental::e661_legacy
