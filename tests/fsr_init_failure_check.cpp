// CPU-only production FSR3 host lifecycle audit; no Vulkan/game process.
#include <bit>
#include <FidelityFX/host/ffx_fsr3.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <memory>

namespace {
unsigned contextSerial=0,serial=0,operation=0,failAt=0,ownershipErrors=0;
std::set<unsigned> contexts;
std::map<unsigned,unsigned> resources,pipelines;
bool Fail(){return ++operation==failAt;}
FfxVersionNumber Version(FfxInterface*){return FFX_SDK_MAKE_VERSION(1,1,4);}
FfxErrorCode Caps(FfxInterface*,FfxDeviceCapabilities* caps){
    if(Fail()) return FFX_ERROR_BACKEND_API_ERROR;
    *caps={};caps->maximumSupportedShaderModel=FFX_SHADER_MODEL_5_1;
    caps->waveLaneCountMin=caps->waveLaneCountMax=32;return FFX_OK;
}
FfxErrorCode CreateContext(FfxInterface*,FfxEffect,FfxEffectBindlessConfig*,FfxUInt32* id){
    if(Fail())return FFX_ERROR_OUT_OF_MEMORY;*id=contextSerial++;contexts.insert(*id);return FFX_OK;
}
FfxErrorCode DestroyContext(FfxInterface*,FfxUInt32 id){
    if (!contexts.erase(id)) ++ownershipErrors;
    for(auto i=resources.begin();i!=resources.end();)if(i->second==id)i=resources.erase(i);else ++i;
    return FFX_OK;
}
FfxErrorCode CreateResource(FfxInterface*,const FfxCreateResourceDescription* desc,FfxUInt32 id,FfxResourceInternal* out){
    if(Fail())return FFX_ERROR_OUT_OF_MEMORY;out->internalIndex=++serial;resources[out->internalIndex]=id;
    if(desc->initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED) resources[++serial]=id;
    return FFX_OK;
}
FfxErrorCode DestroyResource(FfxInterface*,FfxResourceInternal resource,FfxUInt32 id){
    auto found=resources.find(resource.internalIndex);
    if(found!=resources.end() && found->second!=id) ++ownershipErrors;
    else resources.erase(resource.internalIndex);return FFX_OK;
}
FfxErrorCode CreatePipeline(FfxInterface*,FfxEffect,FfxPass,uint32_t,const FfxPipelineDescription*,FfxUInt32 id,FfxPipelineState* out){
    if(Fail())return FFX_ERROR_OUT_OF_MEMORY;*out={};out->pipeline=++serial;pipelines[unsigned(out->pipeline)]=id;return FFX_OK;
}
FfxErrorCode DestroyPipeline(FfxInterface*,FfxPipelineState* state,FfxUInt32){
    pipelines.erase(unsigned(state->pipeline));*state={};return FFX_OK;
}
FfxInterface Backend(){
    FfxInterface result{};result.fpGetSDKVersion=Version;result.fpGetDeviceCapabilities=Caps;
    result.fpCreateBackendContext=CreateContext;result.fpDestroyBackendContext=DestroyContext;
    result.fpCreateResource=CreateResource;result.fpDestroyResource=DestroyResource;
    result.fpCreatePipeline=CreatePipeline;result.fpDestroyPipeline=DestroyPipeline;return result;
}
int Run(uint32_t flags,unsigned failure,unsigned* operations){
    contextSerial=0;serial=1;operation=ownershipErrors=0;failAt=failure;contexts.clear();resources.clear();pipelines.clear();
    resources[1]=0xffffffff; // another context: null-copy cleanup must not touch it
    auto context=std::make_unique<FfxFsr3Context>();std::memset(context.get(),0,sizeof(*context));
    FfxFsr3ContextDescription desc{};
    desc.flags=flags;desc.maxRenderSize={640,480};desc.maxUpscaleSize=desc.displaySize={1280,960};
    desc.backBufferFormat=FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
    desc.backendInterfaceSharedResources=desc.backendInterfaceUpscaling=desc.backendInterfaceFrameInterpolation=Backend();
    const auto result=ffxFsr3ContextCreate(context.get(),&desc);
    *operations=operation;
    if(result==FFX_OK)ffxFsr3ContextDestroy(context.get());
    const bool clean=contexts.empty()&&resources.size()==1&&resources.contains(1)&&pipelines.empty()&&!ownershipErrors;
    if(!clean)std::printf("flags=%u fail-at=%u result=%x leaked contexts=%zu resources=%zu pipelines=%zu\n",flags,failure,unsigned(result),contexts.size(),resources.size(),pipelines.size());
    if(failure&&result==FFX_OK){std::puts("injected failure was swallowed");return 2;}
    return clean?0:1;
}
}
int main(){
    unsigned failed=0,cases=0;
    for(uint32_t flags:{uint32_t(FFX_FSR3_ENABLE_UPSCALING_ONLY),uint32_t(FFX_FSR3_ENABLE_INTERPOLATION_ONLY),uint32_t(0),uint32_t(FFX_FSR3_ENABLE_INTERPOLATION_ONLY|FFX_FSR3_ENABLE_ASYNC_WORKLOAD_SUPPORT)}){
        unsigned operations=0;
        if(Run(flags,0,&operations)){std::puts("successful lifecycle leaked");return 3;}
        const unsigned count=operations;std::printf("flags=%u creation steps=%u\n",flags,count);
        for(unsigned point=1;point<=count;++point){++cases;unsigned ignored;if(Run(flags,point,&ignored))++failed;}
    }
    std::printf("FSR3 initialization failure cases: %u, leaked/swallowed: %u\n",cases,failed);
    return failed?1:0;
}
