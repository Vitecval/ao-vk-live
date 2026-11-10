#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rvk::ambient {
// UI 1 is the former 1.33 baseline. Apply this once, after local placement.
inline float Density(float ui) { return std::isfinite(ui) ? std::clamp(ui,0.f,2.f)*1.33f:0.f; }
inline uint32_t Population(float demand) { return uint32_t(std::max(1.f,std::ceil(demand))); }
inline float Hash(int x,int z,uint32_t salt=0) {
    uint32_t h=uint32_t(x)*73856093u ^ uint32_t(z)*19349663u ^ salt;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
    return float(h>>8)*(1.f/16777216.f);
}
inline float Patch(float x,float z,uint32_t zone) {
    // Smooth, stationary habitat patches. Open gaps survive camera movement.
    x=x/19.f;z=z/19.f;
    int ix=int(std::floor(x)),iz=int(std::floor(z));float u=x-ix,v=z-iz;
    u=u*u*(3-2*u);v=v*v*(3-2*v);
    float a=Hash(ix,iz,zone),b=Hash(ix+1,iz,zone),c=Hash(ix,iz+1,zone),d=Hash(ix+1,iz+1,zone);
    return (a*(1-u)+b*u)*(1-v)+(c*(1-u)+d*u)*v;
}
inline float Frequency(bool automatic,bool sl,bool city,float night) {
    night=std::clamp(night,0.f,1.f);
    if(!automatic)return .50f+.25f*night;
    if(city)return sl ? .20f+.12f*night:.14f+.18f*night;
    return sl ? .40f+.22f*night:.26f+.34f*night;
}
}
