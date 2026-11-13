#pragma once
#include "vk.h"
#include <cstdint>

namespace rvk::startup {
// Called outside the loader lock. No UI or thread remains after first presentation.
void Begin(bool progressWindow);
void Stage(const char* operation);
void Finish(const char* error = nullptr);
bool Active();
uint64_t DriverMicroseconds();
class DriverCall {
public:
    explicit DriverCall(const char* operation);
    ~DriverCall();
private:
    const char* m_operation;
    int64_t m_begin;
    bool m_active;
};
VkResult GraphicsPipelines(VkDevice, VkPipelineCache, uint32_t, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
VkResult ComputePipelines(VkDevice, VkPipelineCache, uint32_t, const VkComputePipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline*);
}
