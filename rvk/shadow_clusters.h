#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rvk::shadow {
struct TriangleCluster {
    uint32_t first, count; // original index range; no reordering or simplification
    float lo[3], hi[3];
};
inline std::vector<TriangleCluster> TerrainClusters(const void* vertices,uint32_t vertexCount,uint32_t stride,
                                                   const uint16_t* indices,uint32_t indexCount) {
    if(!vertices || !indices || stride<12 || indexCount%3 || indexCount<3072 || indexCount>196608)return {};
    constexpr uint32_t batch=96*3;
    std::vector<TriangleCluster> result;
    result.reserve((indexCount+batch-1)/batch);
    const auto* source=static_cast<const uint8_t*>(vertices);
    for(uint32_t first=0;first<indexCount;first+=batch) {
        TriangleCluster c{first,std::min(batch,indexCount-first),{1e30f,1e30f,1e30f},{-1e30f,-1e30f,-1e30f}};
        for(uint32_t at=first;at<first+c.count;++at) {
            if(indices[at]>=vertexCount)return {};
            float p[3];std::memcpy(p,source+size_t(indices[at])*stride,sizeof(p));
            for(int j=0;j<3;++j) {
                if(!std::isfinite(p[j]) || std::fabs(p[j])>1e20f)return {};
                c.lo[j]=std::min(c.lo[j],p[j]);c.hi[j]=std::max(c.hi[j],p[j]);
            }
        }
        // Keep clip-boundary triangles despite CPU/GPU rounding differences.
        for(int j=0;j<3;++j) {
            const float pad=std::max(.01f,std::max(std::fabs(c.lo[j]),std::fabs(c.hi[j]))*1e-5f);
            c.lo[j]-=pad;c.hi[j]+=pad;
        }
        result.push_back(c);
    }
    return result;
}
}
