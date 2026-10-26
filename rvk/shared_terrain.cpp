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
}
struct Device::SharedTerrainMaps {
    maps::MipChain albedo[5];
    std::string error;
};
std::shared_ptr<Device::SharedTerrainMaps> Device::DecodeSharedTerrain()
{
    std::shared_ptr<SharedTerrainMaps> out;
    try {
        out=std::make_shared<SharedTerrainMaps>();
        const uint8_t* data[]={kSharedGrass,kSharedDirt,kSharedGravel,kSharedSand,kSharedRock};
        const size_t sizes[]={sizeof(kSharedGrass),sizeof(kSharedDirt),sizeof(kSharedGravel),sizeof(kSharedSand),sizeof(kSharedRock)};
        for (unsigned i=0;i<5;++i) {
            maps::Image image;
            if (!maps::LoadImageFromMemory(data[i],sizes[i],&image,&out->error)) return out;
            maps::MakeAlbedoChain(image,&out->albedo[i]);
        }
    } catch (const std::bad_alloc&) {return nullptr;}
    return out;
}
void Device::PollSharedTerrain()
{
    if (m_sharedTerrain<=0 || !m_skyPlayfield || m_sharedGround.images[0]) return;
    if (!m_sharedGround.started) {
        MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory);
        if (GlobalMemoryStatusEx(&memory) && memory.ullAvailVirtual < (384ull<<20)) return;
        m_sharedGround.started=true;
        try {m_sharedGround.pending=std::async(std::launch::async,DecodeSharedTerrain);}
        catch (const std::exception&) {Log("shared terrain: worker unavailable; original ground retained");}
        return;
    }
    auto& pending=m_sharedGround.pending;
    if (!pending.valid() || pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    std::shared_ptr<SharedTerrainMaps> maps;
    try {maps=pending.get();} catch (const std::exception&) {}
    if (!maps) {Log("shared terrain: insufficient memory; original ground retained");return;}
    if (!maps->error.empty()) {Log("shared terrain: %s; original ground retained",maps->error.c_str());return;}
    for (unsigned i=0;i<5;++i) {
        m_sharedGround.images[i]=maps::Upload(*this,maps->albedo[i]);
        if (!m_sharedGround.images[i]) {
            for (Texture*& image:m_sharedGround.images) {if(image)DestroyTexture(image);image=nullptr;}
            Log("shared terrain: upload unavailable; original ground retained");return;
        }
        const uint32_t mean=maps->albedo[i].levels.back()[0];
        for (unsigned c=0;c<3;++c) m_sharedGround.means[i][c]=float(mean>>(16-8*c)&255u)/255.0f;
    }
    m_frameLightsDirty=true;
    Log("shared terrain: five continuous master materials uploaded; original tile transitions retained");
}
void Device::FillSharedTerrainLights(detail::FrameLights* fl)
{
    std::memset(fl->sharedGround,0,sizeof(fl->sharedGround));
    std::memset(fl->sharedGroundConfig,0,sizeof(fl->sharedGroundConfig));
    if (m_sharedTerrain<=0 || !m_skyPlayfield) return;
    for (Texture* t:m_sharedGround.images) if (!t) return;
    for (unsigned i=0;i<5;++i) {
        const uint32_t slot=BindlessImage(m_sharedGround.images[i])+1;
        std::memcpy(&fl->sharedGround[i][0],&slot,sizeof(slot));
        for (unsigned c=0;c<3;++c) fl->sharedGround[i][c+1]=m_sharedGround.means[i][c];
    }
    fl->sharedGroundConfig[0]=std::clamp(m_sharedTerrain,0.0f,1.0f);
    fl->sharedGroundConfig[1]=0.25f;
    fl->sharedGroundConfig[2]=m_sharedTerrain>1.0f ? 1.0f : 0.0f;
}
} // namespace rvk
