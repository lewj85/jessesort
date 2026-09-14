#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
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
// -> E738 large-object compact-index realization
// -> E750 selective 20% merge-boundary precompaction (target >=32).
template <class T, class Less = std::less<T>>
void sort(std::vector<T>& values, Less less = Less{}) {
    // Move-only compatibility: the allocating live-phase pipeline retains
    // copied tail/pivot values in several mature backends.  Rather than force
    // those internals to copy T, sort a compact index permutation with the
    // ordinary JesseSort pipeline and realize it using moves.  Copyable types
    // retain the existing production path unchanged.
    if constexpr (!std::is_copy_constructible_v<T> || !std::is_copy_assignable_v<T>) {
        if (values.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            std::sort(values.begin(), values.end(), less);
            return;
        }
        std::vector<std::uint32_t> order(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
            order[i] = static_cast<std::uint32_t>(i);
        const auto indirectLess = [&](std::uint32_t lhs, std::uint32_t rhs) {
            return less(values[lhs], values[rhs]);
        };
        simulated_direct_live_phase::sort(order, indirectLess);
        simulated_direct_live_phase::realizePermutationE738(values, order);
    } else {
        simulated_direct_live_phase::sort(values, less);
    }
}

} // namespace jessesort
