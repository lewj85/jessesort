#pragma once

// JesseSort simulated-direct live-phase production candidate.
//
// Unlike the earlier phase-map pipeline, phase boundaries are discovered,
// extended, classified, and routed locally during execution rather than
// precomputed as one whole-input map.
//
// Lineage: E691 architecture -> E701 classifier -> E706 endpoint shortcuts
//          -> E723 geometry-gated tier-0 gallop
//          -> E726 middle-density sparse-disorder bypass
//          -> E732 global natural-run prepass with endpoint preservation
//          -> E738 dense-disorder large-object compact-index realization.
//
// E691 established the phase-local per-game T=9 pressure-overflow architecture.
// E701 replaced the chunk classifier with the M=16 / k=4 tails-only probe.
// E706 imported ordered and reverse-disjoint outer-merge endpoint shortcuts.
// E723 added conservative local-geometry-gated galloping at outer merge tier 0.
// E726 added the middle-density distributed sparse-disorder bypass using
// fixed 128-element local repair only inside its validated admission band.
// E732 added the size-gated whole-input natural-run prepass, preserving E706
// endpoint shortcuts and falling through to live-phase when admission rejects.
// E738 sorts 32-bit indices and realizes one final in-place permutation when
// sizeof(T)>=32 and distributed local probes find dense direction switching.

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

#include <jessesort/detail/classification/phase_map/range_core.h>
#include <jessesort/detail/pipelines/engines/phase_range_e691_backbone.h>
#include <jessesort/detail/routing/sparse_middle_density.h>

namespace jessesort::simulated_direct_live_phase {
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

// E723: local outer-merge gallop admission. The E722 blanket transplant was
// strongly bimodal, so only tier 0 may use galloping, and only when two cheap
// local probes indicate a blocky merge: one side wins the first 1024 positions
// against the other head, and a 32+32 evenly-spaced quantile sample changes
// winning side at most twice. Later tiers retain std::merge.
template<class T, class Less>
inline bool outerTier0Prefix1024(const std::vector<T>& x,std::size_t b,std::size_t m,std::size_t e,Less less){
    const std::size_t nl=m-b,nr=e-m;
    if(nl>=1024 && !less(x[m],x[b+1023])) return true;
    if(nr>=1024 && less(x[m+1023],x[b])) return true;
    return false;
}

template<class T, class Less>
inline unsigned outerSampledSwitches32(const std::vector<T>& x,std::size_t b,std::size_t m,std::size_t e,Less less){
    const std::size_t nl=m-b,nr=e-m;
    if(!nl||!nr) return 99;
    std::size_t li[32],ri[32];
    for(unsigned q=0;q<32;++q){
        li[q]=b+((2*q+1)*nl)/64; if(li[q]>=m) li[q]=m-1;
        ri[q]=m+((2*q+1)*nr)/64; if(ri[q]>=e) ri[q]=e-1;
    }
    unsigned i=0,j=0,last=0,switches=0;
    while(i<32&&j<32){
        unsigned winner;
        if(less(x[ri[j]],x[li[i]])){++j;winner=2;}else{++i;winner=1;}
        if(last&&winner!=last) ++switches;
        last=winner;
        if(switches>2) break;
    }
    return switches;
}

template<class T, class Less>
inline bool outerTier0UseGallop(const std::vector<T>& x,std::size_t b,std::size_t m,std::size_t e,Less less){
    return outerTier0Prefix1024(x,b,m,e,less) && outerSampledSwitches32(x,b,m,e,less)<=2;
}

template<class T, class Less>
inline void outerGallopMergeToBack(std::vector<T>& a,std::vector<T>& out,std::size_t b,std::size_t m,std::size_t e,Less less){
    std::size_t i=b,j=m; unsigned lw=0,rw=0; constexpr unsigned GT=7;
    while(i<m && j<e){
        if(less(a[j],a[i])){
            out.push_back(std::move(a[j++])); ++rw; lw=0;
            if(rw>=GT && i<m && j<e){
                std::size_t step=1;
                while(j+step<e && less(a[j+step],a[i])) step<<=1;
                std::size_t lo=j,hi=std::min(e,j+step+1);
                while(lo<hi){auto q=lo+(hi-lo)/2;if(less(a[q],a[i]))lo=q+1;else hi=q;}
                out.insert(out.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(j)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(lo)));
                j=lo;rw=0;
            }
        }else{
            out.push_back(std::move(a[i++])); ++lw; rw=0;
            if(lw>=GT && i<m && j<e){
                std::size_t step=1;
                while(i+step<m && !less(a[j],a[i+step])) step<<=1;
                std::size_t lo=i,hi=std::min(m,i+step+1);
                while(lo<hi){auto q=lo+(hi-lo)/2;if(!less(a[j],a[q]))lo=q+1;else hi=q;}
                out.insert(out.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(i)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(lo)));
                i=lo;lw=0;
            }
        }
    }
    if(i<m) out.insert(out.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(i)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(m)));
    else if(j<e) out.insert(out.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(j)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(e)));
}

template<class T, class Less>
void mergeOuterRunsEndpointAware(std::vector<T>& a,const std::vector<std::size_t>& initial,Less less){
    if(initial.size()<=2)return;
    std::vector<std::size_t> starts=initial;
    std::vector<T> tmp;
    tmp.reserve(a.size());
    {
        const std::size_t runs=starts.size()-1;
        std::vector<std::size_t> next; next.reserve((starts.size()+1)/2); next.push_back(0);
        for(std::size_t r=0;r<runs;r+=2){
            const std::size_t b=starts[r],mid=starts[r+1];
            if(r+1<runs){
                const std::size_t e=starts[r+2];
                if(!less(a[mid],a[mid-1])){
                    tmp.insert(tmp.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(b)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(e)));
                }else if(!less(a[b],a[e-1])){
                    tmp.insert(tmp.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(mid)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(e)));
                    tmp.insert(tmp.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(b)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(mid)));
                }else{
                    if(outerTier0UseGallop(a,b,mid,e,less))
                        outerGallopMergeToBack(a,tmp,b,mid,e,less);
                    else
                        std::merge(std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(b)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(mid)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(mid)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(e)),std::back_inserter(tmp),less);
                }
                next.push_back(e);
            }else{
                tmp.insert(tmp.end(),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(b)),std::make_move_iterator(a.begin()+static_cast<std::ptrdiff_t>(mid)));
                next.push_back(mid);
            }
        }
        starts.swap(next);
    }
    bool srcTmp=true;
    while(starts.size()>2){
        auto& src=srcTmp?tmp:a; auto& dst=srcTmp?a:tmp;
        std::vector<std::size_t> next;next.reserve((starts.size()+1)/2);next.push_back(0);
        const std::size_t runs=starts.size()-1;
        for(std::size_t r=0;r<runs;r+=2){
            const std::size_t b=starts[r],mid=starts[r+1];
            if(r+1<runs){
                const std::size_t e=starts[r+2];
                if(!less(src[mid],src[mid-1])){
                    std::move(src.begin()+static_cast<std::ptrdiff_t>(b),src.begin()+static_cast<std::ptrdiff_t>(e),dst.begin()+static_cast<std::ptrdiff_t>(b));
                }else if(!less(src[b],src[e-1])){
                    const std::size_t rl=e-mid;
                    std::move(src.begin()+static_cast<std::ptrdiff_t>(mid),src.begin()+static_cast<std::ptrdiff_t>(e),dst.begin()+static_cast<std::ptrdiff_t>(b));
                    std::move(src.begin()+static_cast<std::ptrdiff_t>(b),src.begin()+static_cast<std::ptrdiff_t>(mid),dst.begin()+static_cast<std::ptrdiff_t>(b+rl));
                }else{
                    std::merge(std::make_move_iterator(src.begin()+static_cast<std::ptrdiff_t>(b)),std::make_move_iterator(src.begin()+static_cast<std::ptrdiff_t>(mid)),std::make_move_iterator(src.begin()+static_cast<std::ptrdiff_t>(mid)),std::make_move_iterator(src.begin()+static_cast<std::ptrdiff_t>(e)),dst.begin()+static_cast<std::ptrdiff_t>(b),less);
                }
                next.push_back(e);
            }else{
                std::move(src.begin()+static_cast<std::ptrdiff_t>(b),src.begin()+static_cast<std::ptrdiff_t>(mid),dst.begin()+static_cast<std::ptrdiff_t>(b));
                next.push_back(mid);
            }
        }
        starts.swap(next);srcTmp=!srcTmp;
    }
    if(srcTmp)std::move(tmp.begin(),tmp.end(),a.begin());
}


// E726: generic middle-density sparse-disorder bypass.
// E725 showed the E661 sparse-disorder route explains most of the Noise5/10
// gap, but E726A also showed that unconditional fixed-128 repair is unsafe on
// very sparse (e.g. BlockShuffle32/64) and too-dense (e.g. SortedNoise10)
// disorder. Admit only a distributed middle-density band, then reuse the
// proven 128-element insertion-block realization followed by mature merging.
template<class T, class Less>
bool trySparseMiddleDensity128(std::vector<T>& values, Less less) {
    constexpr std::size_t kBlock = 128;
    const std::size_t n = values.size();
    if (!jessesort::detail::sparseMiddleDensityCandidate<T, Less>(
            std::span<const T>(values.data(), values.size()), less))
        return false;

    std::span<T> a(values.data(), values.size());
    std::vector<std::size_t> ends;
    ends.reserve((n + kBlock - 1) / kBlock);
    for (std::size_t b = 0; b < n; b += kBlock) {
        const std::size_t e = std::min(n, b + kBlock);
        for (std::size_t i = b + 1; i < e; ++i) {
            T value = std::move(a[i]);
            std::size_t j = i;
            while (j > b && less(value, a[j-1])) {
                a[j] = std::move(a[j-1]);
                --j;
            }
            a[j] = std::move(value);
        }
        ends.push_back(e);
    }
    jessesort::simulated_phase_range_engine_e691::mergePreparedRunsFromSpanE283(a, ends, less);
    return true;
}


// E732: size-gated whole-input natural-run prepass retained for the allocating
// live-phase production candidate. This is intentionally conservative: it
// admits at most 64 natural runs, rejects repeated short-overlap geometry
// early, and preserves E706 ordered/reverse-disjoint endpoint shortcuts before
// handing remaining runs to the allocating mature merger.
struct GlobalRunDescE732 { std::size_t begin{}, end{}; bool descending{}; };
constexpr std::size_t kGlobalMaxRunsE732 = 64;

template<class T, class Less>
bool globalRunsOverlapE732(const std::vector<T>& a,
                           const GlobalRunDescE732& L,
                           const GlobalRunDescE732& R,
                           Less less) {
    const T& l0=a[L.begin], &l1=a[L.end-1], &r0=a[R.begin], &r1=a[R.end-1];
    const T& lmin=less(l1,l0)?l1:l0; const T& lmax=less(l1,l0)?l0:l1;
    const T& rmin=less(r1,r0)?r1:r0; const T& rmax=less(r1,r0)?r0:r1;
    return less(rmin,lmax) && less(lmin,rmax);
}

template<class T, class Less>
bool repeatedGlobalOverlapGeometryE732(
        const std::vector<T>& a,
        const std::array<GlobalRunDescE732,kGlobalMaxRunsE732>& runs,
        std::size_t rc, Less less) {
    if(rc<8) return false;
    std::size_t oc=0;
    for(std::size_t r=0;r+1<rc;++r)
        if(globalRunsOverlapE732(a,runs[r],runs[r+1],less)) ++oc;
    const std::size_t boundaries=rc-1;
    if(oc==boundaries) return true;
    constexpr std::size_t kMinAverageRunForNearOverlapMerge=512;
    if(a.size() >= rc*kMinAverageRunForNearOverlapMerge) return false;
    return oc*10 >= boundaries*9;
}

template<class T, class Less>
void mergePreparedGlobalRunsE732(std::span<T> arr,
                                 std::vector<std::size_t>& ends,
                                 Less less) {
    if (ends.size() <= 1) return;
    const std::size_t n = arr.size();
    std::vector<T> tmp;
    if constexpr (std::is_default_constructible_v<T>) tmp.resize(n);
    else tmp.assign(arr.begin(), arr.end());

    std::vector<std::size_t> next;
    next.reserve((ends.size()+1)/2);
    std::size_t left=0;
    for(std::size_t r=0;r<ends.size();r+=2){
        const std::size_t mid=ends[r];
        if(r+1==ends.size()){
            std::move(arr.begin()+static_cast<std::ptrdiff_t>(left),
                      arr.begin()+static_cast<std::ptrdiff_t>(mid),
                      tmp.begin()+static_cast<std::ptrdiff_t>(left));
            next.push_back(mid); left=mid; continue;
        }
        const std::size_t right=ends[r+1];
        if(!less(arr[mid],arr[mid-1])){
            std::move(arr.begin()+static_cast<std::ptrdiff_t>(left),
                      arr.begin()+static_cast<std::ptrdiff_t>(right),
                      tmp.begin()+static_cast<std::ptrdiff_t>(left));
        } else if(!less(arr[left],arr[right-1])){
            const std::size_t rightLen=right-mid;
            std::move(arr.begin()+static_cast<std::ptrdiff_t>(mid),
                      arr.begin()+static_cast<std::ptrdiff_t>(right),
                      tmp.begin()+static_cast<std::ptrdiff_t>(left));
            std::move(arr.begin()+static_cast<std::ptrdiff_t>(left),
                      arr.begin()+static_cast<std::ptrdiff_t>(mid),
                      tmp.begin()+static_cast<std::ptrdiff_t>(left+rightLen));
        } else {
            std::merge(std::make_move_iterator(arr.begin()+static_cast<std::ptrdiff_t>(left)),
                       std::make_move_iterator(arr.begin()+static_cast<std::ptrdiff_t>(mid)),
                       std::make_move_iterator(arr.begin()+static_cast<std::ptrdiff_t>(mid)),
                       std::make_move_iterator(arr.begin()+static_cast<std::ptrdiff_t>(right)),
                       tmp.begin()+static_cast<std::ptrdiff_t>(left), less);
        }
        next.push_back(right); left=right;
    }
    ends.swap(next);
    if(ends.size()<=1){ std::move(tmp.begin(),tmp.end(),arr.begin()); return; }
    jessesort::simulated_phase_range_engine_e691::mergeRunsAdjacentPairsEndsToSpan(
        tmp,arr,ends,less,false,true);
}

template<class T, class Less>
bool tryGlobalNaturalRunPrepassE732(std::vector<T>& a, Less less) {
    constexpr std::size_t kPrefix=64;
    const std::size_t n=a.size();
    if(n<50000) return false;
    for(std::size_t i=1;i<kPrefix;++i) if(less(a[i],a[i-1])) return false;
    bool coarseDrop=false; std::size_t previous=0;
    for(std::size_t sample=1;sample<9;++sample){
        const std::size_t index=((n-1)*sample)/8;
        if(less(a[index],a[previous])){coarseDrop=true;break;}
        previous=index;
    }
    if(!coarseDrop) return false;

    std::array<GlobalRunDescE732,kGlobalMaxRunsE732> runs{};
    std::size_t rc=0,i=0; bool firstTwoOverlap=true;
    while(i<n){
        if(rc==runs.size()) return false;
        const std::size_t begin=i;
        if(i+1==n){runs[rc++]={begin,n,false};break;}
        const bool d=less(a[i+1],a[i]); i+=2;
        if(d) while(i<n&&less(a[i],a[i-1])) ++i;
        else while(i<n&&!less(a[i],a[i-1])) ++i;
        runs[rc++]={begin,i,d};
        if(rc>=2 && rc<=3){
            firstTwoOverlap &= globalRunsOverlapE732(a,runs[rc-2],runs[rc-1],less);
            if(rc==3 && firstTwoOverlap && i<=8192) return false;
        }
    }
    if(rc<=1) return false;
    if(repeatedGlobalOverlapGeometryE732(a,runs,rc,less)) return false;

    std::vector<std::size_t> ends; ends.reserve(rc);
    for(std::size_t r=0;r<rc;++r){
        if(runs[r].descending)
            std::reverse(a.begin()+static_cast<std::ptrdiff_t>(runs[r].begin),
                         a.begin()+static_cast<std::ptrdiff_t>(runs[r].end));
        ends.push_back(runs[r].end);
    }
    if(rc==2){
        const std::size_t mid=runs[0].end;
        if(!less(a[mid],a[mid-1])) return true;
        if(!less(a.front(),a.back())){
            std::rotate(a.begin(),a.begin()+static_cast<std::ptrdiff_t>(mid),a.end());
            return true;
        }
    }
    std::span<T> s(a.data(),a.size());
    mergePreparedGlobalRunsE732(s,ends,less);
    return true;
}

template<class T, class Less>
bool denseLocalDirectionSwitchingE738(const std::vector<T>& values, Less less) {
    constexpr std::size_t kWindows = 8;
    constexpr std::size_t kEdges = 16;
    if (values.size() <= kEdges) return false;
    const std::size_t maxStart = values.size() - (kEdges + 1);
    for (std::size_t w = 0; w < kWindows; ++w) {
        const std::size_t begin = (w * maxStart) / (kWindows - 1);
        bool previousDesc = less(values[begin + 1], values[begin]);
        std::size_t switches = 0;
        for (std::size_t edge = 1; edge < kEdges; ++edge) {
            const bool desc = less(values[begin + edge + 1], values[begin + edge]);
            switches += desc != previousDesc;
            previousDesc = desc;
        }
        if (switches >= 8) return true;
    }
    return false;
}

template<class T, class Index>
void realizePermutationE738(std::vector<T>& values, std::vector<Index>& order) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (static_cast<std::size_t>(order[i]) == i) continue;
        T saved = std::move(values[i]);
        std::size_t current = i;
        while (static_cast<std::size_t>(order[current]) != i) {
            const std::size_t source = static_cast<std::size_t>(order[current]);
            values[current] = std::move(values[source]);
            order[current] = static_cast<Index>(current);
            current = source;
        }
        values[current] = std::move(saved);
        order[current] = static_cast<Index>(current);
    }
}

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& values, Less less=Less{}) {
    const std::size_t n = values.size();
    if (n < 2) return;
    if constexpr (sizeof(T) >= 32) {
        if (n <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) &&
            denseLocalDirectionSwitchingE738(values, less)) {
            std::vector<std::uint32_t> order(n);
            for (std::size_t i = 0; i < n; ++i) order[i] = static_cast<std::uint32_t>(i);
            const auto indirectLess = [&](std::uint32_t lhs, std::uint32_t rhs) {
                return less(values[lhs], values[rhs]);
            };
            // Recursive instantiation is on 4-byte indices, so the E738 gate is
            // compile-time disabled and recursion terminates in the normal path.
            sort(order, indirectLess);
            realizePermutationE738(values, order);
            return;
        }
    }
    if (tryGlobalNaturalRunPrepassE732(values, less)) return;
    if (n >= 10000 && trySparseMiddleDensity128(values, less)) return;
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
    mergeOuterRunsEndpointAware(values, bounds, less);
}

} // namespace jessesort::simulated_direct_live_phase
