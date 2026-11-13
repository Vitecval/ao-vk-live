#include "internal.h"

namespace rvk {
// Preserve old frontend calls without loading population data, allocating
// actors, scanning a cache, or suppressing the game's own geometry.
void Device::SetLivingContext(uint32_t, bool, uint64_t) {}
void Device::PrepareLivingFrame() {}
bool Device::SuppressLivingCached(uint32_t, uint32_t, const float*, const float*) const { return false; }
bool Device::SuppressLivingOriginal(uint32_t, const void*, uint32_t, const uint16_t*, uint32_t) { return false; }
void Device::DestroyLiving() {}
void Device::DrawLiving() {}
}
