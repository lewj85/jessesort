#pragma once

#include <functional>
#include <vector>
#include <jessesort/detail/pipelines/reference/simulated-direct_live-phase.h>

namespace jessesort::backbone {

// Maintained simulated-direct live-phase reference entry point.
// E733 promoted this same E732 pipeline to the public/default jessesort::sort;
// the backbone namespace remains as a stable explicit reference API.
// Lineage: E691 architecture -> E701 classifier -> E706 endpoint shortcuts
//          -> E723 geometry-gated tier-0 gallop
//          -> E726 middle-density sparse-disorder bypass
//          -> E732 global natural-run prepass
//          -> E738 large-object compact-index realization.
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& values, Less less = Less{}) {
    jessesort::simulated_direct_live_phase::sort(values, less);
}

} // namespace jessesort::backbone
