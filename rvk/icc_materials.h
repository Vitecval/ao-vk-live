#pragma once
#include <cstdint>
namespace rvk::icc {
constexpr uint32_t Playfield = 655;
inline bool Target(uint32_t type,uint32_t id) {
    // Only aliases explicitly belonging to the same resource identity.
    if (type==1010016 || type==1010017) type=1010004;
    if (type==1010021 || type==1010022) type=1010006;
    return (type==1010004 && id==295610) || (type==1010006 && (id==21 || id==22));
}
}
