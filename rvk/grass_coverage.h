#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace rvk::grass {
inline bool Green(const uint8_t* c) {
    return c[1]>24 && int(c[1])*100>int(c[0])*112 && int(c[1])*100>int(c[2])*112;
}
// 64x64 bits (512 bytes), or 128x128 for large baked ground atlases (2048
// bytes), instead of retaining full RGB in the 32-bit client. Interpolate
// coverage, never a nearest-cell on/off field.
inline float Coverage(const uint8_t* bits, float u, float v, bool clamp, int n=64) {
    float x=u*n-.5f,y=v*n-.5f;
    if (clamp) { x=std::clamp(x,0.0f,float(n-1)); y=std::clamp(y,0.0f,float(n-1)); }
    else { x-=std::floor((x+.5f)/n)*n; y-=std::floor((y+.5f)/n)*n; }
    int ix=int(std::floor(x)),iy=int(std::floor(y));
    float fx=x-ix,fy=y-iy;
    auto at=[&](int a,int b) {
        if (clamp) { a=std::clamp(a,0,n-1);b=std::clamp(b,0,n-1); }
        else { a=(a%n+n)%n;b=(b%n+n)%n; }
        int i=b*n+a;return float((bits[i>>3]>>(i&7))&1);
    };
    return (at(ix,iy)*(1-fx)+at(ix+1,iy)*fx)*(1-fy)+
           (at(ix,iy+1)*(1-fx)+at(ix+1,iy+1)*fx)*fy;
}
}
