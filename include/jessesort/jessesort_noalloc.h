#pragma once

#include <functional>
#include <cstdint>
#include <vector>
#include <jessesort/detail/pipelines/reference/noalloc_direct.h>
#include <jessesort/detail/pipelines/reference/live_phase_adaptive_noalloc.h>
#include <jessesort/detail/pipelines/reference/rust_unstable_noalloc.h>
#include <jessesort/detail/pipelines/reference/live_phase_strict_noalloc.h>
#include <jessesort/detail/pipelines/reference/strict_index_scratch.h>
#include <jessesort/detail/pipelines/reference/adaptive_index_scratch.h>

namespace jessesort {

// Allocation-free, bounded-stack, worst-case O(n log n) strict API.
// E735 promotes the strict live-phase routing/decomposition architecture and
// retains E731/E729's size-gated whole-input natural-run prepass ahead of it.
// The former E731 bounded front-end remains archived under research/experimental
// as a legacy control for recovery work.
template <class T, class Less = std::less<T>>
void sort_unstable_noalloc(std::vector<T>& values, Less less = Less{}) {
    allocation_free_live_phase_strict::sort(values, less);
}

// E740 scratch-enabled strict companion for large, movement-expensive values.
// The caller owns and sizes scratch to at least values.size() uint32_t entries.
// The call never allocates; insufficient scratch falls back to the ordinary
// strict sorter. Returns true only when compact-index realization was used.
template <class T, class Less = std::less<T>>
bool sort_unstable_noalloc_with_scratch(std::vector<T>& values,
                                        std::vector<std::uint32_t>& scratch,
                                        Less less = Less{}) {
    return strict_index_scratch::sort(values, scratch, less);
}

} // namespace jessesort

namespace jessesort::noalloc {

// Performance-oriented noalloc companion. E728 rebased this maintained path
// onto the simulated-direct_live-phase region architecture, and E729 retained
// a size-gated whole-input natural-run prepass ahead of that loop. The path
// remains allocation-free but is intentionally distinct from strict bounded
// sort_unstable_noalloc.
template <class T, class Less = std::less<T>>
void sort_adaptive(std::vector<T>& values, Less less = Less{}) {
    allocation_free_live_phase_adaptive::sort_adaptive(values, less);
}

// E741 caller-scratch counterpart to E740 for adaptive noalloc. The caller
// supplies at least values.size() uint32_t entries. Returns true only when the
// compact-index path was used; the call itself performs no allocation.
template <class T, class Less = std::less<T>>
bool sort_adaptive_with_scratch(std::vector<T>& values,
                                std::vector<std::uint32_t>& scratch,
                                Less less = Less{}) {
    return jessesort::adaptive_index_scratch::sort(values, scratch, less);
}

} // namespace jessesort::noalloc
