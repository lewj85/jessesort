#ifndef JESSESORT_SIMULATED_PHASE_MAP_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H
#define JESSESORT_SIMULATED_PHASE_MAP_PROBE_ROUTED_ADJACENT_ADAPTIVE_BUFFERED_H

#include <jessesort/detail/pipelines/reference/simulated.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace jessesort::simulated_phase_map {

enum class RouteState : unsigned char { Structured, Entropy, Other };

template <class T>
struct Observation {
    std::size_t values=0, ascPiles=0, descPiles=0, longest=0, ascValues=0, descValues=0;
};

template <class T, class Less>
Observation<T> observe32(const std::vector<T>& x, std::size_t s, Less less) {
    constexpr std::size_t W=32, M=32;
    Observation<T> o;
    if (s>=x.size()) return o;
    const std::size_t m=std::min(W,x.size()-s); o.values=m;
    std::array<std::size_t,M> at{},dt{},ac{},dc{}; std::size_t na=0,nd=0;
    bool desc=m>=2 && less(x[s+1],x[s]);
    auto ap=[&](std::size_t vi){
        std::ptrdiff_t i=-1; std::size_t st=na?(std::size_t{1}<<(std::bit_width(na)-1)):0;
        for(;st;st>>=1){std::size_t n=(std::size_t)(i+(std::ptrdiff_t)st);if(n<na&&less(x[s+vi],x[s+at[n]]))i=(std::ptrdiff_t)n;}
        return(std::size_t)(i+1);
    };
    auto dp=[&](std::size_t vi){
        std::ptrdiff_t i=-1; std::size_t st=nd?(std::size_t{1}<<(std::bit_width(nd)-1)):0;
        for(;st;st>>=1){std::size_t n=(std::size_t)(i+(std::ptrdiff_t)st);if(n<nd&&less(x[s+dt[n]],x[s+vi]))i=(std::ptrdiff_t)n;}
        return(std::size_t)(i+1);
    };
    for(std::size_t i=0;i<m;++i){
        if(i){if(less(x[s+i-1],x[s+i]))desc=false;else if(less(x[s+i],x[s+i-1]))desc=true;}
        if(desc){auto p=dp(i);if(p==nd){dt[nd]=i;dc[nd]=1;++nd;}else{dt[p]=i;++dc[p];}++o.descValues;}
        else{auto p=ap(i);if(p==na){at[na]=i;ac[na]=1;++na;}else{at[p]=i;++ac[p];}++o.ascValues;}
    }
    o.ascPiles=na;o.descPiles=nd;
    for(std::size_t i=0;i<na;++i)o.longest=std::max(o.longest,ac[i]);
    for(std::size_t i=0;i<nd;++i)o.longest=std::max(o.longest,dc[i]);
    return o;
}

template <class T>
RouteState stateOf(const Observation<T>& o) {
    const std::size_t total=o.ascPiles+o.descPiles;
    if ((total<=2 && o.longest>=24) || (total<=20 && o.longest>=8 && o.ascValues>=20))
        return RouteState::Structured;
    if (o.longest<8) return RouteState::Entropy;
    return RouteState::Other;
}

template <class T, class Less>
int tinyRouteSignature(const std::vector<T>& a, std::size_t s, Less less) {
    const std::size_t e=std::min(a.size(),s+8);
    if(e-s<2) return 0;
    int up=0,down=0;
    for(std::size_t i=s+1;i<e;++i){if(less(a[i-1],a[i]))++up;else if(less(a[i],a[i-1]))++down;}
    if(up>=6 || down>=6) return 1; // either-direction low-pile structural
    return 2;                       // entropy/duplicate/alternating-like local shape
}

template <class T, class Less>
bool routeDiversityGate(const std::vector<T>& a, Less less) {
    if(a.size()<24) return false;
    const std::size_t last=a.size()-8;
    const int s0=tinyRouteSignature(a,0,less);
    const int s3=tinyRouteSignature(a,last,less);
    return s0!=s3;
}

struct PhaseMap { std::vector<std::size_t> bounds; std::vector<RouteState> states; std::size_t probes=0; };

template <class T, class Less>
PhaseMap mapX2Cap8k(const std::vector<T>& v, Less less) {
    PhaseMap m; const std::size_t n=v.size(); if(n==0) return m;
    auto obs=[&](std::size_t p){++m.probes;return stateOf(observe32(v,p,less));};
    std::size_t pos=0,gap=1024,lastStable=0; RouteState cur=obs(0);
    m.bounds.push_back(0);m.states.push_back(cur);
    while(pos+32<n){
        const std::size_t nxt=std::min(n-32,pos+gap); if(nxt<=pos)break;
        const RouteState s=obs(nxt);
        if(s!=cur){
            const std::size_t c1=std::min(n-32,nxt+1024), c2=std::min(n-32,nxt+2048);
            if(c1>nxt&&c2>c1){
                const RouteState s1=obs(c1),s2=obs(c2);
                if(s1==s&&s2==s){
                    const std::size_t b=lastStable+((nxt-lastStable)*3)/4;
                    if(b>m.bounds.back()+2048&&b+2048<n){m.bounds.push_back(b);m.states.push_back(s);}
                    cur=s;pos=c2;lastStable=c2;gap=1024;continue;
                }
            }
            pos=nxt;gap=1024;
        }else{lastStable=nxt;pos=nxt;gap=std::min<std::size_t>(8192,gap*2);}
        if(pos>=n-32)break;
    }
    m.bounds.push_back(n);return m;
}

template <class T, class Less>
void mergeRuns(std::vector<T>& a,const std::vector<std::size_t>& initial,Less less){
    if(initial.size()<=2)return;
    std::vector<std::size_t> starts=initial; std::vector<T> tmp(a); bool srcA=true;
    while(starts.size()>2){
        auto& src=srcA?a:tmp; auto& dst=srcA?tmp:a;
        std::vector<std::size_t> next;next.reserve((starts.size()+1)/2);next.push_back(0);
        const std::size_t runs=starts.size()-1;
        for(std::size_t r=0;r<runs;r+=2){
            const std::size_t b=starts[r],mid=starts[r+1];
            if(r+1<runs){const std::size_t e=starts[r+2];std::merge(src.begin()+b,src.begin()+mid,src.begin()+mid,src.begin()+e,dst.begin()+b,less);next.push_back(e);}
            else{std::move(src.begin()+b,src.begin()+mid,dst.begin()+b);next.push_back(mid);}
        }
        starts.swap(next);srcA=!srcA;
    }
    if(!srcA)a.swap(tmp);
}

template <class T, class Less=std::less<T>>
void sort(std::vector<T>& a,Less less=Less{}){
    constexpr std::size_t kMinMapN=65536;
    if(a.size()<kMinMapN){jessesort::simulated_legacy::sort(a,less);return;}
    if(!routeDiversityGate(a,less)){jessesort::simulated_legacy::sort(a,less);return;}
    PhaseMap map=mapX2Cap8k(a,less); auto bounds=map.bounds;
    if(bounds.size()<=2){jessesort::simulated_legacy::sort(a,less);return;}
    for(std::size_t r=0;r+1<bounds.size();++r){
        const std::size_t b=bounds[r],e=bounds[r+1];
        std::vector<T> chunk(a.begin()+static_cast<std::ptrdiff_t>(b),a.begin()+static_cast<std::ptrdiff_t>(e));
        jessesort::simulated_legacy::sort(chunk,less);
        std::move(chunk.begin(),chunk.end(),a.begin()+static_cast<std::ptrdiff_t>(b));
    }
    mergeRuns(a,bounds,less);
}

} // namespace jessesort::simulated_phase_map
#endif
