#ifndef JESSESORT_LINKED_DIRECT_PHASE_MAP_MATURE_H
#define JESSESORT_LINKED_DIRECT_PHASE_MAP_MATURE_H
#include <jessesort/jessesort_simulated-direct_phase-map-mature_probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/experimental/jessesort_linked_direct-merge-probe-routed_linked-fused-adjacent-adaptive-buffered.h>
namespace jessesort::linked_direct_phase_map_mature {
template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    jessesort::simulated_direct_phase_map_mature::sortWithFallback(
        a, less, [&](std::vector<T>& x) { jessesort::linked_direct_merge::sort(x, less); });
}
}
#endif
