#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "area_profiles.h"
namespace rvk::ambient {
// UI 1 is the former 1.33 baseline. Apply this once, after local placement.
inline float Density(float ui) { return std::isfinite(ui) ? std::clamp(ui,0.f,2.f)*1.33f:0.f; }
// UI 1 is the former brightness 1.5; affects particles, never scene exposure.
inline float Brightness(float ui) { return std::isfinite(ui) ? std::clamp(ui,0.f,2.f)*1.5f:0.f; }
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
    return .80f+.10f*night;
}
// Shader kinds: dust=0, spores=2, fireflies=4, spirits=5, snow=6, embers=7, ash=8, pollen=9.
inline float Kind(uint32_t zone,uint32_t colour,float pick) {
    using areas::Biome;
    const auto biome=areas::For(zone);
    const bool sl=zone>=4000 && zone<5000;
    const float r=float(colour>>16&255),g=float(colour>>8&255),b=float(colour&255);
    const bool green=g>r*1.06f && g>b*1.08f;
    const bool blue=b>r*1.18f && b>g*1.06f;
    const bool violet=r>g*1.16f && b>g*1.16f;
    const bool hot=r>g*1.55f && r>b*1.85f && r>65.f;
    if(biome==Biome::Snow)return 6.f;
    if(biome==Biome::Volcanic)return pick<.83f ? 7.f:8.f;
    if(biome==Biome::Ash)return pick<.85f ? 8.f:5.f;
    if(biome==Biome::Spirit || (sl && biome==Biome::City))return pick<.82f ? 5.f:9.f;
    if(blue || violet)return pick<.75f ? 5.f:2.f;
    if(hot)return pick<.75f ? 7.f:0.f;
    if(biome==Biome::Alien || biome==Biome::Marsh)return pick<.72f ? 2.f:9.f;
    if(green || biome==Biome::Forest)return sl && pick<.24f ? 2.f:9.f;
    if(sl && biome==Biome::Coast)return pick<.65f ? 5.f:9.f;
    return 0.f;
}
}
