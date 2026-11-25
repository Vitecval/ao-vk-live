#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace rvk::lava {
inline constexpr uint32_t MaxSurfaceLights=8,MaxShadowLights=4;
// At most 49 world-grid samples, clipped to submitted triangles. Sampling a
// bounding box would put lamps on dry islands and outside the liquid itself.
template<class Vertex> std::vector<std::array<float,3>> Samples(const Vertex* vertices,uint32_t count,
    const uint16_t* indices,uint32_t indexCount,const float* eye)
{
    std::vector<std::array<float,3>> points;
    if(!vertices || !indices || !eye)return points;
    for(int z=-3;z<=3;++z)for(int x=-3;x<=3;++x){
        float px=(std::floor(eye[0]/12.f)+float(x))*12.f;
        float pz=(std::floor(eye[2]/12.f)+float(z))*12.f;
        for(uint32_t t=0;t+2<indexCount;t+=3){
            if(indices[t]>=count || indices[t+1]>=count || indices[t+2]>=count)continue;
            const float* a=vertices[indices[t]].pos;const float* b=vertices[indices[t+1]].pos;const float* c=vertices[indices[t+2]].pos;
            float det=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);
            if(!std::isfinite(det) || std::abs(det)<.01f)continue;
            float u=((b[2]-c[2])*(px-c[0])+(c[0]-b[0])*(pz-c[2]))/det;
            float v=((c[2]-a[2])*(px-c[0])+(a[0]-c[0])*(pz-c[2]))/det;
            if(u<0.f || v<0.f || u+v>1.f)continue;
            float py=u*a[1]+v*b[1]+(1.f-u-v)*c[1]+1.5f;
            if(std::isfinite(py) && std::abs(py-eye[1])<48.f)points.push_back({px,py,pz});
            break;
        }
    }
    auto dist=[&](const auto& p){float d=0.f;for(int i=0;i<3;++i)d+=(p[i]-eye[i])*(p[i]-eye[i]);return d;};
    std::stable_sort(points.begin(),points.end(),[&](const auto& a,const auto& b){return dist(a)<dist(b);});
    if(points.size()>MaxSurfaceLights)points.resize(MaxSurfaceLights);
    return points;
}
}
