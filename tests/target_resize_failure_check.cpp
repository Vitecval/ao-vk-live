// Actual offscreen Vulkan allocation/readback regression. Fault hooks live only
// in this executable; no game files, HWND or desktop mode are involved.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "internal.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <tuple>
#include <vector>

namespace {
unsigned failTextureAt=0,textureAttempts=0,hostFaults=0;
bool failNextHost=false,trackAllocations=false;
std::array<void*,512> textureSized{};
unsigned trackedHost=0;
void Remember(void* p){for(auto& entry:textureSized)if(!entry){entry=p;++trackedHost;return;}std::abort();}
void Forget(void* p){for(auto& entry:textureSized)if(entry==p){entry=nullptr;--trackedHost;return;}}
}

void* operator new(std::size_t bytes) {
    if(failNextHost || (failTextureAt && bytes==sizeof(rvk::Texture) && ++textureAttempts==failTextureAt)) {
        failNextHost=false;failTextureAt=0;++hostFaults;throw std::bad_alloc();
    }
    void* p=std::malloc(bytes ? bytes:1);if(!p)throw std::bad_alloc();
    if(trackAllocations && bytes==sizeof(rvk::Texture))Remember(p);
    return p;
}
void operator delete(void* p) noexcept {if(p){Forget(p);std::free(p);}}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}

namespace rvk {
struct TargetResizeFailureCheck {
    static void Quiesce(Device& d){d.WaitIdle();d.m_completed=d.m_submitted;d.CollectGarbage();}
    static auto Targets(const Device& d){return std::make_tuple(d.m_width,d.m_height,d.m_renderScale,
        d.m_ldrMain,d.m_scene,d.m_glow,d.m_localFraction,d.m_motionVectors,d.m_albedo,d.m_main,d.m_target,
        d.m_depth,d.m_depthView,d.m_depthAllocation,d.m_depthLayout,d.m_depthWidth,d.m_depthHeight,
        d.m_otherDepth.image,d.m_otherDepth.view,d.m_otherDepth.allocation,d.m_otherDepth.layout,d.m_otherDepth.width,d.m_otherDepth.height,
        d.m_depthSwapped,d.m_readback,d.m_readbackAllocation,d.m_readbackData,
        d.m_viewport.x,d.m_viewport.y,d.m_viewport.width,d.m_viewport.height,d.m_viewport.minZ,d.m_viewport.maxZ);}
    static auto Allocations(Device& d){
        VmaTotalStatistics stats{};vmaCalculateStatistics(d.m_allocator,&stats);
        return std::make_tuple(stats.total.statistics.allocationCount,stats.total.statistics.allocationBytes,
            d.m_textureCount[0],d.m_textureCount[1],d.m_textureBytes[0],d.m_textureBytes[1],
            size_t(d.m_nextBindlessImage)-d.m_freeBindlessImages.size());
    }
    static bool Bindless(const Device& d){return d.m_bindless && d.m_bindlessSet!=VK_NULL_HANDLE;}
    static std::vector<uint32_t> DetachFreeSlots(Device& d){
        std::vector<uint32_t> slots;slots.swap(d.m_freeBindlessImages);return slots;
    }
    static void RestoreFreeSlots(Device& d,std::vector<uint32_t>& slots){d.m_freeBindlessImages.swap(slots);}
};
}

namespace {
PFN_vkCreateImageView realCreate=nullptr;
PFN_vkDestroyImageView realDestroy=nullptr;
unsigned failViewAt=0,viewAttempts=0,viewFaults=0;
bool throwView=false,failHostAfterView=false;
std::array<VkImageView,128> views{};
unsigned liveViews=0;
struct InjectedCreationException {};
VkResult VKAPI_PTR CreateView(VkDevice device,const VkImageViewCreateInfo* info,const VkAllocationCallbacks* callbacks,VkImageView* out){
    ++viewAttempts;
    if(failViewAt && viewAttempts==failViewAt){
        ++viewFaults;*out=VK_NULL_HANDLE;
        if(throwView)throw InjectedCreationException{};
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    const auto result=realCreate(device,info,callbacks,out);
    if(result==VK_SUCCESS){
        bool stored=false;for(auto& entry:views)if(!entry){entry=*out;stored=true;++liveViews;break;}
        assert(stored);
        if(failHostAfterView){failHostAfterView=false;failNextHost=true;}
    }
    return result;
}
void VKAPI_PTR DestroyView(VkDevice device,VkImageView view,const VkAllocationCallbacks* callbacks){
    for(auto& entry:views)if(entry==view){entry=VK_NULL_HANDLE;--liveViews;break;}
    realDestroy(device,view,callbacks);
}
void ResetFaults(){failTextureAt=textureAttempts=hostFaults=failViewAt=viewAttempts=viewFaults=0;
    failNextHost=throwView=failHostAfterView=false;}
constexpr uint32_t W=96,H=64,Colour=0xFF356789;
void Paint(rvk::Device& device){
    device.SetHdr(false);device.BeginFrame();device.Clear(rvk::d3d::CLEAR_TARGET|rvk::d3d::CLEAR_ZBUFFER,Colour,1);
    device.EndFrame();rvk::TargetResizeFailureCheck::Quiesce(device);
}
std::vector<uint32_t> Read(rvk::Device& device){
    std::vector<uint32_t> pixels(size_t(device.Width())*device.Height());
    assert(device.ReadPixels(nullptr,pixels.data()));return pixels;
}
}

int main(){
    using rvk::TargetResizeFailureCheck;
    std::setvbuf(stdout,nullptr,_IONBF,0);
    unsigned cases=0;
    {
        rvk::Device device;std::string error;
        assert(device.Init(nullptr,W,H,&error));
        for(unsigned i=0;i<3;++i)Paint(device);
        const auto reference=Read(device);
        assert(reference.size()==W*H);
        for(uint32_t pixel:reference)assert(pixel==Colour);
        realCreate=rvk::vk::vkCreateImageView;realDestroy=rvk::vk::vkDestroyImageView;
        rvk::vk::vkCreateImageView=CreateView;rvk::vk::vkDestroyImageView=DestroyView;trackAllocations=true;
        const auto check=[&](const char* name,auto arm,auto disarm){
            ResetFaults();TargetResizeFailureCheck::Quiesce(device);
            const auto oldTargets=TargetResizeFailureCheck::Targets(device);
            const auto oldAllocations=TargetResizeFailureCheck::Allocations(device);
            const unsigned oldViews=liveViews,oldHost=trackedHost;
            arm();bool result=true,escaped=false;
            try {result=device.Resize(128,80);}catch(...){escaped=true;}
            failTextureAt=0;failNextHost=false;failHostAfterView=false;
            disarm();
            assert(!escaped && !result && hostFaults+viewFaults==1);
            assert(TargetResizeFailureCheck::Targets(device)==oldTargets);
            assert(device.Width()==W && device.Height()==H && device.SceneWidth()==W && device.SceneHeight()==H);
            assert(liveViews==oldViews && trackedHost==oldHost);
            assert(TargetResizeFailureCheck::Allocations(device)==oldAllocations);
            assert(Read(device)==reference); // actual old GPU colour contents, not just metadata
            ResetFaults();
            assert(device.Resize(128,80));Paint(device);
            auto recovered=Read(device);assert(recovered.size()==128*80);
            for(uint32_t pixel:recovered)assert(pixel==Colour);
            assert(device.Resize(W,H));Paint(device);assert(Read(device)==reference);
            std::printf("PASS %s: target identity, VMA allocations, descriptors, host objects and old pixels retained; resize recovered\n",name);
            ++cases;
        };
        // Six colour targets, followed by the scene depth view. Return-code
        // failures exercise every partial construction prefix including depth.
        for(unsigned n=1;n<=7;++n){char name[64];std::snprintf(name,sizeof(name),"image-view failure %u",n);
            check(name,[&]{failViewAt=n;},[]{});}
        for(unsigned n=1;n<=6;++n){char name[64];std::snprintf(name,sizeof(name),"new Texture bad_alloc %u",n);
            check(name,[&]{failTextureAt=n;},[]{});}
        check("non-allocation exception after unpublished GPU image",[]{failViewAt=3;throwView=true;},[]{});
        if(TargetResizeFailureCheck::Bindless(device)){
            std::vector<uint32_t> freeSlots;
            check("bindless free-list reserve bad_alloc after GPU view",
                [&]{freeSlots=TargetResizeFailureCheck::DetachFreeSlots(device);failHostAfterView=true;},
                [&]{TargetResizeFailureCheck::RestoreFreeSlots(device,freeSlots);});
        }else std::puts("SKIP bindless reserve injection: this device has no bindless descriptor set");
        ResetFaults();
    }
    assert(liveViews==0 && trackedHost==0);
    trackAllocations=false;rvk::vk::vkCreateImageView=realCreate;rvk::vk::vkDestroyImageView=realDestroy;
    std::printf("PASS %u actual target-resize allocation failures; no tracked host/GPU-view leaks after device destruction\n",cases);
}
