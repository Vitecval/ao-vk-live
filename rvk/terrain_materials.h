// Reviewed ground families. Transition tiles retain AO's colour/coverage masks.
#pragma once
#include <cstdint>

namespace rvk::materials {
inline bool IsGroundType(uint32_t type)
{
    return type == 1010006 || type == 1010021 || type == 1010022;
}
inline bool AuthoredGround(uint32_t type, uint32_t id)
{
    return IsGroundType(type) && (id == 17 || (id >= 20 && id <= 26));
}
inline bool AsphaltGround(uint32_t type, uint32_t id)
{
    if (!IsGroundType(type)) return false;
    switch (id) {
    case 18: case 171: case 172: case 180: case 181: case 182:
    case 313: case 315: case 317: case 318: case 324: case 325: case 326:
        return true;
    default: return false;
    }
}
}
