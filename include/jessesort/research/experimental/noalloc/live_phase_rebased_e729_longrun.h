#pragma once

// E729 experimental allocation-free live-phase adaptive variant with one
// whole-input mature noalloc long-natural-run attempt before live-phase.
// Rebase the two noalloc contracts onto the E691->E701->E706->E723->E726
// live-phase front-end policy while keeping fixed-capacity/in-place realizations.

#include <jessesort/detail/pipelines/reference/simulated-direct_live-phase.h>
#include <jessesort/detail/pipelines/reference/noalloc_direct.h>
#include <jessesort/detail/pipelines/reference/rust_unstable_noalloc.h>
#include <jessesort/detail/decomposition/noalloc/low_run_merge.h>
#include <jessesort/detail/routing/sparse_middle_density.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>

namespace jessesort::experimental::live_phase_noalloc_e729 {

inline constexpr std::size_t kMaxRegions = 64;
inline constexpr std::size_t kMaxRuns = 64;

struct Run { std::size_t b{}, e{}; bool desc{}; };

template<class T, class Less>
void rawBoundedSort(std::span<T> s, Less less) {
    if (s.size() < 2) return;
    if constexpr (jessesort::simulated_legacy::specializedIntegralEligible<T, Less> &&
                  std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>) {
        jessesort::simulated_legacy::highEntropyQuickSortProduction(
            s.data(), s.size(), 2 * static_cast<int>(std::bit_width(s.size())),
            less, false, T{});
    } else {
        jessesort::simulated_legacy::highEntropyHeapSortFallback(s.data(), s.size(), less);
    }
}

template<class T, class Less>
bool discoverRuns(std::span<T> s, std::array<Run,kMaxRuns>& runs, std::size_t& rc, Less less) {
    rc=0; std::size_t i=0, n=s.size();
    while(i<n){
        if(rc==kMaxRuns) return false;
        std::size_t b=i;
        if(i+1==n){ runs[rc++]={b,n,false}; break; }
        bool d=less(s[i+1],s[i]); i+=2;
        if(d) while(i<n && less(s[i],s[i-1])) ++i;
        else while(i<n && !less(s[i],s[i-1])) ++i;
        runs[rc++]={b,i,d};
    }
    return true;
}

template<class T, class Less, bool Adaptive>
void mergePair(std::span<T> s, std::size_t b, std::size_t m, std::size_t e, Less less) {
    if(b==m||m==e) return;
    if(!less(s[m],s[m-1])) return;
    if(!less(s[b],s[e-1])) { std::rotate(s.begin()+b,s.begin()+m,s.begin()+e); return; }
    auto f=s.begin()+static_cast<std::ptrdiff_t>(b),
         mid=s.begin()+static_cast<std::ptrdiff_t>(m),
         l=s.begin()+static_cast<std::ptrdiff_t>(e);
    if constexpr (Adaptive) {
        constexpr std::size_t bytes=8192;
        constexpr std::size_t cap=bytes/sizeof(T);
        if constexpr (cap>=2) {
            using Storage=std::aligned_storage_t<sizeof(T),alignof(T)>;
            std::array<Storage,cap> storage;
            T* scratch=reinterpret_cast<T*>(storage.data());
            jessesort::allocation_free_direct::mergeRecursiveLeafBufferedNoAlloc(
                f,mid,l,less,scratch,cap,false);
        } else {
            jessesort::allocation_free_low_run::mergeRecursiveNoAlloc(f,mid,l,less);
        }
    } else {
        jessesort::allocation_free_low_run::mergeRecursiveNoAlloc(f,mid,l,less);
    }
}

template<class T, class Less, bool Adaptive>
void sortRegion(std::span<T> s, Less less) {
    if(s.size()<2) return;
    // Cheap monotone region guard.
    bool asc=true, desc=true;
    for(std::size_t i=1;i<s.size();++i){
        if(less(s[i],s[i-1])) asc=false;
        if(less(s[i-1],s[i])) desc=false;
        if(!asc&&!desc) break;
    }
    if(asc) return;
    if(desc){ std::reverse(s.begin(),s.end()); return; }

    std::array<Run,kMaxRuns> runs{}; std::size_t rc=0;
    if(!discoverRuns(s,runs,rc,less)){ rawBoundedSort(s,less); return; }
    for(std::size_t r=0;r<rc;++r) if(runs[r].desc)
        std::reverse(s.begin()+static_cast<std::ptrdiff_t>(runs[r].b),
                     s.begin()+static_cast<std::ptrdiff_t>(runs[r].e));
    while(rc>1){
        std::size_t out=0;
        for(std::size_t r=0;r<rc;r+=2){
            if(r+1==rc){ runs[out++]={runs[r].b,runs[r].e,false}; continue; }
            const auto b=runs[r].b,m=runs[r].e,e=runs[r+1].e;
            mergePair<T,Less,Adaptive>(s,b,m,e,less);
            runs[out++]={b,e,false};
        }
        rc=out;
    }
}

template<class T, class Less, bool Adaptive>
void mergeRegions(std::span<T> a, std::array<std::size_t,kMaxRegions+1>& bounds,
                  std::size_t bc, Less less) {
    while(bc>2){
        std::array<std::size_t,kMaxRegions+1> next{}; std::size_t nc=1; next[0]=0;
        const std::size_t runs=bc-1;
        for(std::size_t r=0;r<runs;r+=2){
            const std::size_t b=bounds[r],m=bounds[r+1];
            if(r+1==runs){ next[nc++]=m; continue; }
            const std::size_t e=bounds[r+2];
            mergePair<T,Less,Adaptive>(a,b,m,e,less);
            next[nc++]=e;
        }
        bounds=next; bc=nc;
    }
}


template<class T, class Less>
bool tryLongNaturalRunDirectNoAllocE729(std::vector<T>& a, Less less) {
    constexpr std::size_t kMinN=10000, kPrefix=64;
    const std::size_t n=a.size(); if(n<kMinN) return false;
    for(std::size_t i=1;i<kPrefix;++i) if(less(a[i],a[i-1])) return false;
    bool coarseDrop=false; std::size_t previous=0;
    for(std::size_t sample=1;sample<9;++sample){
        const std::size_t index=((n-1)*sample)/8;
        if(less(a[index],a[previous])){coarseDrop=true;break;} previous=index;
    }
    if(!coarseDrop) return false;

    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns> runs{};
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
            auto L=runs[rc-2],R=runs[rc-1];
            const T& l0=a[L.begin]; const T& l1=a[L.end-1];
            const T& r0=a[R.begin]; const T& r1=a[R.end-1];
            const T& lmin=less(l1,l0)?l1:l0; const T& lmax=less(l1,l0)?l0:l1;
            const T& rmin=less(r1,r0)?r1:r0; const T& rmax=less(r1,r0)?r0:r1;
            firstTwoOverlap &= less(rmin,lmax)&&less(lmin,rmax);
            // Generic early bailout for repeated short overlap: after three
            // complete runs, if both boundaries overlap and those runs occupy
            // only a small prefix, the mature route is likely to reject later.
            if(rc==3 && firstTwoOverlap && i<=8192) return false;
        }
    }
    if(rc<=1) return false;
    if(jessesort::allocation_free_bounded::cheapMergeGeometryOnly(a,runs,rc,less)){
        jessesort::allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a,runs,rc,less);
    }else{
        if(jessesort::allocation_free_low_run::repeatedOverlapGeometry(a,runs,rc,less)) return false;
        bool hasDesc=false; for(std::size_t r=0;r<rc;++r) hasDesc|=runs[r].descending;
        if(hasDesc) jessesort::allocation_free_direct::mergeNaturalRunsLeafBufferedNoAlloc(a,runs,rc,less);
        else jessesort::allocation_free_direct::mergeNaturalRunsTopLevelTrimmedNoAlloc(a,runs,rc,less);
    }
    return true;
}

template<class T, class Less, bool Adaptive>
void sortImpl(std::vector<T>& values, Less less) {
    const std::size_t n=values.size(); if(n<2) return;
    if(jessesort::detail::tryTinyInsertionSort(values,less)) return;

    // E729: one global long-natural-run opportunity before the live-phase loop.
    // Adaptive only; strict E728 control remains unchanged.
    if constexpr (Adaptive) {
        if (n >= 50000 && tryLongNaturalRunDirectNoAllocE729(values, less)) return;
    }

    // E726 shared sparse admission. Realization follows each noalloc contract.
    if(n>=10000 && jessesort::detail::sparseMiddleDensityCandidate<T,Less>(
            std::span<const T>(values.data(),values.size()),less)) {
        if constexpr (Adaptive) {
            if(jessesort::allocation_free_direct::trySparseDistributedDisorderDirectNoAlloc(values,less)) return;
        } else {
            jessesort::allocation_free_bounded::fallbackNoAlloc(values,less); return;
        }
    }

    std::span<T> a(values.data(),values.size());
    constexpr std::size_t kMinGhost=32,kInitial=1024,kMaxStep=8192;
    std::array<std::size_t,kMaxRegions+1> bounds{}; std::size_t bc=1; bounds[0]=0;
    auto pushBound=[&](std::size_t x)->bool{
        if(bounds[bc-1]==x) return true;
        if(bc==bounds.size()) return false;
        bounds[bc++]=x; return true;
    };

    std::size_t i=0;
    std::size_t q=1; while(q<n && !less(a[q-1],a[q]) && !less(a[q],a[q-1])) ++q;
    if(q==n) return;
    bool initialDesc=less(a[q],a[q-1]);
    std::size_t initialEnd=initialDesc
      ? jessesort::simulated_direct_live_phase::extendDescending(a,0,less)
      : jessesort::simulated_direct_live_phase::extendAscending(a,0,less);
    if(initialEnd==n){ if(initialDesc) std::reverse(a.begin(),a.end()); return; }
    if(initialEnd>=kMinGhost){
        if(initialDesc) std::reverse(a.begin(),a.begin()+static_cast<std::ptrdiff_t>(initialEnd));
        i=initialEnd; if(!pushBound(i)){ rawBoundedSort(a,less); return; }
    }

    while(i<n){
        std::size_t firstEnd=std::min(n,i+kInitial);
        auto type=jessesort::simulated_direct_live_phase::classifyChunk<T,Less>(
          std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(i),firstEnd-i),less);
        std::size_t actualEnd=firstEnd;
        if(type==jessesort::simulated_direct_live_phase::ChunkType::MonotoneAsc){
            std::size_t re=jessesort::simulated_direct_live_phase::extendAscending(a,i,less);
            if(re-i>=kMinGhost) actualEnd=re; else type=jessesort::simulated_direct_live_phase::ChunkType::Patience;
        } else if(type==jessesort::simulated_direct_live_phase::ChunkType::MonotoneDesc){
            std::size_t re=jessesort::simulated_direct_live_phase::extendDescending(a,i,less);
            if(re-i>=kMinGhost){ std::reverse(a.begin()+static_cast<std::ptrdiff_t>(i),a.begin()+static_cast<std::ptrdiff_t>(re)); actualEnd=re; }
            else type=jessesort::simulated_direct_live_phase::ChunkType::Patience;
        }
        if(type==jessesort::simulated_direct_live_phase::ChunkType::Direct ||
           type==jessesort::simulated_direct_live_phase::ChunkType::Patience){
            std::size_t regionEnd=firstEnd,step=kInitial;
            while(regionEnd<n){
                step=std::min(kMaxStep,step*2); std::size_t pe=std::min(n,regionEnd+step);
                auto nt=jessesort::simulated_direct_live_phase::classifyChunk<T,Less>(
                  std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(regionEnd),pe-regionEnd),less);
                if(nt!=type){
                    bool persistent=true; constexpr std::size_t stride=256;
                    for(std::size_t c=1;c<=2;++c){
                        std::size_t cs=regionEnd+c*stride; if(cs>=n){persistent=false;break;}
                        std::size_t ce=std::min(n,cs+kInitial);
                        auto ct=jessesort::simulated_direct_live_phase::classifyChunk<T,Less>(
                          std::span<const T>(a.data()+static_cast<std::ptrdiff_t>(cs),ce-cs),less);
                        if(ct!=nt){persistent=false;break;}
                    }
                    if(persistent) break;
                }
                regionEnd=pe;
            }
            actualEnd=regionEnd;
            auto region=std::span<T>(a.data()+static_cast<std::ptrdiff_t>(i),actualEnd-i);
            if(type==jessesort::simulated_direct_live_phase::ChunkType::Direct) rawBoundedSort(region,less);
            else sortRegion<T,Less,Adaptive>(region,less);
        }
        i=actualEnd;
        if(!pushBound(i)){ rawBoundedSort(a,less); return; }
    }
    if(bounds[bc-1]!=n && !pushBound(n)){ rawBoundedSort(a,less); return; }
    mergeRegions<T,Less,Adaptive>(a,bounds,bc,less);
}

template<class T,class Less=std::less<T>>
void sort_adaptive(std::vector<T>& v,Less less=Less{}) { sortImpl<T,Less,true>(v,less); }

template<class T,class Less=std::less<T>>
void sort_strict(std::vector<T>& v,Less less=Less{}) { sortImpl<T,Less,false>(v,less); }

} // namespace jessesort::experimental::live_phase_noalloc_e729
