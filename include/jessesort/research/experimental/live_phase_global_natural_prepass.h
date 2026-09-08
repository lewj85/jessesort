#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#include <jessesort/detail/pipelines/reference/simulated-direct_live-phase.h>
#include <jessesort/detail/pipelines/engines/phase_range_e691_backbone.h>

namespace jessesort::research::e732_live_phase_global_natural_prepass {

struct RunDesc { std::size_t begin{}, end{}; bool descending{}; };
constexpr std::size_t kMaxRuns = 64;

template<class T, class Less>
bool overlaps(const std::vector<T>& a, const RunDesc& L, const RunDesc& R, Less less) {
    const T& l0=a[L.begin], &l1=a[L.end-1], &r0=a[R.begin], &r1=a[R.end-1];
    const T& lmin=less(l1,l0)?l1:l0; const T& lmax=less(l1,l0)?l0:l1;
    const T& rmin=less(r1,r0)?r1:r0; const T& rmax=less(r1,r0)?r0:r1;
    return less(rmin,lmax) && less(lmin,rmax);
}

template<class T, class Less>
bool repeatedOverlapGeometry(const std::vector<T>& a,
                             const std::array<RunDesc,kMaxRuns>& runs,
                             std::size_t rc, Less less) {
    if(rc<8) return false;
    std::size_t oc=0;
    for(std::size_t r=0;r+1<rc;++r) if(overlaps(a,runs[r],runs[r+1],less)) ++oc;
    const std::size_t boundaries=rc-1;
    if(oc==boundaries) return true;
    constexpr std::size_t kMinAverageRunForNearOverlapMerge=512;
    if(a.size() >= rc*kMinAverageRunForNearOverlapMerge) return false;
    return oc*10 >= boundaries*9;
}

template<class T, class Less>
void mergePreparedRunsE732(std::span<T> arr, std::vector<std::size_t>& ends, Less less);

template<class T, class Less>
bool tryGlobalNaturalRunPrepass(std::vector<T>& a, Less less) {
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

    std::array<RunDesc,kMaxRuns> runs{};
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
            firstTwoOverlap &= overlaps(a,runs[rc-2],runs[rc-1],less);
            if(rc==3 && firstTwoOverlap && i<=8192) return false;
        }
    }
    if(rc<=1) return false;
    if(repeatedOverlapGeometry(a,runs,rc,less)) return false;

    std::vector<std::size_t> ends; ends.reserve(rc);
    for(std::size_t r=0;r<rc;++r){
        if(runs[r].descending)
            std::reverse(a.begin()+static_cast<std::ptrdiff_t>(runs[r].begin),
                         a.begin()+static_cast<std::ptrdiff_t>(runs[r].end));
        ends.push_back(runs[r].end);
    }
    // E706 endpoint shortcut: a two-run reverse-disjoint result is already
    // sorted by swapping the run order; do not force it through std::merge.
    if(rc==2){
        const std::size_t mid=runs[0].end;
        if(!less(a[mid],a[mid-1])) return true;
        if(!less(a.front(),a.back())){
            std::rotate(a.begin(),a.begin()+static_cast<std::ptrdiff_t>(mid),a.end());
            return true;
        }
    }
    std::span<T> s(a.data(),a.size());
    mergePreparedRunsE732(s,ends,less);
    return true;
}


template<class T, class Less>
void mergePreparedRunsE732(std::span<T> arr, std::vector<std::size_t>& ends, Less less) {
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
    jessesort::simulated_phase_range_engine_e691::mergeRunsAdjacentPairsEndsToSpan(tmp,arr,ends,less,false,true);
}

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& values, Less less=Less{}) {
    if(values.size()<2) return;
    if(tryGlobalNaturalRunPrepass(values,less)) return;
    jessesort::simulated_direct_live_phase::sort(values,less);
}

} // namespace
