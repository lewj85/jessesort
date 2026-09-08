#ifndef JESSESORT_PHYSICAL_DIRECT_PHASE_MAP_MATURE_H
#define JESSESORT_PHYSICAL_DIRECT_PHASE_MAP_MATURE_H
#include <jessesort/detail/pipelines/reference/phase_map_mature.h>
#include <jessesort/detail/pipelines/donors/physical_direct.h>

namespace jessesort::physical_direct_phase_map_mature {

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    constexpr std::size_t kMinMapN = 65536;
    constexpr std::size_t kComplexMapMinN = 262144; // E308: conservative 2^18 economics gate.

    auto baseSort = [&](std::vector<T>& x) {
        jessesort::actual_piles_direct_merge::sort(x, less);
    };

    if (a.size() < kMinMapN ||
        !jessesort::simulated_phase_map_range::routeDiversityGate(a, less)) {
        baseSort(a);
        return;
    }

    // E401: below the existing complex-map threshold, physical-direct only
    // admits simple phase maps. Denser leading-STRUCTURED sampling improves
    // boundary placement in that regime; at larger sizes the broader map
    // space retains the mature x2/8k schedule because denser sampling regressed
    // complex MixedPhase12 maps.
    auto map = (a.size() < kComplexMapMinN)
        ? jessesort::simulated_phase_map_range::mapX2LeadStructuredCap3k(a, less)
        : jessesort::simulated_phase_map_range::mapX2Cap8k(a, less);
    if (!jessesort::simulated_direct_phase_map_mature::semanticallyValidPhaseMap<T,Less>(map)) {
        baseSort(a);
        return;
    }

    // E307/E308 backend-aware economics: simple phase maps are consistently
    // profitable, while physical-direct can already be unusually strong on
    // many-short-phase maps at smaller sizes. Require a larger problem before
    // overriding native physical behavior for maps with >2 regions.
    if (map.states.size() > 2 && a.size() < kComplexMapMinN) {
        baseSort(a);
        return;
    }

    for (std::size_t r = 0; r + 1 < map.bounds.size(); ++r) {
        const std::size_t b = map.bounds[r], e = map.bounds[r + 1];
        jessesort::simulated_phase_range_engine::sortDirect(
            std::span<T>(a.data() + b, e - b), less);
    }
    jessesort::simulated_phase_map_range::mergeRuns(a, map.bounds, less);
}

} // namespace jessesort::physical_direct_phase_map_mature
#endif
