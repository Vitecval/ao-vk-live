// Narrow friend probe for the real Device contract, never compiled into the addon.
#include "living.h"
#include <thread>
namespace rvk {
struct LivingRuntimeProbe {
    inline static int failures=0;inline static uint32_t actorFrames=0,replaced=0,nativeFrames=0,controlFrames=0;
    inline static std::array<uint32_t,6> peak{};
    inline static uint32_t peakPhase=0,peakLarge=0;
    inline static std::shared_ptr<skin::Job> control;
    static void Check(bool ok,const char* why){if(!ok){++failures;std::fprintf(stderr,"RUNTIME CONTRACT FAILED: %s\n",why);}}
    static void Stress(Device& d,int frame){
        if(!d.m_living)return;
        if(frame==2&&d.m_living->fixture&&std::getenv("RANDYVK_LIVING_TEST_ACTIONS")){
            uint32_t index=0;for(auto& a:d.m_living->actors)if(a.group==0){auto& h=a.human;h={};h.last=2./60;h.from=h.to=a.route.At(0);h.tangent={0,0,-1};h.resting=true;h.wait=12;h.activity=int32_t(index++%4);}
        }
        if(frame==60){
            d.DrawLiving();const double time=d.m_living->timeline.time;
            std::vector<std::array<float,3>> positions;
            for(const auto& actor:d.m_living->actors)positions.push_back({actor.world[3][0],actor.world[3][1],actor.world[3][2]});
            d.m_living->timeline.last-=15;d.m_livingFrame=~uint64_t(0);d.DrawLiving();
            Check(d.m_living->timeline.time==time,"actual scene holds route time after 15-second focus pause");
            for(size_t i=0;i<positions.size();++i)for(int axis=0;axis<3;++axis)
                Check(std::fabs(positions[i][axis]-d.m_living->actors[i].world[3][axis])<.001f,"focus pause preserves actual actor world position");
        }
        if(frame>=30&&frame%60==30){d.FlushGroup();auto& f=d.m_frames[d.m_frameIndex];f.ringOffset=f.ringSize-32*1024;}
    }
    static void Control(Device& d){
        if(!d.m_living||d.m_living->meshes.empty()||!d.m_living->meshes[0].skin)return;
        auto& m=d.m_living->meshes[0];if(!control){auto p=std::make_shared<skin::Palette>();p->Set(m.frames.data(),m.bones);control=std::make_shared<skin::Job>();control->source=m.skin;control->bones=p;control->ComputeBounds();}
        d3d::Matrix w{};for(int i=0;i<4;++i)w.m[i][i]=1;auto p=d.m_living->actors[0].route.points[0];w.m[3][0]=p.x;w.m[3][1]=p.y;w.m[3][2]=p.z+1.8f;
        d.SetTransform(d3d::World,w);d.SetTexture(0,m.texture);d.SetDrawVisual(uint32_t(VisualKind::Character),"normal-character-pipeline-control",0x123456,0x789);d.SetRenderState(d3d::RS_LIGHTING,1);d.SetRenderState(d3d::RS_ALPHABLENDENABLE,0);d.SetRenderState(d3d::RS_ZWRITEENABLE,1);
        d.DrawSkinned(d3d::TriangleList,skin::kVertexFvf,*control,0,m.vertexCount,m.skin->indices.data(),uint32_t(m.skin->indices.size()));++controlFrames;
    }
    static void State(Device& d){
        // The UI is deliberately hostile to the previous world state. Ambient actors must use their captured
        // world camera/lights, then return the entire caller-visible state and pick owner untouched.
        d3d::Matrix identity{};for(int i=0;i<4;++i)identity.m[i][i]=1;d.SetTransform(d3d::View,identity);d.SetTransform(d3d::Projection,identity);auto vp=d.m_viewport;vp.x=40;vp.y=30;vp.width=400;vp.height=200;d.SetViewport(vp);d.SetRenderState(d3d::RS_AMBIENT,0);
        for(size_t i=0;i<d.m_lights.size();++i)d.LightEnable(uint32_t(i),false);
        d.SetRenderState(d3d::RS_FOGENABLE,0);d.SetRenderState(d3d::RS_FOGCOLOR,0xff00ff00);
        d.m_pickActive=true;d.m_pickBestOwner=0xabcdef;d.m_pickBestVisual=0xfedcba;d.m_pickDepth=.123f;
        const auto rs=d.m_rs;const auto tss=d.m_tss;const auto tex=d.m_textures;const auto view=d.m_view,proj=d.m_proj,world=d.m_world;const auto mat=d.m_material;const auto viewport=d.m_viewport;
        const auto lights=d.m_lights;const auto owner=d.m_drawOwner,visual=d.m_drawVisual,kind=d.m_drawVisualKind;const char* name=d.m_drawVisualName;
        d.DrawLiving();
        Check(d.m_rs==rs&&d.m_tss==tss&&d.m_textures==tex,"render/sampler/texture state restored");
        Check(!std::memcmp(&view,&d.m_view,sizeof(view))&&!std::memcmp(&proj,&d.m_proj,sizeof(proj))&&!std::memcmp(&world,&d.m_world,sizeof(world)),"world/view/projection restored");
        Check(!std::memcmp(&mat,&d.m_material,sizeof(mat))&&!std::memcmp(&viewport,&d.m_viewport,sizeof(viewport)),"material and UI viewport restored");
        Check(d.m_lights.size()==lights.size()&&(!lights.size()||!std::memcmp(lights.data(),d.m_lights.data(),lights.size()*sizeof(Device::LightSlot))),"D3D light slots restored");
        Check(d.m_pickActive&&d.m_pickBestOwner==0xabcdef&&d.m_pickBestVisual==0xfedcba&&d.m_pickDepth==.123f,"cosmetics never enter picking");
        Check(d.m_drawOwner==owner&&d.m_drawVisual==visual&&d.m_drawVisualKind==kind&&d.m_drawVisualName==name,"ordinary character identity preserved");
        Check(!d.m_livingDrawing&&!d.m_livingPreviousWorld,"draw guard restored");d.m_pickActive=false;
        d.OverlayBegin();Check(!std::memcmp(&view,&d.m_view,sizeof(view))&&!std::memcmp(&proj,&d.m_proj,sizeof(proj))&&!std::memcmp(&viewport,&d.m_viewport,sizeof(viewport))&&d.m_rs[d3d::RS_AMBIENT]==rs[d3d::RS_AMBIENT],"shared exhaust world-camera override restores UI caller state");
    }
    static void Frame(Device& d){
        uint32_t count=0;for(size_t i=0;i<6;++i){peak[i]=std::max(peak[i],d.m_livingDraws[i]);count+=d.m_livingDraws[i];}if(count)++actorFrames;replaced+=d.m_livingReplaced;
        if(d.m_native.ground&&(d.m_native.drawnBase==d.m_frameNumber||d.m_native.drawnLight==d.m_frameNumber||d.m_native.drawnBase+1==d.m_frameNumber||d.m_native.drawnLight+1==d.m_frameNumber))++nativeFrames;
        for(const auto& l:d.m_lightsCur)if(l.cosmeticGroup<6)Check(!l.hasCarrier&&!l.owner&&l.carried==0,"equipment lights cannot become game-character carriers");
        if(d.m_living)Check(d.m_living->bytes<=living::MaxBytes,"decoded source budget");
        if(d.m_living){uint32_t phase=0,large=0,air=0;
            for(const auto& a:d.m_living->actors)if(a.lastFrame==d.m_frameNumber){
                if(a.group==2&&a.parent<0){++air;phase+=a.motion==7;large+=a.motion==8;}
                if(a.parent<0&&living::HasContinuousFlight(*d.m_living,a)){
                    const auto& view=d.m_livingView.m;float eye[3];for(int j=0;j<3;++j)eye[j]=-(view[3][0]*view[j][0]+view[3][1]*view[j][1]+view[3][2]*view[j][2]);
                    float distance=std::hypot(std::hypot(a.world[3][0]-eye[0],a.world[3][2]-eye[2]),a.world[3][1]-eye[1]);
                    Check(std::fabs(a.visibilityAlpha-living::Fade(distance,a.range))<.001f,"visible aircraft opacity has distance fading only, never schedule fading");}
                if((a.motion==7&&!d.m_living->flightLoops.empty())||(a.motion==8&&!d.m_living->largeLoops.empty())){bool inside=false;const auto* w=a.world[3];
                    const auto& loops=a.motion==7?d.m_living->flightLoops:d.m_living->largeLoops;
                    for(const auto& loop:loops)for(float distance=0;distance<loop.length;distance+=1){auto p=loop.At(distance);if(std::hypot(std::hypot(p.x-w[0],p.z-w[2]),p.y-w[1])<1.2f){inside=true;break;}}
                    Check(inside,"continuous aircraft stays on a certified closed flight plan");}
                else if(a.motion==7&&!d.m_living->flightLanes.empty()){bool inside=false;const auto* w=a.world[3];
                    for(const auto& lane:d.m_living->flightLanes){auto x=lane.points[0],y=lane.points[1];float dx=y.x-x.x,dy=y.y-x.y,dz=y.z-x.z;float u=((w[0]-x.x)*dx+(w[1]-x.y)*dy+(w[2]-x.z)*dz)/(dx*dx+dy*dy+dz*dz);
                        if(u>=0&&u<=1)inside|=std::hypot(std::hypot(w[0]-x.x-u*dx,w[2]-x.z-u*dz),w[1]-x.y-u*dy)<=1.2f;}
                    Check(inside,"varied flight stays inside a prepared lane steering envelope");}
                else if(a.motion>=7){const auto& p=a.route.points;const float* w=a.world[3];
                    for(int axis=0;axis<3;++axis)Check(w[axis]>=std::min((&p[0].x)[axis],(&p[1].x)[axis])-.01f&&w[axis]<=std::max((&p[0].x)[axis],(&p[1].x)[axis])+.01f,"flight remains inside validated straight segment");}
                if(a.job)for(const auto& b:d.m_living->actors)if(&a!=&b&&b.job&&b.lastFrame==a.lastFrame)
                    Check(a.job.get()!=b.job.get()&&a.job->bones.get()!=b.job->bones.get(),"each original character owns its animation job and palette");}
            peakPhase=std::max(peakPhase,phase);peakLarge=std::max(peakLarge,large);
            if(d.m_living->flightLoops.size()>=4){std::vector<int32_t> lanes;
                for(const auto& a:d.m_living->actors)if(a.motion==7&&a.flight.active){Check(std::find(lanes.begin(),lanes.end(),a.flight.lane)==lanes.end(),"active Phasefronts reserve distinct city routes");lanes.push_back(a.flight.lane);}}
            const auto level=uint32_t(std::clamp(d.m_phasefrontTraffic,0.f,4.f));
            Check(air<=living::CombinedAirLimit(level)&&phase<=living::PhasefrontLimit(level)&&large<=1,"combined air traffic and showcase caps");}
        if(d.m_living&&(d.m_frameNumber%30)==0)for(const auto& a:d.m_living->actors)if(a.motion==3&&a.job&&a.lastFrame+1>=d.m_frameNumber){
            std::vector<skin::Vertex> vertices(a.job->source->vertices.size());skin::SkinVertices(a.job->source->vertices.data(),uint32_t(vertices.size()),vertices.data(),*a.job->bones,false,nullptr,nullptr);float minimum=1e9f;
            for(const auto& v:vertices){float p[3];for(int j=0;j<3;++j)p[j]=v.pos[0]*a.world[0][j]+v.pos[1]*a.world[1][j]+v.pos[2]*a.world[2][j]+a.world[3][j];float h=a.surface?a.supportHeight:0;if(!a.surface&&!d.m_living->ground.empty()&&!living::GroundHeight(*d.m_living,p[0],p[2],h))continue;minimum=std::min(minimum,p[1]-h);}
            Check(minimum>-.02f&&minimum<.035f,"actual animated mesh stays in contact with the native floor");
        }

    }
    static void Camera(Device& d,bool sky,const d3d::Vector& expected){
        if(!d.m_living)return;Check(d.m_livingCameraValid,"aerial fallback has a camera");Check(d.m_livingWorldCamera!=sky,"opaque world camera supersedes sky fallback");
        const auto& v=d.m_livingView.m;float eye[3];for(int i=0;i<3;++i)eye[i]=-(v[3][0]*v[i][0]+v[3][1]*v[i][1]+v[3][2]*v[i][2]);
        Check(std::fabs(eye[0]-expected.x)<.01f&&std::fabs(eye[1]-expected.y)<.01f&&std::fabs(eye[2]-expected.z)<.01f,"sky camera retains real world origin");
    }
    static void History(Device& d){if(d.m_living)for(const auto& a:d.m_living->actors)if(a.parent<0&&a.job&&a.lastFrame+1>=d.m_frameNumber)Check(!a.job->prevBones,"clock discontinuity clears cosmetic motion history");}
    static void Context(Device& d,bool waiting,bool fog){if(waiting)Check(!d.m_living&&!d.m_livingAttempt,"unknown context waits before mesh IO");if(fog&&d.m_living)Check(d.m_livingWorldFog[0]==1,"captured world fog survives UI overrides");}
    static void Lifecycle(Device& d,int f){
        if(f==39){Device::SmokeVolume orphan{};orphan.key=1;orphan.parent=0x20004c5200000100ull;d.m_smokeVolumes.push_back(orphan);}
        const bool blocked=(f>=40&&f<70)||(f>=130&&f<150)||(f>=200&&f<225);
        if(blocked){Check(!d.m_living,"lifecycle removes active scene");for(const auto& v:d.m_smokeVolumes)Check((v.key&0xffffffffffff0000ull)!=0x20004c5200000000ull&&(v.parent&0xffffffffffff0000ull)!=0x20004c5200000000ull,"lifecycle removes every cosmetic smoke key, including actor 256");for(auto n:d.m_livingDraws)Check(n==0,"lifecycle leaves no cosmetic draws");for(const auto& l:d.m_lightsPrev)Check(l.cosmeticGroup>=6,"disabled context removes previous equipment lights");}
        if(f==120||f==180||f==250)Check(d.m_living!=nullptr,"same-playfield generation and re-enable reload");
    }
    static int Finish(Device& d,bool expected,bool ground,bool ordinary=true){
        Check(expected?actorFrames>0:actorFrames==0,"normal end-of-scene submission/review gate");
        if(std::getenv("RANDYVK_LIVING_EXPECT_AIR")){const auto level=uint32_t(std::clamp(d.m_phasefrontTraffic,0.f,4.f));
            Check(peakPhase==living::PhasefrontLimit(level)&&peakLarge==1&&peak[2]==living::CombinedAirLimit(level),"forced-schedule fixture actually exercised all three aircraft caps");}
        if(expected){if(ordinary)Check(controlFrames>0,"ordinary skinned draw coexists");if(ground)Check(nativeFrames>0,"native/shared ground replacement really executed");}
        std::printf("RUNTIME CONTRACT: %s; actor_frames=%u; normal_skin_frames=%u; replaced_parts=%u; native_ground_frames=%u; fallback_parts=%u; peak_groups=%u,%u,%u,%u,%u,%u; NOT LIVE GAMEPLAY\n",failures?"FAILED":"PASS",actorFrames,controlFrames,replaced,nativeFrames,d.m_livingFallbackCount,peak[0],peak[1],peak[2],peak[3],peak[4],peak[5]);
        d.DestroyLiving();control.reset();return failures?1:0;
    }
};
}
