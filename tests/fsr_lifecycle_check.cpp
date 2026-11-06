// Controlled Vulkan-only fixture. Creates only its own small no-activate window.
#include "rvk.h"
#include "threaded.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <cwchar>

namespace {
PFN_vkGetDeviceProcAddr realGet;
unsigned injectedCalls=0;
PFN_vkQueuePresentKHR realPresent;
std::atomic<unsigned> driverPresents{0};
HWND messageWindow=nullptr;
std::atomic<unsigned> sentMessages{0},sentTimeouts{0};
std::atomic<int> messagePhase{-1};
PFN_vkQueueSubmit realSubmit;
PFN_vkAcquireNextImageKHR realAcquire;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR realCaps;
PFN_vkWaitSemaphores realWaitSemaphores;
std::atomic<bool> delayedHandoff{false},stoppedHandoff{false};
void HandoffLog(const char* line){std::fprintf(stderr,"%s\n",line);if(std::strstr(line,"frame generation: presentation stopped"))stoppedHandoff=true;}
VkResult VKAPI_PTR DelayHandoff(VkDevice device,const VkSemaphoreWaitInfo* info,uint64_t timeout){
    PWSTR name=nullptr;GetThreadDescription(GetCurrentThread(),&name);
    const bool interpolation=name&&std::wcsstr(name,L"FSR Interpolation");if(name)LocalFree(name);
    if(interpolation&&!delayedHandoff.exchange(true))Sleep(6100);
    return realWaitSemaphores(device,info,timeout);
}
std::atomic<unsigned> acquireCalls{0},acquireTimeouts{0},limitedCapsCalls{0};
bool IsPresenter(){PWSTR name=nullptr;GetThreadDescription(GetCurrentThread(),&name);bool yes=name&&std::wcsstr(name,L"FSR Presenter");if(name)LocalFree(name);return yes;}
VkResult VKAPI_PTR TimeoutAcquire(VkDevice d,VkSwapchainKHR s,uint64_t timeout,VkSemaphore semaphore,VkFence fence,uint32_t* index){
    if(IsPresenter() && (++acquireCalls%9==0)){++acquireTimeouts;return VK_TIMEOUT;}
    return realAcquire(d,s,timeout,semaphore,fence,index);
}
VkResult VKAPI_PTR LimitedCaps(VkPhysicalDevice device,VkSurfaceKHR surface,VkSurfaceCapabilitiesKHR* caps){
    VkResult result=realCaps(device,surface,caps);
    if(result==VK_SUCCESS){caps->maxImageCount=caps->minImageCount;++limitedCapsCalls;}
    return result;
}
struct BinarySubmission {VkQueue queue;VkSemaphore binary;};
std::mutex submissionMutex;
std::vector<BinarySubmission> binarySubmissions;
std::atomic<unsigned> pairedPresentCalls{0},withheldGpuPairs{0};
VkResult VKAPI_PTR ObserveSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo* submits,VkFence fence){
    const VkResult result=realSubmit(queue,count,submits,fence);
    if(result!=VK_SUCCESS)return result;
    for(uint32_t n=0;n<count;++n){
        auto* next=static_cast<const VkBaseInStructure*>(submits[n].pNext);
        while(next&&next->sType!=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)next=next->pNext;
        if(!next)continue;
        const auto& timeline=*reinterpret_cast<const VkTimelineSemaphoreSubmitInfo*>(next);
        bool hasTimeline=false;for(uint32_t i=0;i<timeline.signalSemaphoreValueCount;++i)hasTimeline|=timeline.pSignalSemaphoreValues[i]!=0;
        if(!hasTimeline)continue;
        std::lock_guard<std::mutex> lock(submissionMutex);
        for(uint32_t i=0;i<submits[n].signalSemaphoreCount&&i<timeline.signalSemaphoreValueCount;++i)
            if(!timeline.pSignalSemaphoreValues[i])binarySubmissions.push_back({queue,submits[n].pSignalSemaphores[i]});
    }
    return result;
}
VkResult VKAPI_PTR PairPresent(VkQueue queue,const VkPresentInfoKHR* info){
    const bool presenter=IsPresenter();
    if(presenter && (++pairedPresentCalls&1)){
        bool complete=false;
        {
            std::lock_guard<std::mutex> lock(submissionMutex);
            size_t current=binarySubmissions.size();
            for(size_t i=binarySubmissions.size();i>0;--i)
                if(binarySubmissions[i-1].queue==queue&&binarySubmissions[i-1].binary==info->pWaitSemaphores[0]){current=i-1;break;}
            for(size_t i=current+1;current<binarySubmissions.size()&&i<binarySubmissions.size();++i)
                complete|=binarySubmissions[i].queue==queue;
        }
        if(!complete){++withheldGpuPairs;Sleep(50);} // model a WSI call blocking before the second submission
    }
    return realPresent(queue,info);
}
VkResult VKAPI_PTR MessagePresent(VkQueue queue,const VkPresentInfoKHR* info) {
    DWORD_PTR result=0;
    ++sentMessages;
    if(!SendMessageTimeoutW(messageWindow,WM_NULL,0,0,SMTO_BLOCK|SMTO_ABORTIFHUNG,500,&result)){
        ++sentTimeouts;std::fprintf(stderr,"WSI callback timeout at phase %d thread %lu error %lu\n",messagePhase.load(),GetCurrentThreadId(),GetLastError());
    }
    return realPresent(queue,info);
}
VkResult VKAPI_PTR SlowPresent(VkQueue queue,const VkPresentInfoKHR* info) {
    Sleep(45); // let the SDK's pacer replace pending entries while the presenter is busy
    ++driverPresents;
    return realPresent(queue,info);
}
VkResult VKAPI_PTR RejectPipeline(VkDevice,VkPipelineCache,uint32_t,const VkComputePipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline*) {
    ++injectedCalls; return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}
PFN_vkVoidFunction VKAPI_PTR GetProc(VkDevice device,const char* name) {
    if(std::strcmp(name,"vkCreateComputePipelines")==0)return reinterpret_cast<PFN_vkVoidFunction>(&RejectPipeline);
    return realGet(device,name);
}
}
template<class RenderDevice> int Run(int argc,char**argv) {
    const bool inject=argc>1 && std::string(argv[1])=="--fail-init";
    const bool stress=argc>1 && std::string(argv[1])=="--pacing-stress";
    const bool messages=argc>1 && std::string(argv[1])=="--wsi-send";
    const bool pairs=argc>1 && std::string(argv[1])=="--paired-wsi";
    const bool timeouts=argc>1 && std::string(argv[1])=="--acquire-timeout";
    const bool limited=argc>1 && std::string(argv[1])=="--limited-images";
    const bool handoff=argc>1 && std::string(argv[1])=="--handoff-timeout";
    const bool native=handoff || timeouts || limited || pairs || messages || stress || (argc>1 && (std::string(argv[1])=="--native-framegen" || std::string(argv[1])=="--native-threaded"));
    const bool fg=native || (argc>1 && std::string(argv[1])=="--framegen");
    const unsigned width=native?1914:768,height=native?1051:512;
    using namespace rvk;using namespace rvk::d3d;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AOVKFsrLaboratory";RegisterClassW(&wc);
    RECT bounds{0,0,LONG(width),LONG(height)};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"AO-VK isolated FSR fixture",WS_OVERLAPPEDWINDOW,40,40,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)return 1;
    messageWindow=window;
    {
        RenderDevice d;std::string error;
        if(native){ShowWindow(window,SW_SHOWNOACTIVATE);SetWindowPos(window,pairs?HWND_TOP:HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
        if(!d.Init(window,width,height,&error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
        realGet=vk::vkGetDeviceProcAddr;
        realPresent=vk::vkQueuePresentKHR;
        realSubmit=vk::vkQueueSubmit;
        realAcquire=vk::vkAcquireNextImageKHR;realCaps=vk::vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
        realWaitSemaphores=vk::vkWaitSemaphores;
        if(stress)vk::vkQueuePresentKHR=SlowPresent;
        if(messages)vk::vkQueuePresentKHR=MessagePresent;
        if(pairs){vk::vkQueuePresentKHR=PairPresent;vk::vkQueueSubmit=ObserveSubmit;}
        if(timeouts)vk::vkAcquireNextImageKHR=TimeoutAcquire;
        if(limited){vk::vkGetPhysicalDeviceSurfaceCapabilitiesKHR=LimitedCaps;vk::vkQueuePresentKHR=PairPresent;}
        if(handoff){vk::vkWaitSemaphores=DelayHandoff;rvk::SetLogSink(HandoffLog);}
        if(inject)vk::vkGetDeviceProcAddr=GetProc;
        d.SetHdr(true);d.SetTaa(true,.4f);d.SetRenderScale(native?100:67);d.SetFrameGen(fg&&!native);
        Matrix identity{};for(int i=0;i<4;++i)identity.m[i][i]=1;
        Matrix projection{};projection.m[0][0]=projection.m[1][1]=1;projection.m[2][2]=100.f/99.9f;projection.m[2][3]=1;projection.m[3][2]=-.1f*100.f/99.9f;
        struct V{float x,y,z,nx,ny,nz;uint32_t color;float u,v;};
        const V vertices[]={{-1,-1,3,0,0,-1,0xffffffff,0,1},{1,-1,3,0,0,-1,0xffffffff,1,1},{0,1,3,0,0,-1,0xffffffff,.5f,0}};
        Material material{};material.diffuse=material.ambient={1,1,1,1};
        for(int frame=0;frame<(stress?180:72);++frame){
            messagePhase=frame;
            if(!native&&frame%12==0)d.SetUpscaler(unsigned(frame/12)%3);
            if(!native&&(frame==24 || frame==48))d.SetRenderScale(frame==24?50:80);
            if(native&&frame==12)d.SetFrameGen(true);
            if(messages&&frame==24)d.SetRenderScale(67);
            if(messages&&frame==30&&!d.Resize(640,480))return 3;
            if(messages&&frame==42&&!d.Resize(width,height))return 4;
            if(messages&&frame==48)d.SetRenderScale(100);
            if(!native&&!inject && frame==30){SetWindowLongPtrW(window,GWL_STYLE,WS_POPUP);if(!d.Resize(640,480))return 3;}
            if(!native&&!inject && frame==42){SetWindowLongPtrW(window,GWL_STYLE,WS_OVERLAPPEDWINDOW);if(!d.Resize(768,512))return 4;}
            if(!stress&&fg && frame==54)d.SetFrameGen(false);
            if(!stress&&fg && frame==60)d.SetFrameGen(true);
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
            d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff17212b,1);
            d.SetTransform(World,identity);d.SetTransform(View,identity);d.SetTransform(Projection,projection);
            d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_LIGHTING,1);d.SetRenderState(RS_AMBIENT,0xff808080);d.SetRenderState(RS_CULLMODE,CULL_NONE);
            d.SetMaterial(material);d.SetDrawVisual(uint32_t(VisualKind::Static),"controlled FSR lifecycle fixture");
            d.DrawPrimitive(TriangleList,0x152,vertices,3);
            if(native&&(!stress||frame>=24)){d.InterfaceBegin(true);d.InterfaceEnd();}
            d.EndFrame();
            if(native){std::fprintf(stderr,"native toggle frame %d completed\n",frame);Sleep(stress?0:16);}
        }
        messagePhase=72;
        if(inject){vk::vkGetDeviceProcAddr=realGet;if(!injectedCalls){std::puts("failure injection was not exercised");return 5;}std::printf("injected pipeline failures: %u; contexts must be logged once per mode\n",injectedCalls);}
    }
    DestroyWindow(window);
    if(handoff){rvk::SetLogSink(nullptr);std::printf("handoff delayed=%u stopped/logged=%u\n",unsigned(delayedHandoff.load()),unsigned(stoppedHandoff.load()));if(!delayedHandoff||!stoppedHandoff)return 11;}
    if(timeouts){std::printf("injected real-image acquisition timeouts: %u\n",acquireTimeouts.load());if(!acquireTimeouts)return 9;}
    if(limited){std::printf("limited surface queries %u; framegen presenter calls %u\n",limitedCapsCalls.load(),pairedPresentCalls.load());if(!limitedCapsCalls || pairedPresentCalls)return 10;}
    if(pairs){
        std::printf("paired WSI: %u presenter calls, %u calls withheld the real-frame GPU submission\n",pairedPresentCalls.load(),withheldGpuPairs.load());
        if(pairedPresentCalls<72 || withheldGpuPairs)return 8;
    }
    if(messages){
        std::printf("WSI window callbacks: %u sends, %u timed out\n",sentMessages.load(),sentTimeouts.load());
        if(sentMessages<72 || sentTimeouts!=0)return 7;
    }
    if(stress){
        std::printf("slow presenter: %u driver presents for 180 rendered frames\n",driverPresents.load());
        if(driverPresents<=180){std::puts("interpolated presentations were not exercised");return 6;}
    }
    std::puts("controlled FSR lifecycle completed; no AO client was used");
    return 0;
}
int main(int argc,char**argv) {
    if(argc>1 && (std::string(argv[1])=="--native-threaded"||std::string(argv[1])=="--pacing-stress"||std::string(argv[1])=="--wsi-send"||std::string(argv[1])=="--paired-wsi"||std::string(argv[1])=="--acquire-timeout"||std::string(argv[1])=="--limited-images"||std::string(argv[1])=="--handoff-timeout"))return Run<rvk::ThreadedDevice>(argc,argv);
    return Run<rvk::Device>(argc,argv);
}
