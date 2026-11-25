// Reviewed ground families. Transition tiles retain AO's colour/coverage masks.
#pragma once
#include <cstdint>

namespace rvk::materials {
inline bool IsGroundType(uint32_t type)
{
    return type == 1010006 || type == 1010021 || type == 1010022;
}
// Snow-bearing native tiles verified from original colour/transition layouts.
// The snow coat uses pale-pixel masks; exposed rock and paths remain visible.
inline bool SnowGround(uint32_t type,uint32_t id)
{
    return IsGroundType(type) && (id==437u || (id>=439u && id<=442u) ||
                                  (id>=692u && id<=696u) || (id>=698u && id<=701u) ||
                                  (id>=742u && id<=747u) || (id>=752u && id<=758u));
}
inline bool IceGround(uint32_t type,uint32_t id)
{
    return IsGroundType(type) && ((id>=692u && id<=696u) ||
        (id>=698u && id<=701u) || (id>=706u && id<=710u));
}
// Retail Shadowlands void marker, not a visible rock/terrain material. Its
// JPEG contains the word "Brink"; drawing it fills the gaps between islands.
inline bool VoidGround(uint32_t type, uint32_t id)
{
    return IsGroundType(type) && id == 774;
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
    if(DeckGround(type,id))return true;
    if(!IsGroundType(type))return false;
    if(id>=20 && id<=26)return true;
    // Native Shadowlands carved floor patterns and their authored transitions.
    // Their radial/cross/angular inlays are UV layouts, not natural grain.
    switch(id) {
    case 364: case 365: case 366: case 371: case 372: case 373: case 374:
    case 703: case 704: case 720: case 724: case 725: case 737: case 739:
    case 915: case 916: case 929: case 931: case 932: case 933: case 934:
    case 935: case 936:return true;
    default:return false;
    }
}
// Exact 0.7.7 authored-layout classification for ordinary Shared Terrain+.
// Later carved-floor classifications belong to the other/enhanced paths.
inline bool ClassicAuthoredGround(uint32_t type,uint32_t id)
{
    return DeckGround(type,id) || (IsGroundType(type) && id>=20 && id<=26);
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
