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
void presentPreparedPair(FrameinterpolationPresentInfo*,const PreparedPresent*,uint32_t,int64_t&);
namespace {
bool ready=false;
unsigned presents=0, waits=0, submits=0, resets=0;
VkResult waitResult=VK_SUCCESS,submitResult=VK_SUCCESS,resetResult=VK_SUCCESS,createResult=VK_SUCCESS;
bool pairTest=false;
unsigned pairWaits=0,submitsAfterWsi=0;
int64_t presentTimes[2]{};
std::atomic<bool> otherQueueProgress{false};
VkResult VKAPI_PTR CreateFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* fence){if(createResult==VK_SUCCESS)*fence=VkFence(19);return createResult;}
VkResult VKAPI_PTR Reset(VkDevice,uint32_t,const VkFence*){++resets;return resetResult;}
VkResult VKAPI_PTR Submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence){if(pairTest&&presents)++submitsAfterWsi;++submits;return submitResult;}
VkResult VKAPI_PTR Wait(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t timeout){
    ++waits;
    if(pairTest&&pairWaits++==0)Sleep(20); // GPU readiness must precede the pacing timestamp
    if(pairTest){ready=waitResult==VK_SUCCESS;return waitResult;}
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
    if(pairTest&&presents<=2){LARGE_INTEGER now;QueryPerformanceCounter(&now);presentTimes[presents-1]=now.QuadPart;}
    if(!ready || info->waitSemaphoreCount!=1 || (*info->pImageIndices!=2&&!(pairTest&&*info->pImageIndices==3)) || *info->pWaitSemaphores!=VkSemaphore(*info->pImageIndices+10))return VK_ERROR_UNKNOWN;
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
    createResult=VK_SUCCESS;
    for(bool serial:{false,true}){
        FrameinterpolationPresentInfo info{};InitializeCriticalSection(&info.swapchainCriticalSection);
        info.device=reinterpret_cast<VkDevice>(1);info.presentQueue.queue=reinterpret_cast<VkQueue>(2);
        info.serializePresentation=serial;info.frameRenderedSemaphores[2]=VkSemaphore(12);info.frameRenderedSemaphores[3]=VkSemaphore(13);
        pairTest=true;presents=pairWaits=submitsAfterWsi=0;
        LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
        const PreparedPresent pair[]={{2,uint64_t(frequency.QuadPart/200)},{3,uint64_t(frequency.QuadPart/200)}};
        int64_t previous=0;presentPreparedPair(&info,pair,2,previous);
        const double gap=1000.*(presentTimes[1]-presentTimes[0])/frequency.QuadPart;
        std::printf("pair serial=%u GPU waits=%u submits after WSI=%u present gap=%.3f ms\n",unsigned(serial),pairWaits,submitsAfterWsi,gap);
        DeleteCriticalSection(&info.swapchainCriticalSection);
        if(presents!=2 || info.lastPresentResult!=VK_SUCCESS)return 6;
        if(serial&&(pairWaits!=1||submitsAfterWsi||gap<4.0))return 7;
        if(!serial&&pairWaits!=2)return 8;
    }
    std::puts("PASS: image ready before driver present, independent queue progress, bounded wait, sticky timeout/submit/reset failures and acquired-image semaphore.");
    return 0;
}
