#pragma once
#include <cstdint>
#include <cmath>

namespace rvk::lighting {
// The captured Adonis Redeemed temple (4625) uses huge white architectural
// spots with attenuation 0.05 + 0.00002*d*d: up to 20x per light, overlapping
// across the room. Normalize just that authored light family before HDR
// headroom clips away the stone. Ordinary lamps and character lights retain
// the player's intensity settings, including elsewhere in this playfield.
inline float InteriorScale(uint32_t zone,uint32_t type,float range,float attenuation0,float attenuation2) {
    return zone==4625u && type==2u && range>=580.f && range<=590.f &&
        std::fabs(attenuation0-.05f)<.0001f && std::fabs(attenuation2-.00002f)<.000001f ? .05f:1.f;
}
}
