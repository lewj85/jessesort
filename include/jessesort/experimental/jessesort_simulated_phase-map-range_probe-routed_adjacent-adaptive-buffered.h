#ifndef JESSESORT_SIMULATED_PHASE_MAP_RANGE_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_SIMULATED_PHASE_MAP_RANGE_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/jessesort_simulated_probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/detail_phase_map_range_core.h>
#include <jessesort/detail_phase_range_engine.h>
#include <span>

namespace jessesort::simulated_phase_map_range {

template <class T, class Less=std::less<T>>
void sort(std::vector<T>& a,Less less=Less{}){
    constexpr std::size_t kMinMapN=65536;
    if(a.size()<kMinMapN){jessesort::simulated_legacy::sort(a,less);return;}
    if(!routeDiversityGate(a,less)){jessesort::simulated_legacy::sort(a,less);return;}
    PhaseMap map=mapX2Cap8k(a,less); auto bounds=map.bounds;
    if(bounds.size()<=2){jessesort::simulated_legacy::sort(a,less);return;}
    for(std::size_t r=0;r+1<bounds.size();++r){
        const std::size_t b=bounds[r],e=bounds[r+1];
        std::span<T> chunk(a.data()+b,e-b);
        jessesort::simulated_phase_range_engine::sort(chunk,less);
    }
    mergeRuns(a,bounds,less);
}


} // namespace jessesort::simulated_phase_map_range
#endif
