#pragma once
#include <cstdint>

namespace rvk::foliage {
// Soft glow, beam and screen-effect cards have transparency without leaf
// boundaries. Use the same evidence for canopy motion and material shading.
inline constexpr float kMinimumHardAlphaEdges = 0.002f;
inline bool HasCutoutEdges(bool opaque, float hardAlphaEdges) {
    return !opaque && hardAlphaEdges >= kMinimumHardAlphaEdges;
}
// Captured screen wash, scanline overlay and agency lettering. Their alpha is
// authored display coverage, including hard glyph edges; none is a leaf mask.
inline bool AuthoredScreen(uint32_t type, uint32_t id) {
    return type == 1010004u && (id == 258411u || id == 258412u || id == 285825u);
}
inline bool HasFoliageEdges(uint32_t type, uint32_t id, bool opaque, float hardAlphaEdges) {
    return !AuthoredScreen(type, id) && HasCutoutEdges(opaque, hardAlphaEdges);
}
}
