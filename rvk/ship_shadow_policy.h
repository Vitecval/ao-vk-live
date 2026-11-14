#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace rvk::ship {
// Existing authored hull identities used by navigation lights too. These may
// move despite N3 reporting a Static visual; their old poses must not persist.
constexpr bool Hull(uint32_t type, uint32_t id) {
    if(type!=1010004 && type!=1010016 && type!=1010017)return false;
    for(uint32_t known:{99841u,201230u,202773u,203102u,203531u,203532u})if(id==known)return true;
    return false;
}
inline bool LocalCube(float range,uint32_t faceSize) {
    // A far-face texel wider than half a metre gives little useful local shadow
    // detail. Kilometre-wide atmosphere/ship lights still illuminate normally;
    // they must not repeatedly render the whole town into six low-detail faces.
    return std::isfinite(range) && range>=1.f && range<=float(faceSize)*.25f;
}
}
