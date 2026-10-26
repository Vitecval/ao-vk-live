// ICC regression: a nearly full frame ring must not discard the smoke pass's compute pipeline.
#include "rvk.h"
#include <cstdio>
#include <unordered_map>

namespace {
std::unordered_map<VkCommandBuffer, bool> bound;
PFN_vkBeginCommandBuffer beginReal;
PFN_vkCmdBindPipeline bindReal;
PFN_vkCmdDispatch dispatchReal;
unsigned missingPipeline = 0, dispatches = 0;

VkResult VKAPI_PTR Begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo* info)
{
    bound[cmd] = false;
    return beginReal(cmd, info);
}
void VKAPI_PTR Bind(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipeline pipeline)
{
    if (point == VK_PIPELINE_BIND_POINT_COMPUTE) bound[cmd] = pipeline != VK_NULL_HANDLE;
    bindReal(cmd, point, pipeline);
}
void VKAPI_PTR Dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z)
{
    ++dispatches;
    if (!bound[cmd]) {
        // Report the invalid call without forwarding it into a driver that can crash on it.
        ++missingPipeline;
        return;
    }
    dispatchReal(cmd, x, y, z);
}
}

namespace rvk {
struct SmokeRingCheck {
    static bool Run(Device& dev)
    {
        Device::SmokeParams params;
        params.enable = true;
        params.resolution = 4;
        params.quality = 1;
        dev.SetSmokeParams(params);
        dev.SetHdr(true);
        Device::SetFixedClock(1.0 / 60.0);
        unsigned flushes = 0;
        for (int i = 0; i < 8; ++i) {
            params.half = (i % 2) == 0;
            dev.SetSmokeParams(params);
            dev.BeginFrame();
            dev.Clear(d3d::CLEAR_TARGET | d3d::CLEAR_ZBUFFER, 0xff303030, 1.0f);
            Device::SmokeSource source;
            source.pos[2] = 8;
            source.size = 4;
            dev.SmokeSourceFrame(1, source);
            // No fabricated GPU data: skip unused space so the next reservation has to submit and restart.
            auto& frame = dev.m_frames[dev.m_frameIndex];
            frame.ringOffset = frame.ringSize - 32 * 1024;
            auto serial = dev.m_submitted;
            dev.EndScene();
            flushes += dev.m_submitted > serial;
            dev.EndFrame();
        }
        dev.WaitIdle();
        std::printf("smoke ring: %u forced flushes, %u dispatches, %u missing compute pipelines\n",
                    flushes, dispatches, missingPipeline);
        return flushes == 8 && dispatches > 0 && missingPipeline == 0 && !dev.m_deviceLost;
    }
};
}

int main()
{
    rvk::Device dev;
    std::string error;
    if (!dev.Init(nullptr, 320, 240, &error)) {
        std::fprintf(stderr, "init failed: %s\n", error.c_str());
        return 2;
    }
    beginReal = rvk::vk::vkBeginCommandBuffer;
    bindReal = rvk::vk::vkCmdBindPipeline;
    dispatchReal = rvk::vk::vkCmdDispatch;
    rvk::vk::vkBeginCommandBuffer = Begin;
    rvk::vk::vkCmdBindPipeline = Bind;
    rvk::vk::vkCmdDispatch = Dispatch;
    const bool ok = rvk::SmokeRingCheck::Run(dev);
    rvk::vk::vkBeginCommandBuffer = beginReal;
    rvk::vk::vkCmdBindPipeline = bindReal;
    rvk::vk::vkCmdDispatch = dispatchReal;
    return ok ? 0 : 1;
}
