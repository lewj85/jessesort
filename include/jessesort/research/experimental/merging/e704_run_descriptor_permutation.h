#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>

#include <jessesort/detail/classification/phase_map/range_core.h>
#include <jessesort/detail/pipelines/engines/phase_range_e691_backbone.h>

namespace jessesort::experimental::e704_run_descriptor_permutation {
inline thread_local bool g_enable_t9_valve = true;

enum class ChunkType : unsigned char { MonotoneAsc, MonotoneDesc, Direct, Patience };

struct RouteStats {
    std::size_t monotoneAscRegions = 0;
    std::size_t monotoneDescRegions = 0;
    std::size_t directRegions = 0;
    std::size_t patienceRegions = 0;
    std::size_t gallopDoublings = 0;
};

inline thread_local RouteStats* g_route_stats = nullptr;
inline void setRouteStats(RouteStats* stats) { g_route_stats = stats; }

template<class T, class Less>
ChunkType classifyChunk(std::span<const T> x, Less less) {
    if (x.size() < 2) return ChunkType::Patience;
    const std::size_t probe8 = std::min<std::size_t>(8, x.size());
    bool asc = true, desc = true;
    for (std::size_t i = 1; i < probe8; ++i) {
        if (less(x[i], x[i-1])) asc = false;
        if (less(x[i-1], x[i])) desc = false;
    }
    if (probe8 >= 4 && asc && !desc) return ChunkType::MonotoneAsc;
    if (probe8 >= 4 && desc && !asc) return ChunkType::MonotoneDesc;
    if (asc && desc) return ChunkType::MonotoneAsc; // equal prefix; exact extension decides.

    // E701 classifier: tiny tails-only dual-Patience viability probe.
    // Preserve E691 local-direction assignment, but classify from total tail
    // count only.  The E700 1000-trial sweep selected M=16, k=4.
    constexpr std::size_t M = 16;
    constexpr std::size_t kPatienceTailThreshold = 4;
    const std::size_t m = std::min<std::size_t>(M, x.size());
    std::array<std::size_t, M> at{}, dt{};
    std::size_t na = 0, nd = 0;
    bool useDesc = m >= 2 && less(x[1], x[0]);

    auto ap = [&](std::size_t vi) {
        std::size_t lo = 0, hi = na;
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (less(x[vi], x[at[mid]])) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    };
    auto dp = [&](std::size_t vi) {
        std::size_t lo = 0, hi = nd;
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (less(x[dt[mid]], x[vi])) lo = mid + 1;
            else hi = mid;
        }
        return lo;
    };

    for (std::size_t i = 0; i < m; ++i) {
        if (i) {
            if (less(x[i-1], x[i])) useDesc = false;
            else if (less(x[i], x[i-1])) useDesc = true;
        }
        if (useDesc) {
            const std::size_t p = dp(i);
            if (p == nd) ++nd;
            dt[p] = i;
        } else {
            const std::size_t p = ap(i);
            if (p == na) ++na;
            at[p] = i;
        }
    }
    return (na + nd) <= kPatienceTailThreshold ? ChunkType::Patience
                                                : ChunkType::Direct;
}

template<class T, class Less>
std::size_t extendAscending(std::span<T> a, std::size_t i, Less less) {
    std::size_t j = i + 1;
    while (j < a.size() && !less(a[j], a[j-1])) ++j;
    return j;
}

template<class T, class Less>
std::size_t extendDescending(std::span<T> a, std::size_t i, Less less) {
    std::size_t j = i + 1;
    while (j < a.size() && !less(a[j-1], a[j])) ++j;
    return j;
}

template<class T, class Less>
void directSort(std::span<T> a, Less less) {
    if (a.size() < 2) return;
    if constexpr (std::is_default_constructible_v<T>) {
        jessesort::simulated_phase_range_engine_e691::highEntropyQuickSort(
            a.data(), a.size(), 2 * static_cast<int>(std::bit_width(a.size())), less,
            false, T{}, false, true, nullptr, false);
    } else {
        std::sort(a.begin(), a.end(), less);
    }
}

template<class T, class Less>
bool patienceWithPressure(std::span<T> a, Less less, bool enableValve=true) {
    using namespace jessesort::simulated_phase_range_engine_e691;
    if (a.size() < 2) return false;

    SimulatedInsertionResult<T> sim;
    sim.blueprint.resize(a.size());
    const std::size_t reservePiles = estimatePileReserve(a.size());
    sim.ascCounts.reserve(reservePiles); sim.descCounts.reserve(reservePiles);
    sim.ascTails.reserve(reservePiles); sim.descTails.reserve(reservePiles);

    std::size_t lastAsc=0, lastDesc=0;
    bool descMode=false;
    sim.ascTails.push_back(a[0]); sim.ascCounts.push_back(1);
    sim.blueprint[0] = makeAscTag(0);

    std::uint32_t ascBits=0, descBits=0;
    std::size_t ascSeen=0, descSeen=0;
    std::size_t prefixLen=a.size();
    bool tripped=false;

    for (std::size_t i=1;i<a.size();++i) {
        const T& prev=a[i-1]; const T& value=a[i];
        if (less(prev,value)) descMode=false;
        else if (less(value,prev)) descMode=true;
        else {
            simulateInsertAdjacentEquivalent(sim, descMode, lastAsc, lastDesc, i);
            if (descMode) { descBits <<= 1; ++descSeen; }
            else { ascBits <<= 1; ++ascSeen; }
            continue;
        }

        bool created=false;
        if (descMode) {
            const std::size_t before=sim.descCounts.size();
            simulateInsertValueDescendingPiles(sim.descTails,lastDesc,value,i,sim.blueprint,sim.descCounts,less);
            created=sim.descCounts.size()>before;
            descBits=(descBits<<1)|(created?1u:0u); ++descSeen;
            if (enableValve && sim.descCounts.size()>=32 && descSeen>=32 && std::popcount(descBits)>=9) { prefixLen=i+1; tripped=true; break; }
        } else {
            const std::size_t before=sim.ascCounts.size();
            simulateInsertValueAscendingPiles(sim.ascTails,lastAsc,value,i,sim.blueprint,sim.ascCounts,less);
            created=sim.ascCounts.size()>before;
            ascBits=(ascBits<<1)|(created?1u:0u); ++ascSeen;
            if (enableValve && sim.ascCounts.size()>=32 && ascSeen>=32 && std::popcount(ascBits)>=9) { prefixLen=i+1; tripped=true; break; }
        }
    }

    sim.blueprint.resize(prefixLen);
    std::vector<T> prefixTmp;
    auto runStart = reconstructTaggedBlueprintNormalizedForSimulated(
        std::span<const T>(a.data(), prefixLen), std::move(sim.blueprint),
        std::move(sim.ascCounts), std::move(sim.descCounts), prefixTmp,
        false,true,true,true);

    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) tmp.resize(a.size());
    else tmp.assign(a.begin(),a.end());
    std::move(prefixTmp.begin(),prefixTmp.end(),tmp.begin());

    std::vector<std::size_t> ends(runStart.begin()+1,runStart.end());
    if (tripped && prefixLen < a.size()) {
        auto suffix=a.subspan(prefixLen);
        directSort(suffix,less);
        std::move(suffix.begin(),suffix.end(),tmp.begin()+static_cast<std::ptrdiff_t>(prefixLen));
        ends.push_back(a.size());
    }
    mergeRunsAdjacentPairsEndsToSpan(tmp,a,ends,less,false,true);
    return tripped;
}

template<class T, class Less>
void patienceOnly(std::span<T> a, Less less) {
    (void)patienceWithPressure(a,less,g_enable_t9_valve);
}

template<class T, class Less>
bool tryRunDescriptorPermutation(std::vector<T>& values,
                                 const std::vector<std::size_t>& bounds,
                                 Less less) {
    const std::size_t runs = bounds.size() > 0 ? bounds.size() - 1 : 0;
    if (runs < 2) return true;

    // E704: this is a low-run-density exact fast path, not a classifier.
    // Keep descriptor work bounded even on pathological phase fragmentation.
    const std::size_t n = values.size();
    const std::size_t densityCap = std::max<std::size_t>(32, n / 100); // <= ~1% of n, floor 32
    if (runs > densityCap) return false;

    struct RunDesc { std::size_t begin, end; };
    std::vector<RunDesc> order;
    order.reserve(runs);
    for (std::size_t r = 0; r < runs; ++r) order.push_back({bounds[r], bounds[r+1]});

    std::sort(order.begin(), order.end(), [&](const RunDesc& lhs, const RunDesc& rhs) {
        const T& a = values[lhs.begin];
        const T& b = values[rhs.begin];
        if (less(a, b)) return true;
        if (less(b, a)) return false;
        // Deterministic tie only; correctness is established by the interval proof below.
        return lhs.begin < rhs.begin;
    });

    // Exact proof: after ordering by each run's minimum/front, every previous
    // maximum/end must be <= the next minimum/front. If any intervals overlap,
    // refuse the shortcut and leave the ordinary E701 merge path untouched.
    for (std::size_t r = 1; r < runs; ++r) {
        const T& prevMax = values[order[r-1].end - 1];
        const T& curMin = values[order[r].begin];
        if (less(curMin, prevMax)) return false;
    }

    bool identity = true;
    for (std::size_t r = 0; r < runs; ++r) {
        if (order[r].begin != bounds[r]) { identity = false; break; }
    }
    if (identity) return true;

    std::vector<T> tmp;
    tmp.reserve(n);
    for (const auto& run : order) {
        tmp.insert(tmp.end(),
                   std::make_move_iterator(values.begin() + static_cast<std::ptrdiff_t>(run.begin)),
                   std::make_move_iterator(values.begin() + static_cast<std::ptrdiff_t>(run.end)));
    }
    std::move(tmp.begin(), tmp.end(), values.begin());
    return true;
}

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& values, Less less=Less{}) {
    const std::size_t n = values.size();
    if (n < 2) return;
    std::span<T> a(values.data(), values.size());
    constexpr std::size_t kMinGhost = 32;
    constexpr std::size_t kInitialStep = 1024;
    constexpr std::size_t kMaxStep = 8192;

    std::vector<std::size_t> bounds;
    bounds.reserve(n / kInitialStep + 8);
    bounds.push_back(0);

    std::size_t i = 0;
    // Initial whole-input natural-run precheck. Determine direction from the
    // first unequal edge, then extend only that direction.
    std::size_t q = 1;
    while (q < n && !less(a[q-1],a[q]) && !less(a[q],a[q-1])) ++q;
    if (q == n) return;
    const bool initialDesc = less(a[q], a[q-1]);
    const std::size_t initialEnd = initialDesc ? extendDescending(a,0,less)
                                                : extendAscending(a,0,less);
    if (initialEnd == n) {
        if (initialDesc) std::reverse(a.begin(), a.end());
        return;
    }
    if (initialEnd >= kMinGhost) {
        if (initialDesc) std::reverse(a.begin(), a.begin()+static_cast<std::ptrdiff_t>(initialEnd));
        i = initialEnd;
        bounds.push_back(i);
        if (g_route_stats) {
            if (initialDesc) ++g_route_stats->monotoneDescRegions;
            else ++g_route_stats->monotoneAscRegions;
        }
    }

    while (i < n) {
        const std::size_t firstEnd = std::min(n, i + kInitialStep);
        ChunkType type = classifyChunk<T,Less>(
            std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(i), firstEnd-i), less);
        std::size_t actualEnd = firstEnd;

        if (type == ChunkType::MonotoneAsc) {
            const std::size_t runEnd = extendAscending(a, i, less);
            if (runEnd - i >= kMinGhost) {
                actualEnd = runEnd;
                if (g_route_stats) ++g_route_stats->monotoneAscRegions;
            } else {
                // A tiny monotone prefix is weak evidence. Treat it as part of a
                // normal Patience region instead of manufacturing a run boundary.
                type = ChunkType::Patience;
            }
        } else if (type == ChunkType::MonotoneDesc) {
            const std::size_t runEnd = extendDescending(a, i, less);
            if (runEnd - i >= kMinGhost) {
                std::reverse(a.begin()+static_cast<std::ptrdiff_t>(i),
                             a.begin()+static_cast<std::ptrdiff_t>(runEnd));
                actualEnd = runEnd;
                if (g_route_stats) ++g_route_stats->monotoneDescRegions;
            } else {
                type = ChunkType::Patience;
            }
        }

        if (type == ChunkType::Direct || type == ChunkType::Patience) {
            // Discover the whole coherent region first. Do not sort each probe
            // chunk independently; that would turn phase discovery into a merge
            // tax on homogeneous Random/Alternating inputs.
            std::size_t regionEnd = firstEnd;
            std::size_t probeStep = kInitialStep;
            while (regionEnd < n) {
                probeStep = std::min(kMaxStep, probeStep * 2);
                const std::size_t probeEnd = std::min(n, regionEnd + probeStep);
                const ChunkType nextType = classifyChunk<T,Less>(
                    std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(regionEnd), probeEnd-regionEnd), less);
                if (nextType != type) {
                    // Do not let one anomalous tiny probe fracture an otherwise
                    // coherent region. A route transition must persist at two
                    // additional forward probes before it becomes a boundary.
                    bool persistent = true;
                    constexpr std::size_t kConfirmStride = 256;
                    for (std::size_t c = 1; c <= 2; ++c) {
                        const std::size_t cs = regionEnd + c * kConfirmStride;
                        if (cs >= n) { persistent = false; break; }
                        const std::size_t ce = std::min(n, cs + kInitialStep);
                        const ChunkType ct = classifyChunk<T,Less>(
                            std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(cs), ce-cs), less);
                        if (ct != nextType) { persistent = false; break; }
                    }
                    if (persistent) break;
                }
                regionEnd = probeEnd;
                if (g_route_stats) ++g_route_stats->gallopDoublings;
            }
            actualEnd = regionEnd;
            auto region = std::span<T>(a.data()+static_cast<std::ptrdiff_t>(i), actualEnd-i);
            if (type == ChunkType::Direct) {
                directSort(region, less);
                if (g_route_stats) ++g_route_stats->directRegions;
            } else {
                patienceOnly(region, less);
                if (g_route_stats) ++g_route_stats->patienceRegions;
            }
        }

        i = actualEnd;
        if (bounds.back() != i) bounds.push_back(i);
    }

    if (bounds.back() != n) bounds.push_back(n);
    if (!tryRunDescriptorPermutation(values, bounds, less))
        jessesort::simulated_phase_map_range::mergeRuns(values, bounds, less);
}

} // namespace jessesort::experimental::e704_run_descriptor_permutation
