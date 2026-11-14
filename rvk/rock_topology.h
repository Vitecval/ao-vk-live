#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
namespace rvk::rocks {
// Weld positions rather than vertex indices: normal/UV seams must not be
// mistaken for open mesh borders. Non-manifold edges are pinned too.
struct Position {
    int64_t x,y,z;
    bool operator==(const Position& p)const{return x==p.x&&y==p.y&&z==p.z;}
};
struct Hash {
    size_t operator()(const Position& p)const{
        uint64_t h=uint64_t(p.x)*0x9e3779b97f4a7c15ull;
        h^=uint64_t(p.y)*0xc2b2ae3d27d4eb4full;h^=uint64_t(p.z)*0x165667b19e3779f9ull;
        return size_t(h^(h>>32));
    }
};
inline uint64_t Edge(uint32_t a,uint32_t b){return uint64_t(std::min(a,b))<<32|std::max(a,b);}
inline std::vector<uint32_t> Borders(const void* vertices,uint32_t count,uint32_t stride,uint32_t positionOffset,
                                      const uint16_t* indices,uint32_t indexCount,int normalOffset=-1){
    const uint32_t n=indices?indexCount:count;
    if(!vertices||!count||n%3||positionOffset+12>stride)return {};
    std::unordered_map<Position,uint32_t,Hash> welded;std::vector<uint32_t> owners(count);
    welded.reserve(count);
    for(uint32_t i=0;i<count;++i){
        float p[3];std::memcpy(p,static_cast<const char*>(vertices)+size_t(i)*stride+positionOffset,12);
        for(float v:p)if(!std::isfinite(v)||std::fabs(v)>1e7f)return {};
        const Position key{int64_t(std::llround(double(p[0])*4096)),int64_t(std::llround(double(p[1])*4096)),int64_t(std::llround(double(p[2])*4096))};
        auto entry=welded.emplace(key,uint32_t(welded.size()));owners[i]=entry.first->second;
    }
    std::vector<std::array<float,3>> firstNormal(welded.size());
    std::vector<uint8_t> seen(welded.size()),crease(welded.size());
    if(normalOffset>=0&&uint32_t(normalOffset)+12<=stride)for(uint32_t i=0;i<count;++i){
        float n[3];std::memcpy(n,static_cast<const char*>(vertices)+size_t(i)*stride+normalOffset,12);
        float len=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);const uint32_t o=owners[i];
        if(!std::isfinite(len)||len<1e-8){crease[o]=1;continue;}
        for(float& v:n)v/=len;
        if(!seen[o]){firstNormal[o]={n[0],n[1],n[2]};seen[o]=1;}
        else if(n[0]*firstNormal[o][0]+n[1]*firstNormal[o][1]+n[2]*firstNormal[o][2]<.995f)crease[o]=1;
    }
    std::unordered_map<uint64_t,uint32_t> edges;edges.reserve(n);
    auto at=[&](uint32_t i){return indices?uint32_t(indices[i]):i;};
    for(uint32_t i=0;i<n;++i)if(at(i)>=count)return {};
    for(uint32_t i=0;i<n;i+=3)for(uint32_t e=0;e<3;++e)
        ++edges[Edge(owners[at(i+(e+1)%3)],owners[at(i+(e+2)%3)])];
    std::vector<uint32_t> flags(n/3);
    for(uint32_t i=0;i<n;i+=3)for(uint32_t e=0;e<3;++e){
        auto a=owners[at(i+(e+1)%3)],b=owners[at(i+(e+2)%3)];
        if(a==b||edges[Edge(a,b)]!=2)flags[i/3]|=1u<<e;
        if(crease[a]||crease[b])flags[i/3]|=1u<<(e+3);
    }
    return flags;
}
inline float Level(uint32_t triangles){
    // Fractional odd spacing can round up to the next odd integer. Choose only
    // odd caps so the bound remains 8,000 generated triangles per draw.
    if(!triangles||triangles>8000)return 0;
    return triangles<=98?9.f:triangles<=320?5.f:triangles<=888?3.f:1.f;
}
}
