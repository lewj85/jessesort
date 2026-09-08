#pragma once

#include <functional>
#include <vector>
#include <jessesort/detail/pipelines/reference/simulated-direct_live-phase.h>

namespace jessesort {

// Public/default allocating JesseSort.
// E733 promoted the E732 simulated-direct live-phase lineage over the former
// E661 phase-map/local-fusion production path after a same-binary paired
// canonical + expanded-OOD closeout audit. E661 remains available under
// research/experimental/e661_legacy_production.h for provenance, specialist
// performance comparisons, and donor work.
//
// Current lineage:
// E691 architecture -> E701 classifier -> E706 endpoint shortcuts
// -> E723 geometry-gated tier-0 gallop
// -> E726 middle-density sparse-disorder bypass
// -> E732 global natural-run prepass
// -> E733 public/default promotion
// -> E738 large-object compact-index realization.
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& values, Less less = Less{}) {
    simulated_direct_live_phase::sort(values, less);
}

} // namespace jessesort
