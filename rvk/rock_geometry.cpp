#include "internal.h"
namespace rvk {
// Legacy RVK_RockShape values cannot enable geometry changes. Enhanced Rocks
// is the original surface-detail effect; each game mesh is drawn exactly once.
bool Device::RockShapeDraw(uint32_t,uint32_t,const detail::FvfLayout&,const void*,uint32_t,const uint16_t*,uint32_t,
                          detail::DrawTransform&,VkBuffer&,VkDeviceSize&,VkDeviceSize&){return false;}
}
