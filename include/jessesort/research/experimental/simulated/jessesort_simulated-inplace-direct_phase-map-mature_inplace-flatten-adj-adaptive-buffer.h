#ifndef JESSESORT_SIMULATED_INPLACE_DIRECT_PHASE_MAP_MATURE_H
#define JESSESORT_SIMULATED_INPLACE_DIRECT_PHASE_MAP_MATURE_H
#include <jessesort/detail/pipelines/reference/phase_map_mature.h>
#include <jessesort/research/experimental/simulated/jessesort_simulated_direct-merge-probe-route_inplace-flatten-adj-adaptive-buffer.h>
namespace jessesort::simulated_inplace_direct_phase_map_mature {
template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    jessesort::simulated_direct_phase_map_mature::sortWithFallback(
        a, less, [&](std::vector<T>& x) { jessesort::simulated_inplace_flatten_direct_merge::sort(x, less); });
}
}
#endif
