#include "ffx_vk_shim.h"
#include "FrameInterpolationSwapchainVK.h"
#include <thread>
#include <cstdio>
VkResult waitForPresentationCompletion(FrameinterpolationPresentInfo*);
int main(){
    FrameinterpolationPresentInfo info{};
    info.pendingPresentation=2;
    if(waitForPresentationCompletion(&info)!=VK_SUCCESS)return 1; // vendors without serialization
    info.serializePresentation=true;info.pendingPresentation=0;
    if(waitForPresentationCompletion(&info)!=VK_SUCCESS)return 2; // inline/passthrough, no pending WSI
    info.pendingPresentation=2;
    std::atomic<bool> progressed=false;
    const auto began=GetTickCount64();
    std::thread presenter([&]{
        Sleep(20);
        std::lock_guard<std::mutex> lock(ffxQueueMutex());
        progressed=true;info.completedPresentation.store(2,std::memory_order_release);WakeByAddressAll(&info.completedPresentation);
    });
    const auto result=waitForPresentationCompletion(&info);presenter.join();
    if(result!=VK_SUCCESS||!progressed||GetTickCount64()-began<15)return 3;
    info.lastPresentResult=VK_SUBOPTIMAL_KHR;
    if(waitForPresentationCompletion(&info)!=VK_SUBOPTIMAL_KHR)return 4;
    info.lastPresentResult=VK_ERROR_OUT_OF_DATE_KHR;
    if(waitForPresentationCompletion(&info)!=VK_ERROR_OUT_OF_DATE_KHR)return 5;
    info.pendingPresentation=4;info.lastPresentResult=VK_ERROR_DEVICE_LOST;
    if(waitForPresentationCompletion(&info)!=VK_ERROR_DEVICE_LOST)return 6;
    info.lastPresentResult=VK_SUCCESS;
    const auto timeoutBegan=GetTickCount64();
    if(waitForPresentationCompletion(&info)!=VK_ERROR_DEVICE_LOST||GetTickCount64()-timeoutBegan<4900)return 7;
    const auto stickyBegan=GetTickCount64();
    if(waitForPresentationCompletion(&info)!=VK_ERROR_DEVICE_LOST||GetTickCount64()-stickyBegan>1000)return 8;
    std::puts("PASS: actual WSI completion gates submissions, independent queue progress, passthrough/vendor bypass, stale swapchain propagation, bounded timeout and sticky failure");
}
