// AO-VK Living Rubi-Ka: optional renderer-only actors, on the renderer thread.
#include "internal.h"
#include "living.h"
#include "living_release.h"
#include "material_maps.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace rvk {
namespace {
constexpr uint64_t SmokeKey=0x20004c5200000000ull;
double Now() {return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string InstalledClient() {
    HMODULE mod=nullptr;wchar_t path[32768]={};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&InstalledClient),&mod))return {};
    const DWORD size=GetModuleFileNameW(mod,path,32768);if(!size||size>=32768)return {};
    try{return std::filesystem::path(path).parent_path().string();}catch(const std::exception&){return {};}
}
std::string PackPath(uint32_t playfield) {
    if(const char* p=std::getenv("RANDYVK_LIVING_SCENE");p&&*p)return p;
    HMODULE mod=nullptr;char path[MAX_PATH]={};
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCSTR>(&PackPath),&mod);
    GetModuleFileNameA(mod,path,MAX_PATH);
    std::filesystem::path root=std::filesystem::path(path).parent_path()/"randy-vk/living";
    if(const char* p=std::getenv("RANDYVK_LIVING_DIR");p&&*p)root=p;
    auto pack=root/(std::to_string(playfield)+".lrs");
    std::error_code error;
    return (std::filesystem::exists(pack,error)?pack:root/"scene.lrs").string();
}
float Smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
bool ReviewAllowed(const std::string& path){const char* p=std::getenv("RANDYVK_LIVING_REVIEW");return (p&&std::strcmp(p,"1")==0)||living::LocalReviewEnabled(path);}
float Angle(float a,float b){return std::atan2(std::sin(b-a),std::cos(b-a));}
}
void Device::SetLivingContext(uint32_t pf,bool outdoor,uint64_t generation){
    if(!m_livingContextKnown||pf!=m_livingContextPlayfield||outdoor!=m_livingOutdoor||generation!=m_livingContextGeneration){
        DestroyLiving();Log("Living Rubi-Ka context: playfield %u, outdoor %u, generation %llu",pf,unsigned(outdoor),(unsigned long long)generation);
    }
    m_livingContextKnown=true;m_livingContextPlayfield=pf;m_livingOutdoor=outdoor;m_livingContextGeneration=generation;
}
void Device::PrepareLivingFrame(){
    if(!m_livingHadLights)return;
    const float opts[]={m_livePeople,m_liveDrones,m_liveAir,m_liveWild,m_liveMachine,m_liveAtmos};
    std::erase_if(m_lightsPrev,[&](const CapturedLight& l){return l.cosmeticGroup<6&&(opts[l.cosmeticGroup]<1||m_indoors||!m_living);});
    m_livingHadLights=std::any_of(m_lightsPrev.begin(),m_lightsPrev.end(),[](const CapturedLight& l){return l.cosmeticGroup<6;});
}
bool Device::SuppressLivingCached(uint32_t vc,uint32_t ic,const float* lo,const float* hi) const {
    if(m_liveMachine<1||!m_living)return false;
    bool active=false;for(const auto& a:m_living->actors)active|=a.replacementMask&&a.parent<0&&a.lastFrame==m_frameNumber;if(!active)return false;
    for(size_t k=0;k<m_living->replacements.size();++k){const auto& r=m_living->replacements[k];bool rendered=false;for(const auto& a:m_living->actors)rendered|=a.parent<0&&(a.replacementMask&(1u<<k))&&a.lastFrame==m_frameNumber;
    if(rendered&&r.witnessed&&vc==r.vertices&&ic==r.indices){bool same=true;for(int j=0;j<3;++j)same&=std::fabs(lo[j]-r.boundsMin[j])<.005f&&std::fabs(hi[j]-r.boundsMax[j])<.005f;if(same)return true;}}return false;
}
bool Device::SuppressLivingOriginal(uint32_t fvf,const void* vertices,uint32_t vc,const uint16_t* indices,uint32_t ic){
    if(m_liveMachine<1||!m_living||m_livingDrawing||m_drawSkin||!vertices||!indices||m_target!=m_main||m_indoors||m_pickActive)return false;
    if((fvf&d3d::FVF_POSITION_MASK)!=d3d::FVF_XYZ)return false;
    uint64_t candidateHash=0;bool hashed=false;
    for(auto& r:m_living->replacements){
        if(r.vertices!=vc||r.indices!=ic)continue;
        const float dx=m_world.m[3][0]-r.origin.x,dy=m_world.m[3][1]-r.origin.y,dz=m_world.m[3][2]-r.origin.z;
        if(dx*dx+dy*dy+dz*dz>400)continue;
        const auto layout=detail::DecodeFvf(fvf);
        if(!hashed){candidateHash=living::GeometryHash(vertices,vc,layout.stride,indices,ic,m_world.m);hashed=true;
            auto& seen=m_living->diagnosticCandidates;
            if(seen.size()<16&&std::find(seen.begin(),seen.end(),candidateHash)==seen.end()){
                seen.push_back(candidateHash);Log("Living machinery candidate: v %u i %u at %.2f,%.2f,%.2f hash %016llx",vc,ic,m_world.m[3][0],m_world.m[3][1],m_world.m[3][2],(unsigned long long)candidateHash);
            }
        }
        if(candidateHash!=r.hash)continue;
        float lo[3]={1e20f,1e20f,1e20f},hi[3]={-1e20f,-1e20f,-1e20f};
        for(uint32_t i=0;i<vc;++i){float p[3];std::memcpy(p,static_cast<const uint8_t*>(vertices)+size_t(i)*layout.stride,12);for(int j=0;j<3;++j){lo[j]=std::min(lo[j],p[j]);hi[j]=std::max(hi[j],p[j]);}}
        detail::TransformBounds(lo,hi,m_world,r.boundsMin,r.boundsMax);
        r.lastSeen=m_frameNumber;const uint32_t part=1u<<uint32_t(&r-m_living->replacements.data());bool stable=false;
        for(const auto& a:m_living->actors)stable|=a.parent<0&&(a.replacementMask&part)&&a.replacementStable>=12;
        if(!stable)return false;
        living::Point eye{};float* e=&eye.x;for(int j=0;j<3;++j)e[j]=-(m_view.m[3][0]*m_view.m[j][0]+m_view.m[3][1]*m_view.m[j][1]+m_view.m[3][2]*m_view.m[j][2]);
        d3d::Matrix vp{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)vp.m[i][j]+=m_view.m[i][k]*m_proj.m[k][j];bool visible=false;
        for(const auto& a:m_living->actors)if(a.replacementMask&&a.parent<0){auto point=a.route.points[0];float d=std::hypot(std::hypot(point.x-eye.x,point.y-eye.y),point.z-eye.z);visible|=d>3&&d<a.range*.8f&&living::InFrustum(vp.m,point,m_living->meshes[a.mesh].radius*a.scale);}
        if(!visible)return false;
        // Keep the six small original draws until the replacement actually submits. If a LOD part
        // disappears or an upload fails mid-frame, replay the matching originals instead of leaving holes.
        auto geometry=std::make_shared<std::vector<uint8_t>>(size_t(vc)*layout.stride);std::memcpy(geometry->data(),vertices,geometry->size());
        auto triangles=std::make_shared<std::vector<uint16_t>>(indices,indices+ic);
        const auto rs=m_rs;const auto tss=m_tss;const auto tex=m_textures;const auto tm=m_texMatrix;
        const auto world=m_world,view=m_view,proj=m_proj;const auto mat=m_material;const auto viewport=m_viewport;const auto lights=m_lights;
        const auto kind=m_drawVisualKind,owner=m_drawOwner,visual=m_drawVisual;const char* name=m_drawVisualName;
        const uint32_t bit=1u<<uint32_t(&r-m_living->replacements.data());
        m_livingOriginals.emplace_back(bit,[=,this]{
            m_rs=rs;m_tss=tss;m_textures=tex;m_texMatrix=tm;m_world=world;m_view=view;m_proj=proj;m_material=mat;m_viewport=viewport;m_lights=lights;
            m_drawVisualKind=kind;m_drawOwner=owner;m_drawVisual=visual;m_drawVisualName=name;
            m_drawSkin=nullptr;m_drawGpu=nullptr;m_drawSkinBase=nullptr;m_pickSkinned=nullptr;m_drawStaticBuffer=VK_NULL_HANDLE;m_drawStaticIndexBuffer=VK_NULL_HANDLE;
            ++m_cameraGen;++m_lightsGen;m_constantsDirty=true;
            DrawIndexedPrimitive(d3d::TriangleList,fvf,geometry->data(),vc,triangles->data(),ic);
        });
        r.witnessed=true;r.suppressedFrame=m_frameNumber;++m_livingReplaced;return true;
    }return false;
}
void Device::DestroyLiving() {
    m_livingDraws.fill(0);m_livingOriginals.clear();
    std::erase_if(m_lightsPrev,[](const CapturedLight& l){return l.cosmeticGroup<6;});
    std::erase_if(m_lightsCur,[](const CapturedLight& l){return l.cosmeticGroup<6;});m_livingHadLights=false;
    for(auto& v:m_smokeVolumes)if((v.key&0xffffffffffff0000ull)==SmokeKey||(v.parent&0xffffffffffff0000ull)==SmokeKey){v.key=0;v.parent=0;}
    if(m_living) {
        // Return only this pack's arena slots and buffers through normal GPU retirement.
        auto release=[&](const std::shared_ptr<const std::vector<uint8_t>>& data) {
            if(!data)return;auto it=m_staticGeometry.find(data.get());if(it==m_staticGeometry.end())return;
            if(it->second.chunk){ArenaFree(it->second.chunk,it->second.offset,it->second.size);m_staticBytes-=it->second.size;}
            m_staticGeometry.erase(it);
        };
        for(auto& a:m_living->actors)if(a.job) {
            m_skinOutputs.erase(a.job.get());m_skinBonesAt.erase(a.job->bones.get());m_skinBonesAt.erase(a.job->prevBones.get());
        }
        for(size_t i=0;i<m_living->meshes.size();++i) {
            auto& m=m_living->meshes[i];bool first=true;
            for(size_t j=0;j<i;++j)if(m_living->meshes[j].texture==m.texture)first=false;
            if(m.texture&&first)DestroyTexture(m.texture);release(m.vertices);for(auto& index:m.indexBytes)release(index);
            if(m.skin) {auto it=m_skinMeshes.find(m.skin.get());if(it!=m_skinMeshes.end()){
                if(it->second.buffer)m_deadBuffers.push_back({DeathTag(),{it->second.buffer,it->second.allocation}});m_skinMeshes.erase(it);
            }}
        }
    }
    m_living.reset();m_livingAttempt=false;
    for(auto& t:m_livingEnabled)t=-1;
}
void Device::DrawLiving() {
    if constexpr (!living::ReleaseContentAvailable) {
        if (m_living || m_livingAttempt) DestroyLiving();
        return;
    }

    const float opts[6]={m_livePeople,m_liveDrones,m_liveAir,m_liveWild,m_liveMachine,m_liveAtmos};
    bool any=false;for(float x:opts)any|=x>=1;
    if(!any){if(m_living||m_livingAttempt)DestroyLiving();return;}
    if(m_livingFrame==m_frameNumber||!m_inFrame||m_deviceLost||m_livingDrawing||!m_sceneSaw3D||(!m_aoProjValid&&!m_livingCameraValid)||m_target!=m_main)return;
    m_livingFrame=m_frameNumber; m_livingDraws.fill(0);
    if(m_indoors||!m_skyPlayfield||(m_livingContextKnown&&(!m_livingOutdoor||m_livingContextPlayfield!=m_skyPlayfield))){if(m_living)DestroyLiving();return;}
    if(m_skyPlayfield!=living::FixturePlayfield&&!m_livingContextKnown)return;
    if(!m_living&&!m_livingAttempt) {
        m_livingAttempt=true;auto s=std::make_unique<living::Scene>();std::string error;
        living::LocalAssets local;
        const bool fixture=m_skyPlayfield==living::FixturePlayfield;
        if(!fixture&&!living::ResolveInstalled(InstalledClient(),m_skyPlayfield,local,error)){
            Log("Living Rubi-Ka: local assets unavailable (%s); %s",error.c_str(),living::UnavailableReason);return;
        }
        const auto path=fixture?PackPath(m_skyPlayfield):local.scene;const bool review=ReviewAllowed(path);
        Log("Living Rubi-Ka: pack '%s'; unverified-route review %s (not live certification)",path.c_str(),review?"enabled":"disabled");
        if(!living::Load(path,*s,error,m_skyPlayfield,!review,fixture?nullptr:&local.verified)){Log("Living Rubi-Ka: scene unavailable (%s); no actors submitted",error.c_str());return;}
        for(const auto& diagnostic:s->diagnostics)Log("Living Rubi-Ka: %s",diagnostic.c_str());
        if(!s->fixture&&!m_livingContextKnown){Log("Living Rubi-Ka: no current playfield/outdoor context; population disabled");return;}
        if(s->playfield!=m_skyPlayfield){Log("Living Rubi-Ka: pack is for %u, current playfield %u; placement rejected",s->playfield,m_skyPlayfield);return;}
        if(!s->fixture&&!s->liveVerified&&!review){Log("Living Rubi-Ka: static-only route pack DISABLED; live verification is pending. Explicit isolated review mode is required.");return;}
        m_living=std::move(s);m_livingStart=Now();
        Log("Living Rubi-Ka: %zu actors, %zu shared assets, %.2f MiB; %s",m_living->actors.size(),m_living->meshes.size(),double(m_living->bytes)/1048576.,m_living->fixture?"CONTROLLED FIXTURE (not gameplay)":"STATIC-REVIEW PLAYFIELD PACK (not live-verified)");
    }
    if(!m_living)return;
    m_living->replacementStable=0;
    for(auto& a:m_living->actors)if(a.parent<0&&a.replacementMask){bool complete=true;for(size_t k=0;k<m_living->replacements.size();++k)if(a.replacementMask&(1u<<k))complete&=m_living->replacements[k].lastSeen==m_frameNumber;
        a.replacementStable=complete?std::min(12u,a.replacementStable+1):0;m_living->replacementStable=std::max(m_living->replacementStable,a.replacementStable);
    }
    const double clock=Now();
    // Suspended/background rendering never fast-forwards the visible actors on return.
    // Deterministic fixture time is explicit and has no influence on live playfields.
    double time=m_living->timeline.Advance(clock);
    if(m_living->fixture)if(const char* p=std::getenv("RANDYVK_LIVING_TIME")){double v=std::strtod(p,nullptr);if(std::isfinite(v)&&v>=0&&v<1e6)time=v;}
    for(size_t i=0;i<6;++i){if(opts[i]<1)m_livingEnabled[i]=-1;else if(m_livingEnabled[i]<0)m_livingEnabled[i]=clock;}
    const double cpu=ProfileCpu();FlushGroup();ProfileMark("living begin");
    // Save all D3D draw inputs, including the gameplay visual identity and pick state.
    const auto lights=m_lights;
    const auto rs=m_rs;const auto tss=m_tss;const auto tex=m_textures;const auto tm=m_texMatrix;
    const auto world=m_world,view=m_view,proj=m_proj;const auto material=m_material;const auto viewport=m_viewport;
    const auto kind=m_drawVisualKind,owner=m_drawOwner,visual=m_drawVisual;const char* name=m_drawVisualName;
    const bool pick=m_pickActive;m_pickActive=false;m_livingDrawing=true;
    auto restore=[&] {
        m_lights=lights;++m_lightsGen;
        m_rs=rs;m_tss=tss;m_textures=tex;m_texMatrix=tm;m_world=world;m_view=view;m_proj=proj;m_material=material;m_viewport=viewport;
        m_drawVisualKind=kind;m_drawOwner=owner;m_drawVisual=visual;m_drawVisualName=name;m_pickActive=pick;
        m_livingDrawing=false;m_livingPreviousWorld=nullptr;++m_cameraGen;m_constantsDirty=true;
    };
    struct Restore { decltype(restore)& action; ~Restore(){action();} } guard{restore};
    m_view=m_livingCameraValid?m_livingView:m_aoView;m_proj=m_livingCameraValid?m_livingProj:m_aoProj;
    if(m_livingCameraValid)m_viewport=m_livingViewport;++m_cameraGen;
    d3d::Matrix vp{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)vp.m[i][j]+=m_view.m[i][k]*m_proj.m[k][j];
    living::Point eye{};float* e=&eye.x;for(int i=0;i<3;++i)e[i]=-(m_view.m[3][0]*m_view.m[i][0]+m_view.m[3][1]*m_view.m[i][1]+m_view.m[3][2]*m_view.m[i][2]);
    SetDrawVisual(uint32_t(VisualKind::Other),"LivingRubiKa",0,0);
    using namespace d3d;
    SetRenderState(RS_ZENABLE,1);SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);SetRenderState(RS_CULLMODE,CULL_NONE);
    if(m_livingLightingValid){m_lights=m_livingWorldLights;++m_lightsGen;SetRenderState(RS_AMBIENT,m_livingAmbient);}
    SetRenderState(RS_LIGHTING,1);SetRenderState(RS_COLORVERTEX,0);SetRenderState(RS_NORMALIZENORMALS,1);
    SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_MATERIAL);SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_MATERIAL);
    SetRenderState(RS_EMISSIVEMATERIALSOURCE,MCS_MATERIAL);SetRenderState(RS_SPECULARMATERIALSOURCE,MCS_MATERIAL);
    SetRenderState(RS_ZBIAS,0);SetRenderState(RS_ALPHATESTENABLE,0);
    if(m_livingLightingValid)for(size_t i=0;i<LivingFogStates.size();++i)SetRenderState(LivingFogStates[i],m_livingWorldFog[i]);
    SetTexture(1,nullptr);SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
    SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
    SetTextureStageState(0,TSS_ALPHAOP,TOP_MODULATE);SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);SetTextureStageState(0,TSS_ALPHAARG2,TA_DIFFUSE);
    SetTextureStageState(0,TSS_TEXCOORDINDEX,0);SetTextureStageState(0,TSS_TEXTURETRANSFORMFLAGS,TTFF_DISABLE);
    SetTextureStageState(0,TSS_ADDRESS,TADDRESS_CLAMP);SetTextureStageState(0,TSS_MINFILTER,TFN_LINEAR);SetTextureStageState(0,TSS_MAGFILTER,TFG_LINEAR);SetTextureStageState(0,TSS_MIPFILTER,TFP_LINEAR);
    size_t drawn=0;uint32_t groupDraws[6]={};uint32_t accepted[6]={};uint32_t largeUploads=0;
    const uint32_t caps[6][4]={{0,6,12,20},{0,2,4,6},{0,1,2,2},{0,3,6,6},{0,8,8,8},{0,2,2,2}};
    for(size_t i=0;i<m_living->actors.size();++i) {
        auto& a=m_living->actors[i];const uint64_t key=SmokeKey|uint64_t(i+1);
        if(!m_living->meshes[a.mesh].available||(a.parent>=0&&!m_living->meshes[m_living->actors[a.parent].mesh].available))continue;
        const uint32_t level=uint32_t(std::clamp(opts[a.group],0.f,3.f));
        if(level<a.tier||(a.parent<0&&accepted[a.group]>=caps[a.group][level])) {
            a.activationTime=-1;a.headingTime=-1;a.previous.reset();a.job.reset();for(auto& v:m_smokeVolumes)if(v.key==key||v.parent==key){v.key=0;v.parent=0;}continue;
        }
        if(a.replacementMask){bool ready=true;for(size_t k=0;k<m_living->replacements.size();++k)if(a.replacementMask&(1u<<k))ready&=m_living->replacements[k].suppressedFrame==m_frameNumber;if(!ready){a.activationTime=-1;continue;}if(a.activationTime<0)a.activationTime=time;}
        const bool stopping=a.motion==3||a.motion==5;
        const living::Movement movement=stopping?living::Locomotion(time,a):living::Movement{};
        const float slot=float(std::fmod(time+a.phase,double(a.walkSeconds+a.pauseSeconds)));
        const double distance=stopping?movement.distance:a.motion==6?slot*a.speed:time*a.speed+a.phase;
        const float travel=a.route.length>0?float(std::fmod(distance,double(a.route.length))):0.f;
        living::Point tangent,p=a.route.At(travel,&tangent);
        if(a.parent>=0){const auto& parent=m_living->actors[a.parent];if(parent.lastFrame!=m_frameNumber)continue;const auto& w=parent.world;const auto local=p;
            p={local.x*w[0][0]+local.y*w[1][0]+local.z*w[2][0]+w[3][0],local.x*w[0][1]+local.y*w[1][1]+local.z*w[2][1]+w[3][1],local.x*w[0][2]+local.y*w[1][2]+local.z*w[2][2]+w[3][2]};tangent={w[2][0],w[2][1],w[2][2]};}
        else if(a.motion==3&&!a.surface&&!m_living->ground.empty()){float y;if(!living::GroundHeight(*m_living,p.x,p.z,y))continue;p.y=y+.008f;}
        a.supportHeight=p.y;
        p.y+=a.hover*(.7f*std::sin(float(time*1.3+a.phase))+.3f*std::sin(float(time*2.7+a.phase*.37)));
        const float dist=std::sqrt((p.x-eye.x)*(p.x-eye.x)+(p.y-eye.y)*(p.y-eye.y)+(p.z-eye.z)*(p.z-eye.z));
        float alpha=living::Fade(dist,a.range)*Smooth(float(clock-m_livingEnabled[a.group])/1.5f);
        if(m_living->fixture&&std::getenv("RANDYVK_LIVING_TIME"))alpha=living::Fade(dist,a.range);
        if(a.replacementMask)alpha=living::Fade(dist,a.range);
        if(a.motion==6)alpha*=Smooth(slot/3)*Smooth((a.walkSeconds-slot)/3);
        else if(a.group==2){const float legacySlot=float(std::fmod(time+a.phase,90.0));alpha*=Smooth(legacySlot/2)*Smooth((30-legacySlot)/2);}
        if(a.parent>=0&&a.group==4)alpha*=.86f+.14f*std::sin(float(time*2.3+a.phase));
        auto& m=m_living->meshes[a.mesh];
        if(alpha<.005f||!living::InFrustum(vp.m,p,m.radius*a.scale+a.hover)){a.previous.reset();continue;}
        if(a.parent<0)++accepted[a.group];
        ++m_livingDraws[a.group];
        if(a.lightRange>0&&m_lightsCur.size()<512&&m_livingLightCount<4){
            CapturedLight light{};light.light.type=LIGHT_POINT;light.light.position={p.x+tangent.x/std::max(.001f,std::hypot(tangent.x,tangent.z))*.15f,p.y+.12f,p.z+tangent.z/std::max(.001f,std::hypot(tangent.x,tangent.z))*.15f};light.light.range=a.lightRange;
            light.light.diffuse={float((a.lightColour>>16)&255)/255*.55f*alpha,float((a.lightColour>>8)&255)/255*.55f*alpha,float(a.lightColour&255)/255*.55f*alpha,1};
            light.light.attenuation0=1;light.light.attenuation2=1.4f;light.cosHalfTheta=light.cosHalfPhi=1;light.cosmeticGroup=a.group;
            m_lightsCur.push_back(light);m_livingHadLights=true;++m_livingLightCount;
        }
        if(a.motion==2) {
            // Shared smoke implementation: no duplicate particle/weather system or arbitrary building detection.
            SmokeSource source;source.pos[0]=p.x;source.pos[1]=p.y;source.pos[2]=p.z;
            source.radius=.22f*a.scale;source.rate=.38f*alpha;source.heat=.45f;source.speed=.55f;source.life=3.2f;source.size=4.5f;source.colour=0xb0b2ac;
            SmokeSourceFrame(key,source);continue;
        }
        auto& materialMesh=m.materialOwner==~0u?m:m_living->meshes[m.materialOwner];
        if(!materialMesh.texture) {
            if(materialMesh.dimension>=256){if(largeUploads)continue;++largeUploads;}
            maps::Decoded decoded;
            auto image=[&](int j){maps::Image img;img.w=img.h=int(materialMesh.mapDimensions[j]);img.channels=4;img.rgba.resize(materialMesh.pixels[j].size()*4);
                for(size_t i=0;i<materialMesh.pixels[j].size();++i){const auto p=materialMesh.pixels[j][i];img.rgba[i*4]=uint8_t(p>>16);img.rgba[i*4+1]=uint8_t(p>>8);img.rgba[i*4+2]=uint8_t(p);img.rgba[i*4+3]=uint8_t(p>>24);}return img;};
            maps::MakeAlbedoChain(image(0),&decoded.albedo);maps::NormalSpread spread;
            maps::MakeNormalChain(image(1),&decoded.normal,&spread);
            std::vector<float> orm(materialMesh.pixels[2].size()*3);
            for(size_t i=0;i<materialMesh.pixels[2].size();++i){const auto p=materialMesh.pixels[2][i];orm[i*3]=float((p>>16)&255)/255;orm[i*3+1]=float((p>>8)&255)/255;orm[i*3+2]=float(p&255)/255;}
            maps::MakeOrmChain(std::move(orm),int(materialMesh.mapDimensions[2]),int(materialMesh.mapDimensions[2]),spread,&decoded.orm);
            maps::MakeAlbedoChain(image(3),&decoded.emissive);
            // Albedo remains the base texture: normal/ORM/emission use the existing PBR material path.
            materialMesh.texture=maps::Upload(*this,decoded.albedo);decoded.albedo={};
            if(!materialMesh.texture)continue;
            maps::Attach(*this,materialMesh.texture,decoded);
            for(auto& pixels:materialMesh.pixels){pixels.clear();pixels.shrink_to_fit();}
        }
        m.texture=materialMesh.texture;
        float yaw=a.motion==1?float(time*a.speed+a.phase):std::atan2(tangent.x,tangent.z);
        if(a.route.length>0){auto before=a.route.At(travel-.45f),after=a.route.At(travel+.45f);yaw=std::atan2(after.x-before.x,after.z-before.z);}
        yaw+=a.heading;
        if(stopping)yaw+=std::clamp(Angle(yaw,a.idleYaw),-.4f,.4f)*(1-movement.walkWeight)+(.07f*std::sin(float(time*.31+a.phase)))*(1-movement.walkWeight);
        if(a.parent<0&&a.motion!=1){
            const bool continuous=a.headingTime>=0&&time>=a.headingTime&&time-a.headingTime<.25&&a.lastFrame+1==m_frameNumber;
            if(a.headingTime>=0&&!continuous){a.previous.reset();a.job.reset();a.lastFrame=~uint64_t(0);}
            if(continuous)yaw=living::TurnTowards(a.facingYaw,yaw,float(time-a.headingTime),a.group==1?.85f:a.group==2?.65f:1.6f);
            a.facingYaw=yaw;a.headingTime=time;
        }
        const float cs=std::cos(yaw)*a.scale,sn=std::sin(yaw)*a.scale;
        Matrix w{};w.m[0][0]=cs;w.m[0][2]=-sn;w.m[1][1]=a.scale;w.m[2][0]=sn;w.m[2][2]=cs;w.m[3][0]=p.x;w.m[3][1]=p.y;w.m[3][2]=p.z;w.m[3][3]=1;
        if(a.parent>=0){const auto& parent=m_living->actors[a.parent].world;const float localC=std::cos(a.heading)*a.scale,localS=std::sin(a.heading)*a.scale;
            for(int c=0;c<3;++c){w.m[0][c]=localC*parent[0][c]-localS*parent[2][c];w.m[1][c]=a.scale*parent[1][c];w.m[2][c]=localS*parent[0][c]+localC*parent[2][c];}}
        if((a.motion==4||a.motion==6)&&a.route.length>0){living::Point aheadT;a.route.At(travel+1,&aheadT);float turn=Angle(yaw-a.heading,std::atan2(aheadT.x,aheadT.z));float bank=std::clamp(-std::atan(a.speed*a.speed*turn/9.81f),-.28f,.28f);for(int j=0;j<3;++j){float x=w.m[0][j],y=w.m[1][j];w.m[0][j]=x*std::cos(bank)+y*std::sin(bank);w.m[1][j]=-x*std::sin(bank)+y*std::cos(bank);}}
        SetTransform(World,w);SetTexture(0,m.texture);
        m_livingPreviousWorld=a.lastFrame+1==m_frameNumber?a.world:nullptr;
        Material mat{};mat.diffuse={1,1,1,alpha};mat.ambient={1,1,1,1};mat.specular={.12f,.12f,.12f,1};mat.power=24;SetMaterial(mat);
        SetRenderState(RS_ALPHABLENDENABLE,alpha<.995f);SetRenderState(RS_ZWRITEENABLE,alpha>=.995f);
        SetRenderState(RS_ALPHATESTENABLE,m.cutout);SetRenderState(RS_ALPHAFUNC,CMP_GREATER);SetRenderState(RS_ALPHAREF,uint32_t(96*alpha));SetRenderState(RS_COLORKEYENABLE,0);
        SetRenderState(RS_SRCBLEND,BLEND_SRCALPHA);SetRenderState(RS_DESTBLEND,BLEND_INVSRCALPHA);
        const int lod=dist>80?2:dist>30?1:0;
        if(m.skin) {
          if(!a.job||!a.previous||dist<60||m_frameNumber%2==i%2) {
            const double animationTime=a.replacementMask&&!m_living->fixture?std::max(0.,time-a.activationTime):time;
            float frame=float(std::fmod(animationTime*m.fps+a.phase,double(m.frameCount)));
            uint32_t count=m.frameCount;
            if(m.walkFrames){count=m.walkFrames;frame=a.motion==3?living::GaitFrame(m,movement.distance/a.scale):float(std::fmod(time*m.fps+a.phase,double(count)));}
            const uint32_t i0=uint32_t(frame)%count,i1=(i0+1)%count;const float u=frame-std::floor(frame);
            const uint32_t idleCount=m.frameCount-m.walkFrames;
            const float idleFrame=m.walkFrames?float(std::fmod((time+a.phase)*idleCount/m.idleSeconds,double(idleCount))):0.f;
            const uint32_t j0=m.walkFrames+uint32_t(idleFrame),j1=m.walkFrames+(uint32_t(idleFrame)+1)%std::max(idleCount,1u);const float idleU=idleFrame-std::floor(idleFrame);
            const float walking=a.motion==3?movement.walkWeight:1.f;
            auto& bones=m.poseScratch;
            for(uint32_t b=0;b<m.bones;++b)for(int j=0;j<12;++j){
                float value=m.frames[i0*m.bones+b].m[j]*(1-u)+m.frames[i1*m.bones+b].m[j]*u;
                if(m.walkFrames&&walking<1){float idle=m.frames[j0*m.bones+b].m[j]*(1-idleU)+m.frames[j1*m.bones+b].m[j]*idleU;value=value*walking+idle*(1-walking);}
                bones[b].m[j]=value;
            }
            for(auto& bone:bones) {
                float* x=bone.m;float* y=bone.m+3;float* z=bone.m+6;
                auto unit=[](float* p){float len=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);if(len>1e-6f)for(int k=0;k<3;++k)p[k]/=len;};
                unit(x);float dot=x[0]*y[0]+x[1]*y[1]+x[2]*y[2];for(int k=0;k<3;++k)y[k]-=dot*x[k];unit(y);
                z[0]=x[1]*y[2]-x[2]*y[1];z[1]=x[2]*y[0]-x[0]*y[2];z[2]=x[0]*y[1]-x[1]*y[0];
            }
            auto palette=std::make_shared<skin::Palette>();palette->Set(bones.data(),m.bones);
            a.job=std::make_shared<skin::Job>();a.job->source=m.skin;a.job->bones=palette;a.job->prevBones=a.previous;a.job->ComputeBounds();
            a.previous=palette;
          }
            if(a.motion==3&&!m.contacts.empty()){w.m[3][1]+=living::ContactOffset(*m_living,m,*a.job->bones,w.m,a.surface);SetTransform(World,w);}
            DrawSkinned(TriangleList,skin::kVertexFvf,*a.job,0,m.vertexCount,m.skin->indices.data(),uint32_t(m.indices[lod]->size()));
        }else DrawSharedIndexed(TriangleList,skin::kVertexFvf,m.vertices,0,m.vertexCount,m.indexBytes[lod],uint32_t(m.indices[lod]->size()));
        std::memcpy(a.world,w.m,sizeof(a.world));a.lastFrame=m_frameNumber;++drawn;++groupDraws[a.group];
    }
    for(auto& original:m_livingOriginals){
        bool replaced=false;for(const auto& a:m_living->actors)replaced|=(a.replacementMask&original.first)&&a.parent<0&&a.lastFrame==m_frameNumber;
        if(!replaced){FlushGroup();original.second();++m_livingFallbackCount;if(m_livingReplaced)--m_livingReplaced;}
    }
    m_livingOriginals.clear();
    // Queued draws own their constants. Restore every caller-visible input before interface/overlay draws.
    FlushGroup();ProfileCpuAdd("living submit",cpu);ProfileMark("living meshes");
    if((m_frameNumber%600)==0){uint32_t seen=0;for(const auto& r:m_living->replacements)seen+=r.lastSeen==m_frameNumber;
        Log("Living visibility: camera %s; machinery original parts %u/%zu, stable %u, suppressed %u",m_livingWorldCamera?"world":"main-camera sky fallback",seen,m_living->replacements.size(),m_living->replacementStable,m_livingReplaced);
    }
    if((m_frameNumber%600)==0)Log("Living Rubi-Ka: %zu mesh draws; groups %u,%u,%u,%u,%u,%u; eye %.2f,%.2f,%.2f; time %.2f",drawn,groupDraws[0],groupDraws[1],groupDraws[2],groupDraws[3],groupDraws[4],groupDraws[5],eye.x,eye.y,eye.z,time);
}
}
