#ifndef JESSESORT_SIMULATED_DIRECT_PHASE_MAP_MATURE_H
#define JESSESORT_SIMULATED_DIRECT_PHASE_MAP_MATURE_H
#include <jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h>
#include <jessesort/detail_phase_map_range_core.h>
#include <jessesort/detail_phase_range_engine.h>
#include <span>

namespace jessesort::simulated_direct_phase_map_mature {

template<class T, class Less>
bool semanticallyValidPhaseMap(const jessesort::simulated_phase_map_range::PhaseMap& m) {
    if (m.bounds.size() <= 2 || m.states.size() + 1 != m.bounds.size()) return false;
    for (std::size_t i = 1; i < m.states.size(); ++i)
        if (m.states[i] == m.states[i - 1]) return false;
    return true;
}

template<class T, class Less>
bool economicallyAcceptPhaseMap(const jessesort::simulated_phase_map_range::PhaseMap& m, std::size_t n) {
    using RouteState = jessesort::simulated_phase_map_range::RouteState;
    if (m.states.empty() || m.states.size() + 1 != m.bounds.size() || n == 0) return false;

    // E315: E314's route-composition economics were learned and independently
    // validated only for small integral values with the default comparator.
    // Type-domain testing shows mapped-vs-native economics are not monotonic in
    // sizeof(T), so do not export that u64/int policy as a generic type law.
    // Unvalidated type/comparator domains retain the pre-E314 mature behavior:
    // execute any semantically valid map.
    constexpr bool kE314ValidatedDomain =
        std::is_integral_v<T> && sizeof(T) <= 8 &&
        std::is_same_v<std::remove_cvref_t<Less>, std::less<T>>;
    if constexpr (!kE314ValidatedDomain) return true;

    std::size_t structuredValues = 0;
    std::size_t entropyValues = 0;
    for (std::size_t r = 0; r < m.states.size(); ++r) {
        const std::size_t len = m.bounds[r + 1] - m.bounds[r];
        if (m.states[r] == RouteState::Structured) structuredValues += len;
        else if (m.states[r] == RouteState::Entropy) entropyValues += len;
    }

    // E314: broad held-out mixed-input validation favored an acceptance model
    // based on measured route composition rather than phase count or benchmark
    // identity. A structured-leading map is directly useful when it transitions
    // away from structure. Otherwise require the map to be structurally dominant
    // enough that local range sorting can plausibly amortize final run merging.
    const bool structuredLeadingTransition =
        m.states.front() == RouteState::Structured &&
        m.states.back() != RouteState::Structured;
    const bool structuredDominant =
        structuredValues * 100 >= n * 55 && entropyValues * 100 <= n * 30;
    return structuredLeadingTransition || structuredDominant;
}

template<class T, class Less, class BaseSort>
void sortWithFallback(std::vector<T>& a, Less less, BaseSort&& baseSort) {
    constexpr std::size_t kMinMapN=65536;
    if(a.size()<kMinMapN || !jessesort::simulated_phase_map_range::routeDiversityGate(a,less)) {
        baseSort(a); return;
    }
    auto map=jessesort::simulated_phase_map_range::mapX2LeadStructuredCap3k(a,less);
    if(!semanticallyValidPhaseMap<T,Less>(map) ||
       !economicallyAcceptPhaseMap<T,Less>(map, a.size())) {
        baseSort(a); return;
    }
    for(std::size_t r=0;r+1<map.bounds.size();++r){
        const std::size_t b=map.bounds[r],e=map.bounds[r+1];
        jessesort::simulated_phase_range_engine::sortDirect(
            std::span<T>(a.data()+b,e-b),less);
    }
    jessesort::simulated_phase_map_range::mergeRuns(a,map.bounds,less);
}

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    sortWithFallback(a, less, [&](std::vector<T>& x) {
        jessesort::simulated_direct_merge::sort(x, less);
    });
}

} // namespace
#endif
