// Exercise the production SDK present boundary with an unready driver image.
// GPU waiting must happen outside the shared queue lock, and an error must never
// enter the driver or reuse a potentially pending fence. No AO process is used.
#include "ffx_vk_shim.h"
#include "FrameInterpolationSwapchainVK.h"
#include <cstdio>
#include <thread>
#include <atomic>

using namespace rvk::vk;
VkResult presentToSwapChain(FrameinterpolationPresentInfo*,uint32_t,uint32_t);
namespace {
bool ready=false;
unsigned presents=0, waits=0, submits=0, resets=0;
VkResult waitResult=VK_SUCCESS,submitResult=VK_SUCCESS,resetResult=VK_SUCCESS,createResult=VK_SUCCESS;
std::atomic<bool> otherQueueProgress{false};
VkResult VKAPI_PTR CreateFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* fence){if(createResult==VK_SUCCESS)*fence=VkFence(19);return createResult;}
VkResult VKAPI_PTR Reset(VkDevice,uint32_t,const VkFence*){++resets;return resetResult;}
VkResult VKAPI_PTR Submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence){++submits;return submitResult;}
VkResult VKAPI_PTR Wait(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t timeout){
    ++waits;
    // Another queue must remain usable while WSI readiness is awaited. Model a
    // driver whose presentation lock otherwise prevents the work that makes progress.
    otherQueueProgress=false;
    std::thread other([]{std::lock_guard<std::mutex> lock(ffxQueueMutex());otherQueueProgress=true;});
    for(unsigned i=0;i<100 && !otherQueueProgress.load();++i)Sleep(1);
    if(!otherQueueProgress){other.detach();return VK_ERROR_UNKNOWN;}
    other.join();
    if(timeout==UINT64_MAX)return VK_ERROR_UNKNOWN;
    ready=waitResult==VK_SUCCESS;return waitResult;
}
VkResult VKAPI_PTR Present(VkQueue,const VkPresentInfoKHR* info){
    ++presents;
    if(!ready || info->waitSemaphoreCount!=1 || *info->pImageIndices!=2 || *info->pWaitSemaphores!=VkSemaphore(12))return VK_ERROR_UNKNOWN;
    return VK_SUCCESS;
}
int Run(VkResult expected,unsigned expectedPresents){
    FrameinterpolationPresentInfo info{};
    InitializeCriticalSection(&info.swapchainCriticalSection);
    info.device=reinterpret_cast<VkDevice>(1);info.presentQueue.queue=reinterpret_cast<VkQueue>(2);
    info.frameRenderedSemaphores[2]=VkSemaphore(12);
    const unsigned prior=presents;
    const VkResult result=presentToSwapChain(&info,2,2);
    if(result!=expected || presents-prior!=expectedPresents){std::fprintf(stderr,"present boundary failed: result %d expected %d; calls %u\n",result,expected,presents-prior);return 1;}
    if(expected!=VK_SUCCESS){
        const unsigned oldSubmits=submits,oldResets=resets,oldWaits=waits;
        if(presentToSwapChain(&info,2,2)!=expected || submits!=oldSubmits || resets!=oldResets || waits!=oldWaits){std::puts("pending/error fence was reused");return 2;}
    }
    DeleteCriticalSection(&info.swapchainCriticalSection);
    return 0;
}
}
int main(){
    vkCreateFence=CreateFence;vkResetFences=Reset;vkQueueSubmit=Submit;vkWaitForFences=Wait;vkQueuePresentKHR=Present;
    if(Run(VK_SUCCESS,1))return 1;
    waitResult=VK_TIMEOUT;ready=false;if(Run(VK_ERROR_DEVICE_LOST,0))return 2;
    waitResult=VK_SUCCESS;submitResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;if(Run(submitResult,0))return 3;
    submitResult=VK_SUCCESS;resetResult=VK_ERROR_DEVICE_LOST;if(Run(resetResult,0))return 4;
    resetResult=VK_SUCCESS;createResult=VK_ERROR_OUT_OF_HOST_MEMORY;if(Run(createResult,0))return 5;
    std::puts("PASS: image ready before driver present, independent queue progress, bounded wait, sticky timeout/submit/reset failures and acquired-image semaphore.");
    return 0;
}
