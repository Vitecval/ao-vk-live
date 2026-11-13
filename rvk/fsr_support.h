#pragma once
#include <cstdint>

namespace rvk::detail {
// Requirements of the shipped FP32 shaders, independent of GPU vendor names.
struct FsrHardware {
    bool computeSubgroups, quad, arithmetic, extendedStorage, timeline;
    uint32_t invocations, workgroupX, workgroupY, workgroupZ, subgroupSize, queues;
};
inline const char* FsrUnsupported(const FsrHardware& c, bool fsr2, bool framegen) {
    if (!c.computeSubgroups || !c.quad) return "compute subgroup quad operations unavailable";
    if (!c.extendedStorage) return "extended storage image formats unavailable";
    if (c.invocations < 256 || c.workgroupX < 256 || c.workgroupY < 16 || c.workgroupZ < 4)
        return "FSR compute workgroup limits unavailable";
    if (fsr2 && c.subgroupSize != 32 && c.subgroupSize != 64)
        return "FSR 2 backend requires a native subgroup of 32 or 64";
    if (framegen && !c.arithmetic) return "frame generation subgroup arithmetic unavailable";
    if (framegen && !c.timeline) return "timeline semaphores unavailable";
    if (framegen && c.queues < 3) return "three independent graphics queues unavailable";
    return nullptr;
}
}
