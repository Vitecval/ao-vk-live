#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace rvk::ship {
// Existing authored hull identities used by navigation lights too. These may
// move despite N3 reporting a Static visual; their old poses must not persist.
constexpr bool Hull(uint32_t type, uint32_t id) {
    if(type!=1010004 && type!=1010016 && type!=1010017)return false;
    for(uint32_t known:{99841u,201230u,202773u,203102u,203531u,203532u})if(id==known)return true;
    return false;
}
inline bool CapturedMobilePart(const char* visual,uint32_t type,uint32_t id,uint32_t vertices,uint32_t indices) {
    // ICC captures show these GenericMeshObjects travelling through
    // hundreds of placements. Its textures also occur on stationary RTriMesh
    // structures: texture ID alone must never classify those as moving.
    if(!visual || std::strcmp(visual,"GenericMeshObject_t") || type!=1010004)return false;
    struct Part {uint32_t id,vertices,indices;};
    constexpr Part parts[]={{295615,2353,4608},{297313,82,144},{275940,234,354},
                            {296212,465,1164},{297811,34,102},{295916,1495,5184},{295915,1352,5142}};
    for(const auto& p:parts)if(id==p.id && vertices==p.vertices && indices==p.indices)return true;
    return false;
}
inline bool LocalCube(float range,uint32_t faceSize) {
    // A far-face texel wider than half a metre gives little useful local shadow
    // detail. Kilometre-wide atmosphere/ship lights still illuminate normally;
    // they must not repeatedly render the whole town into six low-detail faces.
    return std::isfinite(range) && range>=1.f && range<=float(faceSize)*.25f;
}
}
