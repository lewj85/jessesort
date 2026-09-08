#ifndef JESSESORT_LINKED_DIRECT_PHASE_MAP_MATURE_H
#define JESSESORT_LINKED_DIRECT_PHASE_MAP_MATURE_H
#include <jessesort/detail/pipelines/reference/phase_map_mature.h>
#include <jessesort/research/experimental/linked/jessesort_linked_direct-merge-probe-route_linked-fused-adj-adaptive-buffer.h>
namespace jessesort::linked_direct_phase_map_mature {
template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    jessesort::simulated_direct_phase_map_mature::sortWithFallback(
        a, less, [&](std::vector<T>& x) { jessesort::linked_direct_merge::sort(x, less); });
}
}
#endif
