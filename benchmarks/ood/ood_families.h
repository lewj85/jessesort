#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace jessesort::bench::ood {

using Generator = std::function<void(std::vector<int>&, std::mt19937_64&)>;
struct Family { std::string name; Generator generate; };

inline int ri(std::mt19937_64& r, int a, int b) {
    return std::uniform_int_distribution<int>(a, b)(r);
}
inline std::size_t rz(std::mt19937_64& r, std::size_t a, std::size_t b) {
    return std::uniform_int_distribution<std::size_t>(a, b)(r);
}
inline double rd(std::mt19937_64& r, double a, double b) {
    return std::uniform_real_distribution<double>(a, b)(r);
}
inline void clamp_store(std::vector<int>& v, std::size_t i, long long x) {
    x = std::max<long long>(std::numeric_limits<int>::min(),
                            std::min<long long>(std::numeric_limits<int>::max(), x));
    v[i] = static_cast<int>(x);
}

inline std::vector<Family> families() {
    std::vector<Family> f;
    f.push_back({"RunMosaic", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; while(p<n){std::size_t L=std::min(rz(r,16,std::max<std::size_t>(32,n/40)),n-p); bool d=ri(r,0,1); long long x=ri(r,-1000000,1000000),s=ri(r,1,31); for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,x+(d?-(long long)k*s:(long long)k*s)); p+=L;} }});
    f.push_back({"UnevenRunMosaic", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; while(p<n){double u=rd(r,0,1); std::size_t L=(u<.7?rz(r,4,64):rz(r,65,std::max<std::size_t>(128,n/8))); L=std::min(L,n-p); bool d=ri(r,0,1); long long x=ri(r,-10000000,10000000),s=ri(r,1,9); for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,x+(d?-(long long)k*s:(long long)k*s)); p+=L;} }});
    f.push_back({"MonotoneBurstNoise", [](auto& v, auto& r) { std::size_t n=v.size(); std::iota(v.begin(),v.end(),0); int bursts=ri(r,2,20); for(int b=0;b<bursts;b++){std::size_t L=rz(r,4,std::max<std::size_t>(8,n/200)); std::size_t p=rz(r,0,n-L); std::shuffle(v.begin()+p,v.begin()+p+L,r);} }});
    f.push_back({"SparseInversionPatches", [](auto& v, auto& r) { std::size_t n=v.size(); std::iota(v.begin(),v.end(),0); int patches=ri(r,1,32); for(int b=0;b<patches;b++){std::size_t L=rz(r,2,std::max<std::size_t>(3,n/1000)); std::size_t p=rz(r,0,n-L); std::reverse(v.begin()+p,v.begin()+p+L);} }});
    f.push_back({"WindowShuffle", [](auto& v, auto& r) { std::size_t n=v.size(); std::iota(v.begin(),v.end(),0); std::size_t W=rz(r,16,std::max<std::size_t>(32,n/100)); for(std::size_t p=0;p<n;p+=W){std::size_t e=std::min(n,p+W); if(ri(r,0,3)==0) std::shuffle(v.begin()+p,v.begin()+e,r);} }});
    f.push_back({"PlateauStaircase", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; int val=ri(r,-5000,5000); while(p<n){std::size_t L=std::min(rz(r,1,std::max<std::size_t>(2,n/200)),n-p); int step=ri(r,0,9); if(ri(r,0,4)==0) step=-step; for(std::size_t k=0;k<L;k++)v[p+k]=val; val+=step; p+=L;} }});
    f.push_back({"DuplicateRunMosaic", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; while(p<n){std::size_t L=std::min(rz(r,16,std::max<std::size_t>(32,n/100)),n-p); int base=ri(r,-256,256),step=ri(r,0,3); bool d=ri(r,0,1); for(std::size_t k=0;k<L;k++)v[p+k]=base+(d?-(int)(k*step):(int)(k*step)); p+=L;} }});
    f.push_back({"VariableCardinality", [](auto& v, auto& r) { int card=ri(r,2,4096); for(int& x:v)x=ri(r,0,card-1); }});
    f.push_back({"ClusteredDuplicates", [](auto& v, auto& r) { std::size_t n=v.size(); int centers=ri(r,4,64); std::vector<int> c(centers); for(int& x:c)x=ri(r,-100000,100000); for(std::size_t i=0;i<n;i++){int z=c[ri(r,0,centers-1)]; v[i]=z+ri(r,-3,3);} }});
    f.push_back({"InterleavedLanes", [](auto& v, auto& r) { std::size_t n=v.size(); int lanes=ri(r,3,17); std::vector<long long>x(lanes); for(auto& z:x)z=ri(r,-1000000,1000000); std::vector<int>s(lanes); for(auto& z:s)z=ri(r,1,19); for(std::size_t i=0;i<n;i++){int q=i%lanes; clamp_store(v,i,x[q]); x[q]+=s[q];} }});
    f.push_back({"AlternatingWithJitter", [](auto& v, auto& r) { std::size_t n=v.size(); long long lo=0,hi=n*4; for(std::size_t i=0;i<n;i++){long long x=(i&1)?hi:lo; x+=ri(r,-16,16); clamp_store(v,i,x); if(i&1)hi+=ri(r,1,5); else lo-=ri(r,1,5);} }});
    f.push_back({"WarpedBitonic", [](auto& v, auto& r) { std::size_t n=v.size(); double peak=rd(r,.15,.85),a=rd(r,.6,2.4),b=rd(r,.6,2.4); std::size_t m=(std::size_t)(peak*n); for(std::size_t i=0;i<n;i++){double y; if(i<=m){double t=m?double(i)/m:0;y=std::pow(t,a);} else {double t=double(i-m)/std::max<std::size_t>(1,n-1-m);y=std::pow(1.0-t,b);} clamp_store(v,i,(long long)(y*100000000.0)+ri(r,-2,2));} }});
    f.push_back({"AsymmetricPipePlateau", [](auto& v, auto& r) { std::size_t n=v.size(),m=rz(r,n/5,4*n/5),plat=rz(r,0,std::max<std::size_t>(1,n/50)); long long x=0; for(std::size_t i=0;i<n;i++){if(i<m)x+=ri(r,1,5); else if(i<m+plat){} else x-=ri(r,1,11); clamp_store(v,i,x);} }});
    f.push_back({"MultiTurnAffine", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; int turns=ri(r,2,12); long long x=ri(r,-100000,100000); for(int t=0;t<=turns && p<n;t++){std::size_t rem=n-p, seg=(t==turns?rem:rz(r,std::max<std::size_t>(2,rem/(turns-t+3)),std::max<std::size_t>(2,2*rem/(turns-t+2)))); seg=std::min(seg,rem); long long s=ri(r,1,23)*(t%2?-1:1); if(ri(r,0,1))s=-s; for(std::size_t k=0;k<seg;k++){clamp_store(v,p+k,x);x+=s;} p+=seg;} }});
    f.push_back({"OffsetRotationRamp", [](auto& v, auto& r) { std::size_t n=v.size(); long long base=ri(r,-1000000,1000000),step=ri(r,1,17); for(std::size_t i=0;i<n;i++)clamp_store(v,i,base+(long long)i*step); std::size_t q=rz(r,1,n-1); std::rotate(v.begin(),v.begin()+q,v.end()); int jumps=ri(r,1,8); for(int j=0;j<jumps;j++){std::size_t p=rz(r,0,n-1); v[p]+=ri(r,-100,100);} }});
    f.push_back({"JitteredRotation", [](auto& v, auto& r) { std::size_t n=v.size(); for(std::size_t i=0;i<n;i++)v[i]=(int)i+ri(r,-3,3); std::sort(v.begin(),v.end()); std::size_t q=rz(r,1,n-1); std::rotate(v.begin(),v.begin()+q,v.end()); }});
    f.push_back({"DiscontinuousAffinePhases", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; int phases=ri(r,3,16); for(int z=0;z<phases&&p<n;z++){std::size_t rem=n-p,L=(z==phases-1?rem:rz(r,std::max<std::size_t>(4,rem/(phases-z+2)),std::max<std::size_t>(4,2*rem/(phases-z+1)))); L=std::min(L,rem); long long x=ri(r,-10000000,10000000),s=ri(r,-15,15); if(!s)s=1; for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,x+(long long)k*s);p+=L;} }});
    f.push_back({"NoisyAffinePhases", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; int phases=ri(r,2,10); for(int z=0;z<phases&&p<n;z++){std::size_t rem=n-p,L=(z==phases-1?rem:rz(r,std::max<std::size_t>(8,rem/(phases-z+2)),std::max<std::size_t>(8,2*rem/(phases-z+1)))); L=std::min(L,rem); long long x=ri(r,-10000000,10000000),s=ri(r,-9,9); if(!s)s=1; int noise=ri(r,0,20); for(std::size_t k=0;k<L;k++){long long y=x+(long long)k*s;if(ri(r,1,100)<=noise)y+=ri(r,-10000,10000);clamp_store(v,p+k,y);}p+=L;} }});
    f.push_back({"OverlappingSortedBlocks", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; while(p<n){std::size_t L=std::min(rz(r,32,std::max<std::size_t>(64,n/50)),n-p); long long base=ri(r,-100000,100000),s=ri(r,1,5); for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,base+(long long)k*s);p+=L;} }});
    f.push_back({"RandomWalk", [](auto& v, auto& r) { long long x=0; int drift=ri(r,-3,3); for(std::size_t i=0;i<v.size();i++){x+=drift+ri(r,-20,20);clamp_store(v,i,x);} }});
    f.push_back({"StickyRandomWalk", [](auto& v, auto& r) { long long x=0; int dir=1; for(std::size_t i=0;i<v.size();i++){if(ri(r,0,99)<2)dir=-dir; x+=dir*ri(r,0,9);clamp_store(v,i,x);} }});
    f.push_back({"PeriodicPerturbed", [](auto& v, auto& r) { std::size_t n=v.size(); std::size_t period=rz(r,17,std::max<std::size_t>(33,n/200)); int amp=ri(r,1,1000); for(std::size_t i=0;i<n;i++)v[i]=(int)(i%period)*amp+ri(r,-amp/4,amp/4); }});
    f.push_back({"ChunkEntropyMixture", [](auto& v, auto& r) { std::size_t n=v.size(),p=0; while(p<n){std::size_t L=std::min(rz(r,64,std::max<std::size_t>(128,n/20)),n-p); int mode=ri(r,0,3); if(mode==0){long long x=ri(r,-1000000,1000000),s=ri(r,1,11);for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,x+k*s);} else if(mode==1){long long x=ri(r,-1000000,1000000),s=ri(r,1,11);for(std::size_t k=0;k<L;k++)clamp_store(v,p+k,x-(long long)k*s);} else if(mode==2){int card=ri(r,4,128);for(std::size_t k=0;k<L;k++)v[p+k]=ri(r,0,card-1);} else {for(std::size_t k=0;k<L;k++)v[p+k]=ri(r,std::numeric_limits<int>::min(),std::numeric_limits<int>::max());} p+=L;} }});
    f.push_back({"LocalizedAlternatingBursts", [](auto& v, auto& r) { std::size_t n=v.size(); std::iota(v.begin(),v.end(),0); int b=ri(r,1,12); for(int q=0;q<b;q++){std::size_t L=rz(r,8,std::max<std::size_t>(16,n/100));std::size_t p=rz(r,0,n-L);int lo=v[p],hi=v[p+L-1];for(std::size_t k=0;k<L;k++)v[p+k]=(k&1)?hi--:lo++;} }});

    for (int card : {5,10,25,50,100,256,1024,4096,16384,65536}) {
        f.push_back({"RandomCardinalityK" + std::to_string(card), [card](auto& v, auto& r) {
            for (int& x : v) x = ri(r, 0, card - 1);
        }});
    }
    for (std::size_t block : {std::size_t(16),std::size_t(32),std::size_t(64),std::size_t(128),std::size_t(256),std::size_t(512),std::size_t(1024),std::size_t(4096),std::size_t(16384)}) {
        f.push_back({"BlockShuffle" + std::to_string(block), [block](auto& v, auto& r) {
            const std::size_t n=v.size();
            for(std::size_t i=0;i<n;i++)v[i]=ri(r,std::numeric_limits<int>::min(),std::numeric_limits<int>::max());
            for(std::size_t p=0;p<n;p+=block)std::sort(v.begin()+p,v.begin()+std::min(n,p+block));
            const std::size_t nb=(n+block-1)/block;
            std::vector<std::size_t> ord(nb); std::iota(ord.begin(),ord.end(),0); std::shuffle(ord.begin(),ord.end(),r);
            std::vector<int> t(n); std::size_t o=0;
            for(std::size_t b:ord){std::size_t p=b*block,e=std::min(n,p+block);std::copy(v.begin()+p,v.begin()+e,t.begin()+o);o+=e-p;}
            v.swap(t);
        }});
    }
    // Canonical-style replacement noise. Canonical already covers 5% and 10%,
    // so OOD retains only the additional severity levels.
    for (int pct : {1,2,20,30}) {
        f.push_back({"SortedNoise" + std::to_string(pct), [pct](auto& v, auto& r) {
            std::iota(v.begin(),v.end(),0);
            std::bernoulli_distribution change(static_cast<double>(pct) / 100.0);
            std::uniform_int_distribution<int> replacement(0, static_cast<int>(v.size()) - 1);
            for (int& x : v) if (change(r)) x = replacement(r);
        }});
    }
    return f;
}

} // namespace jessesort::bench::ood
