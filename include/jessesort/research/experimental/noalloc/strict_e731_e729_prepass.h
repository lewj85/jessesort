#pragma once
#include <jessesort/detail/pipelines/reference/rust_unstable_noalloc.h>
#include <array>
#include <vector>
#include <algorithm>

namespace jessesort::experimental::strict_e731 {

template<class T, class Less>
bool tryE729PrepassStrict(std::vector<T>& a, Less less) {
    constexpr std::size_t kPrefix = 64;
    const std::size_t n = a.size();
    if (n < 50000) return false;
    for (std::size_t i=1;i<kPrefix;++i) if (less(a[i],a[i-1])) return false;
    bool coarseDrop=false; std::size_t previous=0;
    for (std::size_t sample=1;sample<9;++sample) {
        const std::size_t index=((n-1)*sample)/8;
        if (less(a[index],a[previous])) { coarseDrop=true; break; }
        previous=index;
    }
    if (!coarseDrop) return false;

    std::array<jessesort::allocation_free_bounded::RunDesc,
               jessesort::allocation_free_low_run::kMaxRuns> runs{};
    std::size_t rc=0,i=0; bool firstTwoOverlap=true;
    while (i<n) {
        if (rc==runs.size()) return false;
        const std::size_t begin=i;
        if (i+1==n) { runs[rc++]={begin,n,false}; break; }
        const bool d=less(a[i+1],a[i]); i+=2;
        if (d) while(i<n && less(a[i],a[i-1])) ++i;
        else while(i<n && !less(a[i],a[i-1])) ++i;
        runs[rc++]={begin,i,d};
        if (rc>=2 && rc<=3) {
            const auto L=runs[rc-2], R=runs[rc-1];
            const T& l0=a[L.begin]; const T& l1=a[L.end-1];
            const T& r0=a[R.begin]; const T& r1=a[R.end-1];
            const T& lmin=less(l1,l0)?l1:l0; const T& lmax=less(l1,l0)?l0:l1;
            const T& rmin=less(r1,r0)?r1:r0; const T& rmax=less(r1,r0)?r0:r1;
            firstTwoOverlap &= less(rmin,lmax) && less(lmin,rmax);
            if (rc==3 && firstTwoOverlap && i<=8192) return false;
        }
    }
    if (rc<=1) return false;
    if (jessesort::allocation_free_bounded::cheapMergeGeometryOnly(a,runs,rc,less)) {
        jessesort::allocation_free_bounded::sortCheapNaturalRunsNoAlloc(a,runs,rc,less);
    } else {
        if (jessesort::allocation_free_low_run::repeatedOverlapGeometry(a,runs,rc,less)) return false;
        bool hasDesc=false;
        for (std::size_t r=0;r<rc;++r) hasDesc |= runs[r].descending;
        if (hasDesc)
            jessesort::rust_unstable_noalloc_detail::mergeNaturalRunsLeafBufferedNoAlloc(a,runs,rc,less);
        else
            jessesort::rust_unstable_noalloc_detail::mergeNaturalRunsTopLevelTrimmedNoAlloc(a,runs,rc,less);
    }
    return true;
}

template<class T, class Less=std::less<T>>
void sort(std::vector<T>& a, Less less=Less{}) {
    static_assert(std::is_move_constructible_v<T> && std::is_move_assignable_v<T>);
    static_assert(std::is_invocable_r_v<bool, Less&, const T&, const T&>);
    if (a.size() >= 50000 && tryE729PrepassStrict(a, less)) return;
    jessesort::rust_unstable_noalloc_detail::sort(a, less);
}

} // namespace jessesort::experimental::strict_e731
