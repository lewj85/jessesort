#ifndef JESSESORT_SIMULATED_PHASE_MAP_RANGE_TRANSITION_PARTITION_BODY_ENTROPY_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_SIMULATED_PHASE_MAP_RANGE_TRANSITION_PARTITION_BODY_ENTROPY_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/experimental/jessesort_simulated_phase-map-range_probe-routed_adjacent-adaptive-buffered.h>

namespace jessesort::simulated_phase_map_range_transition_partition_body_entropy {

using jessesort::simulated_phase_map_range::RouteState;

template <class T, class Less>
bool shouldForceTransitionPartition(const std::vector<T>& a,
                                    std::size_t b,
                                    std::size_t e,
                                    RouteState stored,
                                    Less less) {
    if constexpr (!jessesort::simulated_phase_range_engine::specializedIntegralEligible<T, Less>) {
        (void)a; (void)b; (void)e; (void)stored; (void)less;
        return false;
    } else {
        if (stored != RouteState::Entropy || e <= b || e - b < 96) return false;
        const RouteState lead = jessesort::simulated_phase_map_range::stateOf(
            jessesort::simulated_phase_map_range::observe32(a, b, less));
        if (lead != RouteState::Structured) return false;

        const std::size_t len = e - b;
        const std::size_t mid = b + (len - 32) / 2;
        const std::size_t tail = e - 32;
        const RouteState middle = jessesort::simulated_phase_map_range::stateOf(
            jessesort::simulated_phase_map_range::observe32(a, mid, less));
        if (middle != RouteState::Entropy) return false;
        const RouteState last = jessesort::simulated_phase_map_range::stateOf(
            jessesort::simulated_phase_map_range::observe32(a, tail, less));
        return last != RouteState::Structured;
    }
}

template <class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    constexpr std::size_t kMinMapN=65536;
    if(a.size()<kMinMapN){jessesort::simulated_legacy::sort(a,less);return;}
    if(!jessesort::simulated_phase_map_range::routeDiversityGate(a,less)){
        jessesort::simulated_legacy::sort(a,less);return;
    }
    auto map=jessesort::simulated_phase_map_range::mapX2Cap8k(a,less);
    auto bounds=map.bounds;
    if(bounds.size()<=2){jessesort::simulated_legacy::sort(a,less);return;}

    for(std::size_t r=0;r+1<bounds.size();++r){
        const std::size_t b=bounds[r],e=bounds[r+1];
        std::span<T> chunk(a.data()+b,e-b);
        const RouteState stored = r < map.states.size() ? map.states[r] : RouteState::Other;
        if (shouldForceTransitionPartition(a,b,e,stored,less)) {
            if constexpr (jessesort::simulated_phase_range_engine::specializedIntegralEligible<T, Less>) {
                jessesort::simulated_phase_range_engine::highEntropyQuickSort(
                    chunk.data(),chunk.size(),
                    2*static_cast<int>(std::bit_width(chunk.size())),less,
                    false,T{},true,true,nullptr,true);
            } else {
                jessesort::simulated_phase_range_engine::sort(chunk,less);
            }
        } else {
            jessesort::simulated_phase_range_engine::sort(chunk,less);
        }
    }
    jessesort::simulated_phase_map_range::mergeRuns(a,bounds,less);
}

} // namespace jessesort::simulated_phase_map_range_transition_partition_body_entropy
#endif
