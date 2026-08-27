#ifndef JESSESORT_E229_JESSESORT_INDEXED_DIRECT_MERGE_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_MOVE_ONLY_H
#define JESSESORT_E229_JESSESORT_INDEXED_DIRECT_MERGE_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_MOVE_ONLY_H

#include <jessesort/jessesort_simulated_direct-merge-probe-routed_adjacent-adaptive-buffered.h>
#include <optional>
#include <vector>
#include <cstdint>
#include <bit>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <cassert>

namespace jessesort::index_tail_direct_merge {


template<class T>
struct IndexSimResult {
    std::vector<uint32_t> blueprint;
    std::vector<std::size_t> ascCounts, descCounts;
    std::vector<std::size_t> ascTails, descTails; // indices into original arr
    bool alreadySortedAscending = true;
    bool reverseSortedDescending = true;
    bool earlyRandomLike = false;
};

template<class T, class Less>
inline std::size_t findDesc(const std::vector<T>& a, const std::vector<std::size_t>& tails,
                            std::size_t hint, std::size_t vi, Less less, bool useHint=true) {
    const std::size_t n=tails.size(); if(!n) return 0;
    const T& v=a[vi];
    if(useHint && hint<n && !less(a[tails[hint]],v) && (hint==0 || less(a[tails[hint-1]],v))) return hint;
    std::ptrdiff_t idx=-1; std::size_t step=std::size_t{1} << (std::bit_width(n)-1);
    for(;step;step>>=1){ auto next=static_cast<std::size_t>(idx+static_cast<std::ptrdiff_t>(step)); if(next<n && less(a[tails[next]],v)) idx=static_cast<std::ptrdiff_t>(next); }
    return static_cast<std::size_t>(idx+1);
}
template<class T, class Less>
inline std::size_t findAsc(const std::vector<T>& a, const std::vector<std::size_t>& tails,
                           std::size_t hint, std::size_t vi, Less less, bool useHint=true) {
    const std::size_t n=tails.size(); if(!n) return 0;
    const T& v=a[vi];
    if(useHint && hint<n && !less(v,a[tails[hint]]) && (hint==0 || less(v,a[tails[hint-1]]))) return hint;
    std::ptrdiff_t idx=-1; std::size_t step=std::size_t{1} << (std::bit_width(n)-1);
    for(;step;step>>=1){ auto next=static_cast<std::size_t>(idx+static_cast<std::ptrdiff_t>(step)); if(next<n && less(v,a[tails[next]])) idx=static_cast<std::ptrdiff_t>(next); }
    return static_cast<std::size_t>(idx+1);
}

template<class T,class Less>
inline void insertOne(const std::vector<T>& a, std::size_t i, bool desc, IndexSimResult<T>& r,
                      std::size_t& ah, std::size_t& dh, Less less, bool useHint=true) {
    if(desc){ auto p=findDesc(a,r.descTails,dh,i,less,useHint); if(p<r.descTails.size()){r.descTails[p]=i;++r.descCounts[p];} else {r.descTails.push_back(i);r.descCounts.push_back(1);} dh=p; r.blueprint[i]=jessesort::simulated_direct_merge::makeDescTag((uint32_t)p); }
    else { auto p=findAsc(a,r.ascTails,ah,i,less,useHint); if(p<r.ascTails.size()){r.ascTails[p]=i;++r.ascCounts[p];} else {r.ascTails.push_back(i);r.ascCounts.push_back(1);} ah=p; r.blueprint[i]=jessesort::simulated_direct_merge::makeAscTag((uint32_t)p); }
}

template<class T,class Less>
IndexSimResult<T> simulate(const std::vector<T>& a, Less less, bool natural=true, bool forceNoSpecial=false) {
    (void)forceNoSpecial;
    constexpr std::size_t MinPrefix=32, Sample=64, Probe=64;
    IndexSimResult<T> r; const std::size_t n=a.size(); if(!n) return r;
    enum class Dir{Unknown,Asc,Desc}; Dir pd=Dir::Unknown; std::size_t pe=1;
    for(;pe<n;++pe){const T& prev=a[pe-1];const T& v=a[pe]; if(pd==Dir::Asc){if(less(v,prev))break;} else if(pd==Dir::Desc){if(less(prev,v))break;} else if(less(prev,v))pd=Dir::Asc; else if(less(v,prev))pd=Dir::Desc;}
    if(pe==n){r.alreadySortedAscending=pd!=Dir::Desc; r.reverseSortedDescending=pd==Dir::Desc; return r;}
    r.alreadySortedAscending=false;r.reverseSortedDescending=false;r.blueprint.resize(n);
    auto reserve=jessesort::simulated_direct_merge::estimatePileReserve(n); r.ascCounts.reserve(reserve);r.descCounts.reserve(reserve);r.ascTails.reserve(reserve);r.descTails.reserve(reserve);
    std::size_t ah=0,dh=0; bool dm=false; std::size_t ps=1;
    bool confirmed = pd!=Dir::Unknown && pe<MinPrefix && jessesort::simulated_direct_merge::confirmedShortSameDirectionPrefix(a,pe,pd==Dir::Desc,less,MinPrefix);
    bool materialize=pd!=Dir::Unknown && (pe>=MinPrefix || confirmed);
    if(materialize){ps=pe;if(pd==Dir::Asc){r.ascTails.push_back(pe-1);r.ascCounts.push_back(pe);std::fill_n(r.blueprint.begin(),pe,jessesort::simulated_direct_merge::makeAscTag(0));dm=false;}else{r.descTails.push_back(pe-1);r.descCounts.push_back(pe);std::fill_n(r.blueprint.begin(),pe,jessesort::simulated_direct_merge::makeDescTag(0));dm=true;}}
    else {if(pd==Dir::Desc){r.descTails.push_back(0);r.descCounts.push_back(1);r.blueprint[0]=jessesort::simulated_direct_merge::makeDescTag(0);dm=true;}else{r.ascTails.push_back(0);r.ascCounts.push_back(1);r.blueprint[0]=jessesort::simulated_direct_merge::makeAscTag(0);dm=false;}}
    bool override=false,od=false; if(ps==pe&&materialize&&ps+1<n){if(less(a[ps],a[ps+1])){override=true;od=false;}else if(less(a[ps+1],a[ps])){override=true;od=true;}}
    // Current natural-run route, index-tail form.
    bool nat=natural && pd!=Dir::Unknown && pe>=MinPrefix && pe*8>=n;
    if(nat && ps<n){ std::size_t i=ps; bool op=override;
      auto one=[&](std::size_t j){ if(op){dm=od;op=false;} else if(less(a[j-1],a[j]))dm=false; else if(less(a[j],a[j-1]))dm=true; else {if(dm){++r.descCounts[dh];r.blueprint[j]=jessesort::simulated_direct_merge::makeDescTag((uint32_t)dh);}else{++r.ascCounts[ah];r.blueprint[j]=jessesort::simulated_direct_merge::makeAscTag((uint32_t)ah);}return;} insertOne(a,j,dm,r,ah,dh,less,true); };
      while(i<n){bool asc,desc;if(op){asc=!od;desc=od;}else{asc=less(a[i-1],a[i]);desc=!asc&&less(a[i],a[i-1]);} if(!asc&&!desc){one(i++);continue;} std::size_t e=i+1;if(asc){while(e<n&&less(a[e-1],a[e]))++e;}else{while(e<n&&less(a[e],a[e-1]))++e;} auto len=e-i;bool bat=false;if(len>=8){if(asc){auto p1=findAsc(a,r.ascTails,ah,i,less,false),p2=findAsc(a,r.ascTails,ah,e-1,less,false);if(p1==p2){if(p1<r.ascTails.size()){r.ascTails[p1]=e-1;r.ascCounts[p1]+=len;}else{r.ascTails.push_back(e-1);r.ascCounts.push_back(len);}std::fill(r.blueprint.begin()+i,r.blueprint.begin()+e,jessesort::simulated_direct_merge::makeAscTag((uint32_t)p1));ah=p1;dm=false;op=false;bat=true;}}else{auto p1=findDesc(a,r.descTails,dh,i,less,false),p2=findDesc(a,r.descTails,dh,e-1,less,false);if(p1==p2){if(p1<r.descTails.size()){r.descTails[p1]=e-1;r.descCounts[p1]+=len;}else{r.descTails.push_back(e-1);r.descCounts.push_back(len);}std::fill(r.blueprint.begin()+i,r.blueprint.begin()+e,jessesort::simulated_direct_merge::makeDescTag((uint32_t)p1));dh=p1;dm=true;op=false;bat=true;}}}if(!bat)for(std::size_t j=i;j<e;++j)one(j);i=e;} return r; }
    auto process=[&](std::size_t i,bool useHint){if(override){dm=od;override=false;}else if(less(a[i-1],a[i]))dm=false;else if(less(a[i],a[i-1]))dm=true;else{if(dm){++r.descCounts[dh];r.blueprint[i]=jessesort::simulated_direct_merge::makeDescTag((uint32_t)dh);}else{++r.ascCounts[ah];r.blueprint[i]=jessesort::simulated_direct_merge::makeAscTag((uint32_t)ah);}return true;} std::size_t old=dm?dh:ah; bool had=dm?!r.descTails.empty():!r.ascTails.empty();insertOne(a,i,dm,r,ah,dh,less,useHint);return had&&old==(dm?dh:ah);};
    std::size_t se=std::min(n,ps+Sample),hits=0,cnt=0,i=ps;for(;i<se;++i){hits+=process(i,true);++cnt;if((ps>=Probe&&cnt==Probe)||(ps<Probe&&i+1==Probe))r.earlyRandomLike=r.ascCounts.size()>=6&&r.descCounts.size()>=6;}
    bool earlyRandom=n>=10000&&r.earlyRandomLike; bool useSplit=cnt&&hits*4>=cnt*3; (void)useSplit;
    std::size_t piles=r.ascCounts.size()+r.descCounts.size(); bool valley=n>=10000&&piles>=3&&piles<=12;
    for(;i<n;++i){ if(valley){ bool desc=less(a[i],a[i-1]),asc=!desc&&less(a[i-1],a[i]); if(desc&&i+1<n&&less(a[i],a[i+1])){bool create=r.descTails.empty()||less(a[r.descTails.back()],a[i]);bool can=!r.ascTails.empty()&&!less(a[i],a[r.ascTails.back()]);if(create&&can){desc=false;asc=true;}} if(asc)dm=false;else if(desc)dm=true;else{if(dm){++r.descCounts[dh];r.blueprint[i]=jessesort::simulated_direct_merge::makeDescTag((uint32_t)dh);}else{++r.ascCounts[ah];r.blueprint[i]=jessesort::simulated_direct_merge::makeAscTag((uint32_t)ah);}continue;} insertOne(a,i,dm,r,ah,dh,less,true); }
      else process(i,!earlyRandom); }
    return r;
}

template<class T>
bool randomMerge(const IndexSimResult<T>& sim,std::size_t n){std::size_t c=sim.ascCounts.size()+sim.descCounts.size();__extension__ typedef unsigned __int128 W;W sq=(W)c*c;bool d=n<=20000?(W)2*sq>=(W)7*n:n<500000?(W)4*sq>=(W)17*n:sq>=(W)5*n;return n>=10000&&sim.earlyRandomLike&&d&&std::is_trivially_copyable_v<T>&&sizeof(T)<=96;}

template <class T>
std::vector<std::size_t> reconstructMoveOnly(
    std::vector<T>& arr,
    const std::vector<uint32_t>& blueprint,
    const std::vector<std::size_t>& ascCounts,
    const std::vector<std::size_t>& descCounts,
    std::vector<T>& tmp
) {
    const std::size_t n = arr.size();
    const std::size_t numAsc = ascCounts.size();
    const std::size_t numDesc = descCounts.size();
    const std::size_t numRuns = numAsc + numDesc;
    std::vector<std::size_t> start(numRuns + 1, 0);
    std::size_t out = 0;
    for (std::size_t p = 0; p < numDesc; ++p) { start[p] = out; out += descCounts[p]; }
    for (std::size_t p = 0; p < numAsc; ++p) { const std::size_t r = numDesc + p; start[r] = out; out += ascCounts[p]; }
    start[numRuns] = out;
    assert(out == n);

    std::vector<std::size_t> cursor(numRuns);
    for (std::size_t p = 0; p < numDesc; ++p) cursor[p] = start[p + 1] - 1;
    for (std::size_t p = 0; p < numAsc; ++p) cursor[numDesc + p] = start[numDesc + p];

    std::vector<std::optional<T>> slots(n);
    for (std::size_t i = 0; i < n; ++i) {
        const uint32_t tag = blueprint[i];
        const std::size_t local = static_cast<std::size_t>(jessesort::simulated_direct_merge::localPileId(tag));
        const bool desc = jessesort::simulated_direct_merge::isDescTag(tag);
        const std::size_t r = desc ? local : numDesc + local;
        const std::size_t pos = cursor[r];
        if (desc) --cursor[r]; else ++cursor[r];
        slots[pos].emplace(std::move(arr[i]));
    }
    tmp.clear(); tmp.reserve(n);
    for (auto& slot : slots) {
        assert(slot.has_value());
        tmp.emplace_back(std::move(*slot));
    }
    return start;
}

template<class T,class Less=std::less<T>>
void sort(std::vector<T>& a,Less less=Less{}){
 static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
               "jessesort::index_tail_direct_merge::sort requires movable values");
 static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>,
               "Comparator must be callable as bool(const T&, const T&)");
 if(jessesort::detail::tryTinyInsertionSort(a,less))return;if(a.size()<2)return;
 if(a.size()>=10000 && jessesort::simulated_direct_merge::tryLongAscendingNaturalRunDirect(a,less))return;
 bool mix=true;if(a.size()>=4){bool up=less(a[0],a[1])&&less(a[1],a[2])&&less(a[2],a[3]);bool dn=less(a[1],a[0])&&less(a[2],a[1])&&less(a[3],a[2]);mix=!(up||dn);} 
 // E264: intentional inline mirror; shared noinline routing regressed canonical structured inputs.
 // Keep synchronized with the owning simulated specialized router and audit drift explicitly.
 if constexpr(jessesort::simulated_direct_merge::specializedIntegralEligible<T,Less>){if(mix){T dom=a[0];if(jessesort::simulated_direct_merge::dominantValueSampleCandidate(a,dom,less)){jessesort::simulated_direct_merge::highEntropyQuickSort(a.data(),a.size(),2*(int)std::bit_width(a.size()),less,false,T{},false,true,nullptr,false);return;} if(jessesort::simulated_direct_merge::lowCardinalityDirectionGate(a,less)&&jessesort::simulated_direct_merge::lowCardinalitySampleCandidate(a,less)&&jessesort::simulated_direct_merge::trySortLowCardinalityDirectConfirmed(a,less))return; bool alt=false;if(a.size()>=8){int prev=0;alt=true;for(std::size_t i=1;i<8;++i){int d=less(a[i-1],a[i])?1:less(a[i],a[i-1])?-1:0;if(d==0||(prev&&d==prev)){alt=false;break;}prev=d;}}if(!alt&&jessesort::simulated_direct_merge::trySortHighEntropyPartitionDirect(a,less))return;}}
 auto s=simulate(a,less,true);if(s.alreadySortedAscending)return;if(s.reverseSortedDescending){std::reverse(a.begin(),a.end());return;}bool br=randomMerge(s,a.size());std::vector<T> tmp;std::vector<std::size_t> starts;
 if constexpr (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>) {
   starts=jessesort::simulated_direct_merge::reconstructTaggedBlueprintNormalizedForSimulated(a,std::move(s.blueprint),std::move(s.ascCounts),std::move(s.descCounts),tmp,false,true,true,true);
 } else {
   starts=reconstructMoveOnly(a,s.blueprint,s.ascCounts,s.descCounts,tmp);
 }
 std::vector<std::size_t> ends(starts.begin()+1,starts.end());jessesort::simulated_direct_merge::mergeRunsAdjacentPairsEnds(tmp,a,ends,less,br,true);a=std::move(tmp);
}}

#endif
