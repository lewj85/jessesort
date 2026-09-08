#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <jessesort/detail/pipelines/reference/strict_index_scratch.h>

namespace jessesort::adaptive_index_scratch {

template<class T, class Less = std::less<T>>
bool sort(std::vector<T>& values,
          std::vector<std::uint32_t>& scratch,
          Less less = Less{}) {
    // Preserve adaptive's retained E729 large natural-run opportunity before
    // scratch admission, then avoid repeating it on the fallback path.
    if (values.size() >= 50000 &&
        jessesort::allocation_free_live_phase_adaptive::tryLongNaturalRunDirectNoAllocE729(
            values, less))
        return false;
    if (scratch.size() >= values.size() &&
        jessesort::strict_index_scratch::shouldUseScratch(values, less))
        return jessesort::strict_index_scratch::sortUnconditional(values, scratch, less);
    jessesort::allocation_free_live_phase_adaptive::sortImpl<T, Less, true, true>(values, less);
    return false;
}

} // namespace jessesort::adaptive_index_scratch
