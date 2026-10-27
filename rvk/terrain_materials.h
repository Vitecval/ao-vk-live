// Reviewed ground families. Transition tiles retain AO's colour/coverage masks.
#pragma once
#include <cstdint>

namespace rvk::materials {
inline bool IsGroundType(uint32_t type)
{
    return type == 1010006 || type == 1010021 || type == 1010022;
}
inline bool DeckGround(uint32_t type, uint32_t id)
{
    if (!IsGroundType(type)) return false;
    switch (id) {
    case 17: case 68: case 69: case 71: case 174: case 176:
    case 183: case 184: case 185: case 186: case 187: case 188: case 189:
    case 190: case 191: case 192: case 193: case 194: case 195: case 196: case 197:
    case 209: case 211: return true;
    default: return false;
    }
}
inline bool AuthoredGround(uint32_t type, uint32_t id)
{
    return DeckGround(type,id) || (IsGroundType(type) && id >= 20 && id <= 26);
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
