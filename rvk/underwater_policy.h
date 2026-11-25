#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <span>
namespace rvk::underwater {
inline bool Surface(std::span<const float> levels,uint32_t w,uint32_t h,float ox,float oz,float texel,float x,float z,float& y) {
    if(!w||!h||!(texel>0)||levels.size()!=size_t(w)*h||!std::isfinite(x)||!std::isfinite(z))return false;
    const float u=(x-ox)/texel,v=(z-oz)/texel;
    if(u<0||v<0||u>=float(w)||v>=float(h))return false;
    y=levels[size_t(uint32_t(v))*w+uint32_t(u)];return std::isfinite(y);
}
inline bool Below(float eye,float surface){return std::isfinite(eye)&&std::isfinite(surface)&&eye<surface-.25f;}
template<class Sample> bool BubbleSurface(float x,float z,float& ceiling,Sample sample){
    if(!sample(x,z,ceiling))return false;
    const float centre=ceiling;
    for(int iz=-1;iz<=1;++iz)for(int ix=-1;ix<=1;++ix){float height=0;
        if(!sample(x+float(ix)*.35f,z+float(iz)*.35f,height)||std::fabs(height-centre)>.5f)return false;
        ceiling=std::min(ceiling,height);
    }
    return true;
}
inline bool Fresh(uint64_t state,uint64_t stamp,uint64_t now,uint32_t zone){return zone&&uint32_t(state>>1)==zone&&(state&1)&&stamp&&now>=stamp&&now-stamp<=2000;}
}
