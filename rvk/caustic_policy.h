#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace rvk::caustics {
// All sixteen native/current pairs were reviewed. These are animated light
// fields over existing receivers, never the receiver's physical surface maps.
inline bool Texture(uint32_t type,uint32_t id) {
    return type==1010004u && id>=226088u && id<=226103u;
}
inline bool Receiver(uint32_t type,uint32_t id,bool world3D,bool additiveOneOne,
                     bool depthTest,bool depthWrite,bool depthEqual) {
    return Texture(type,id) && world3D && additiveOneOne && depthTest && !depthWrite && depthEqual;
}
// Millimetre decal tolerance: the unchanged receiver can be split
// into smaller triangles by Shared+ powder tessellation. Exact EQUAL compares
// their finite-precision raster depth against an unsplit light pass and shreds
// the light field into silver staircase edges. The vertex shader projects a
// fixed view-distance tolerance, with one representable-depth-step minimum;
// unlike a constant normalized offset it cannot grow arbitrarily with distance.
inline constexpr float kReceiverTolerance=.001f;
inline constexpr float kReceiverGain=.16f;
inline float Gain(bool hdr,bool enhancedWater,float intensity) {
    // Original graphics/HDR-off and New Water off keep authored brightness.
    // The active enhancement follows the existing 0..2 caustics control.
    if(!hdr || !enhancedWater)return 1.f;
    return kReceiverGain*(std::isfinite(intensity) ? std::clamp(intensity,0.f,2.f):0.f);
}
}
