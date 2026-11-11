#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rvk::budget {
// Whole smoke steps cannot be interrupted. Bound their cubic grid cost first,
// then spread due steps across frames instead of letting overdue work bypass it.
inline uint32_t SmokeCells(uint32_t requested,float ms,float scale) {
    if(ms<=0.f)return requested;
    const float cells=std::cbrt(std::max(ms,.05f)/(std::max(scale,.1f)*1.e-6f));
    const uint32_t cap=uint32_t(std::max(32.f,std::floor(cells/16.f)*16.f));
    return std::min(requested,cap);
}
inline float SmokeScale(float previous,double measured,double estimate) {
    const float sample=float(std::clamp(measured/std::max(estimate,1.e-6),.1,256.));
    // Learn an underestimate promptly; recover more slowly to avoid oscillation.
    return std::clamp(previous+(sample-previous)*(sample>previous?.5f:.02f),.1f,256.f);
}
inline bool MaterialCold(uint64_t frame,uint64_t published,uint64_t drawn) {
    const uint64_t used=std::max(published,drawn);
    return frame>used && frame-used>180;
}
}
