// Five immutable terrain masters; original tiles supply palette and boundaries.
#include "internal.h"
#include "material_maps.h"
#include <cstring>
namespace rvk {
namespace {
#include "shared-grass.png.inc"
#include "shared-dirt.png.inc"
#include "shared-gravel.png.inc"
#include "shared-sand.png.inc"
#include "shared-rock.png.inc"
#include "shared-hd-grass.png.inc"
#include "shared-hd-dirt.png.inc"
#include "shared-hd-gravel.png.inc"
#include "shared-hd-sand.png.inc"
#include "shared-hd-rock.png.inc"
}
struct Device::SharedTerrainMaps {
    maps::MipChain albedo[5];
    std::string error;
};
std::shared_ptr<Device::SharedTerrainMaps> Device::DecodeSharedTerrain(bool hd)
{
    std::shared_ptr<SharedTerrainMaps> out;
    try {
        out=std::make_shared<SharedTerrainMaps>();
        const uint8_t* data[]={kSharedGrass,kSharedDirt,kSharedGravel,kSharedSand,kSharedRock};
        const size_t sizes[]={sizeof(kSharedGrass),sizeof(kSharedDirt),sizeof(kSharedGravel),sizeof(kSharedSand),sizeof(kSharedRock)};
        const uint8_t* hdData[]={kHdGrass,kHdDirt,kHdGravel,kHdSand,kHdRock};
        const size_t hdSizes[]={sizeof(kHdGrass),sizeof(kHdDirt),sizeof(kHdGravel),sizeof(kHdSand),sizeof(kHdRock)};
        for (unsigned i=0;i<5;++i) {
            maps::Image image;
            if (!maps::LoadImageFromMemory(hd ? hdData[i]:data[i],hd ? hdSizes[i]:sizes[i],&image,&out->error)) return out;
            maps::MakeAlbedoChain(image,&out->albedo[i]);
        }
    } catch (const std::bad_alloc&) {return nullptr;}
    return out;
}
void Device::PollSharedTerrain()
{
    if (m_native.on || m_sharedTerrain<=0 || !m_skyPlayfield) return;
    auto poll=[this](SharedTerrain& state,bool hd) {
    if(state.uploaded==5)return;
    if (!state.started) {
        MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory);
        if (GlobalMemoryStatusEx(&memory) && memory.ullAvailVirtual < (384ull<<20)) return;
        state.started=true;
        try {state.pending=std::async(std::launch::async,DecodeSharedTerrain,hd);}
        catch (const std::exception&) {Log("shared terrain: worker unavailable; original ground retained");}
        return;
    }
    auto& pending=state.pending;
    if(!state.decoded) {
        if (!pending.valid() || pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        try {state.decoded=pending.get();} catch (const std::exception&) {}
    }
    auto maps=state.decoded;
    if (!maps) {Log("shared terrain: insufficient memory; original ground retained");return;}
    if (!maps->error.empty()) {Log("shared terrain: %s; original ground retained",maps->error.c_str());state.decoded.reset();return;}
    const unsigned stop=hd ? std::min(state.uploaded+1u,5u):5u;
    for (unsigned i=state.uploaded;i<stop;++i) {
        state.images[i]=maps::Upload(*this,maps->albedo[i]);
        if (!state.images[i]) {
            for (Texture*& image:state.images) {if(image)DestroyTexture(image);image=nullptr;}
            state.decoded.reset();state.uploaded=0;
            Log("shared terrain: upload unavailable; original ground retained");return;
        }
        const uint32_t mean=maps->albedo[i].levels.back()[0];
        for (unsigned c=0;c<3;++c) state.means[i][c]=float(mean>>(16-8*c)&255u)/255.0f;
        ++state.uploaded;
    }
    if(state.uploaded<5)return;
    state.decoded.reset();
    m_frameLightsDirty=true;
    Log("shared terrain: five %s master materials uploaded; original tile transitions retained",hd ? "full-HD albedo":"detail");
    };
    poll(m_sharedGround,false);
    if(m_hdGroundMode>.5f && SharedGroundPlus())poll(m_hdGroundState,true);
}
void Device::FillSharedTerrainLights(detail::FrameLights* fl)
{
    std::memset(fl->sharedGround,0,sizeof(fl->sharedGround));
    std::memset(fl->sharedGroundConfig,0,sizeof(fl->sharedGroundConfig));
    std::memset(fl->hdGround,0,sizeof(fl->hdGround));
    std::memset(fl->hdGroundConfig,0,sizeof(fl->hdGroundConfig));
    // Independent of master-texture upload readiness: no temporary native
    // sun/AO darkening while the five shared maps are still decoding.
    fl->sharedGroundConfig[3]=SharedGroundPlus() ? 1.0f : 0.0f;
    if (m_native.on || m_sharedTerrain<=0 || !m_skyPlayfield) return;
    for (Texture* t:m_sharedGround.images) if (!t) return;
    for (unsigned i=0;i<5;++i) {
        const uint32_t slot=BindlessImage(m_sharedGround.images[i])+1;
        std::memcpy(&fl->sharedGround[i][0],&slot,sizeof(slot));
        for (unsigned c=0;c<3;++c) fl->sharedGround[i][c+1]=m_sharedGround.means[i][c];
    }
    fl->sharedGroundConfig[0]=std::clamp(m_sharedTerrain,0.0f,1.0f);
    fl->sharedGroundConfig[1]=0.25f;
    fl->sharedGroundConfig[2]=0.0f;
    if(m_hdGroundMode>.5f && SharedGroundPlus()) {
        for(Texture* t:m_hdGroundState.images)if(!t)return;
        for(unsigned i=0;i<5;++i) {
            const uint32_t slot=BindlessImage(m_hdGroundState.images[i])+1;
            std::memcpy(&fl->hdGround[i][0],&slot,sizeof(slot));
            for(unsigned c=0;c<3;++c)fl->hdGround[i][c+1]=m_hdGroundState.means[i][c];
        }
        fl->hdGroundConfig[0]=1.f;fl->hdGroundConfig[1]=.25f;
    }
}
} // namespace rvk
