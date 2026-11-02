// Controlled renderer laboratory, with optional private Newland reconstruction. Never live gameplay.
#include "rvk.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
using namespace rvk;using namespace rvk::d3d;
#include "living_world.h"
#include "combined_window_fixture.h"
#ifdef LIVING_RUNTIME_CHECK
#include "living_runtime_probe.h"
#endif
namespace {
bool skyFirst=false;bool skyOnly=false;bool worldFog=false;float fixtureFov=.65f;Vector overrideEye{},overrideAt{};bool customEye=false,customAt=false;
Matrix Identity(){Matrix m{};for(int i=0;i<4;++i)m.m[i][i]=1;return m;}
Vector Sub(Vector a,Vector b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float Dot(Vector a,Vector b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vector Cross(Vector a,Vector b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vector Norm(Vector a){float n=std::sqrt(Dot(a,a));return {a.x/n,a.y/n,a.z/n};}
Matrix MakeView(Vector eye,Vector at){Vector z=Norm(Sub(at,eye)),x=Norm(Cross({0,1,0},z)),y=Cross(z,x);Matrix m=Identity();
    m.m[0][0]=x.x;m.m[1][0]=x.y;m.m[2][0]=x.z;m.m[0][1]=y.x;m.m[1][1]=y.y;m.m[2][1]=y.z;m.m[0][2]=z.x;m.m[1][2]=z.y;m.m[2][2]=z.z;m.m[3][0]=-Dot(x,eye);m.m[3][1]=-Dot(y,eye);m.m[3][2]=-Dot(z,eye);return m;}
void Box(Device& d,Vector c,Vector s,uint32_t colour){
    struct V{float x,y,z,nx,ny,nz;};
    const float x[2]={c.x-s.x/2,c.x+s.x/2},y[2]={c.y-s.y/2,c.y+s.y/2},z[2]={c.z-s.z/2,c.z+s.z/2};
    Vector p[8]={{x[0],y[0],z[0]},{x[1],y[0],z[0]},{x[1],y[1],z[0]},{x[0],y[1],z[0]},{x[0],y[0],z[1]},{x[1],y[0],z[1]},{x[1],y[1],z[1]},{x[0],y[1],z[1]}};
    const int faces[6][4]={{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{3,7,6,2},{0,1,5,4}};
    Material m{};m.diffuse={float((colour>>16)&255)/255,float((colour>>8)&255)/255,float(colour&255)/255,1};m.ambient=m.diffuse;m.power=12;d.SetMaterial(m);d.SetTexture(0,nullptr);
    for(const auto& f:faces){Vector n=Norm(Cross(Sub(p[f[1]],p[f[0]]),Sub(p[f[2]],p[f[0]])));V v[6];const int order[6]={0,1,2,0,2,3};for(int i=0;i<6;++i){auto q=p[f[order[i]]];v[i]={q.x,q.y,q.z,n.x,n.y,n.z};}d.DrawPrimitive(TriangleList,0x12,v,6);}
}
void Scene(Device& d,bool night,const std::string& camera){
    d.SetViewport({0,0,1920,1080,0,1}); // a new world frame follows the hostile UI-viewport probe
    d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,night?0xff131b22:0xff87969c,1);
    Vector eye={6,4,-8},at={0,2,3};
    if(camera=="newland"){eye={367.7f,32.8f,317.7f};at={385,31.8f,328};}
    if(camera=="market"){eye={398,33.3f,308};at={408,32.5f,315};}
    if(camera=="survey"){eye={365,155,220};at={365,28,320};}
    if(camera=="drone"){eye={1.f,3.3f,5.6f};at={-1.1f,2.1f,2.1f};}
    if(camera=="machine"){eye={4.3f,3.7f,-.8f};at={1.9f,.75f,2.4f};}
    if(camera=="traffic"){eye={0,12,-8};at={0,24,90};}
    if(camera=="people"){eye={8,3.5f,-8};at={0,1.1f,3.5f};}
    if(camera=="portrait"){eye={-3.8f,1.7f,0};at={-6,1.35f,3.5f};}
    if(camera=="wildlife"){eye={7,3,-6};at={0,1.1f,4};}
    if(customEye)eye=overrideEye;if(customAt)at=overrideAt;
    d.SetTransform(d3d::View,MakeView(eye,at));Matrix proj{};const float y=1/std::tan(fixtureFov/2);proj.m[0][0]=y*9/16;proj.m[1][1]=y;proj.m[2][2]=600.f/(600.f-.1f);proj.m[2][3]=1;proj.m[3][2]=-.1f*600.f/(600.f-.1f);d.SetTransform(Projection,proj);d.SetTransform(World,Identity());
    d.SetRenderState(RS_LIGHTING,1);d.SetRenderState(RS_AMBIENT,night?0x151c25:0x62696a);d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);d.SetRenderState(RS_CULLMODE,CULL_NONE);d.SetRenderState(RS_ALPHABLENDENABLE,0);d.SetRenderState(RS_COLORVERTEX,0);
    d.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);d.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);d.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);d.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
    Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={.35f,-.8f,.25f};sun.diffuse=night?Color{.12f,.17f,.24f,1}:Color{.85f,.82f,.73f,1};d.SetLight(0,sun);d.LightEnable(0,true);
    Light lamp{};lamp.type=LIGHT_POINT;lamp.position={-1,4,-1};lamp.diffuse=night?Color{1.5f,1.3f,1.f,1}:Color{.3f,.32f,.32f,1};lamp.range=18;lamp.attenuation0=1;lamp.attenuation2=.025f;d.SetLight(1,lamp);d.LightEnable(1,true);
        if(worldFog){d.SetRenderState(RS_FOGENABLE,1);d.SetRenderState(RS_FOGCOLOR,0xff536674);d.SetRenderState(RS_FOGVERTEXMODE,3);float start=6,end=38;uint32_t a,b;std::memcpy(&a,&start,4);std::memcpy(&b,&end,4);d.SetRenderState(RS_FOGSTART,a);d.SetRenderState(RS_FOGEND,b);}
    if(skyOnly||skyFirst){d.SetDrawVisual(uint32_t(VisualKind::Sky),"sky-only-camera-regression",0,0);d.SetRenderState(RS_ZWRITEENABLE,0);d.SetRenderState(RS_LIGHTING,0);Box(d,{400,27,315},{1800,1800,1800},0x87969c);if(skyOnly)return;d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_LIGHTING,1);}
    if(!worldParts.empty()){DrawWorld(d);DrawCombinedWindow(d);return;}
    // Laboratory architecture only: all screenshots explicitly identify it as a fixture.
    Box(d,{0,-.15f,5},{30,.3f,28},0x535952);Box(d,{0,2.2f,9},{22,4.4f,.4f},0x68706c);
    for(int i=-3;i<4;++i){Box(d,{float(i*3),2.3f,8.7f},{.15f,4.6f,.24f},0x343d3c);Box(d,{float(i*3),4.1f,8.65f},{2.7f,.10f,.2f},0xb4ac83);}
    if(camera!="people"&&camera!="portrait"&&camera!="wildlife"){
        Box(d,{1.9f,.35f,2.4f},{1.8f,.7f,1.8f},0x444e4b);Box(d,{1.9f,.77f,2.4f},{1.65f,.08f,1.65f},0x71796e);
        Box(d,{2.9f,.15f,2.4f},{.2f,.3f,.25f},0x39453f);
    }
    for(int i=0;i<6;++i)Box(d,{-4.f+i*.12f,1.1f,8.5f},{.055f,1.7f,.2f},0x383e3a);
}
}
int main(int argc,char**argv){
    std::string shot="fixture.bmp",scene,camera="wide",film,world,groundData;const char* windowFile=nullptr;int windowMode=0;int groundMode=0;bool liveStress=false;bool occlude=false,review=false,indoor=false,fanDrop=false,unknownContext=false,lateContext=false,lifecycle=false,animate=false,tour=false,fog=false,expectNone=false,timeJump=false;int frames=120,level=1,group=-1,cycles=0;bool night=false,hdr=true,toggle=false,shadows=false;uint32_t zone=0xfffffffeu;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--window-map")windowFile=argv[++i];else if(a=="--window-mode")windowMode=std::atoi(argv[++i]);else if(a=="--sky-first")skyFirst=true;else if(a=="--sky-only")skyOnly=true;else if(a=="--live-stress")liveStress=true;else if(a=="--time-jump")timeJump=true;else if(a=="--expect-none")expectNone=true;else if(a=="--unknown-context")unknownContext=true;else if(a=="--late-context")lateContext=true;else if(a=="--lifecycle")lifecycle=true;else if(a=="--animate")animate=true;else if(a=="--tour")tour=true;else if(a=="--fog")fog=true;else if(a=="--fan-drop")fanDrop=true;else if(a=="--shot")shot=argv[++i];else if(a=="--scene")scene=argv[++i];else if(a=="--fov")fixtureFov=float(std::atof(argv[++i]));else if(a=="--eye"){overrideEye={float(std::atof(argv[i+1])),float(std::atof(argv[i+2])),float(std::atof(argv[i+3]))};i+=3;customEye=true;}else if(a=="--at"){overrideAt={float(std::atof(argv[i+1])),float(std::atof(argv[i+2])),float(std::atof(argv[i+3]))};i+=3;customAt=true;}else if(a=="--indoor")indoor=true;else if(a=="--review")review=true;else if(a=="--occlude")occlude=true;else if(a=="--ground-mode")groundMode=std::atoi(argv[++i]);else if(a=="--ground-data")groundData=argv[++i];else if(a=="--world")world=argv[++i];else if(a=="--frames")frames=std::atoi(argv[++i]);else if(a=="--level")level=std::atoi(argv[++i]);else if(a=="--group")group=std::atoi(argv[++i]);else if(a=="--camera")camera=argv[++i];else if(a=="--shadows")shadows=true;else if(a=="--night")night=true;else if(a=="--hdr-off")hdr=false;else if(a=="--toggle")toggle=true;else if(a=="--cycles")cycles=std::atoi(argv[++i]);else if(a=="--zone")zone=uint32_t(std::strtoul(argv[++i],nullptr,0));else if(a=="--film")film=argv[++i];}
    worldFog=fog;
    if(!scene.empty())_putenv_s("RANDYVK_LIVING_SCENE",scene.c_str());
    if(review)_putenv_s("RANDYVK_LIVING_REVIEW","1");
    Device::SetFixedClock(1.0/60);Device d;std::string error;if(!d.Init(nullptr,1920,1080,&error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
    if(!world.empty()&&!LoadWorld(d,world))return 3;
    if(windowFile)PrepareCombinedWindow(d,windowFile,windowMode);
    if(!PrepareWorldGround(d,groundData,groundMode))return 4;
    std::printf("GPU: %s; driver: %s; Vulkan API: %u\n",d.Info().gpu.c_str(),d.Info().driver.c_str(),d.Info().apiVersion);
    if(liveStress){d.SetTessellation(.6f,16.f,3);d.SetPointShadows(16);}
    d.SetHdr(hdr);d.SetShadows(shadows);d.SetShadowParams(.8f,60,2);d.SetSkyPlayfield(zone);if(!unknownContext&&!lateContext)d.SetLivingContext(zone,!indoor,1);Device::SmokeParams smoke;smoke.enable=true;smoke.resolution=8;smoke.budget=.5f;d.SetSmokeParams(smoke);
    double submitMs=0;
    for(int f=0;f<frames;++f){
        if(f==std::min(120,frames/3))d.ProfileWindow(true,"living fixture warm");
#ifndef LIVING_BASELINE
        const Knob knobs[6]={Knob::LivePeople,Knob::LiveDrones,Knob::AirTraffic,Knob::LiveWild,Knob::LiveMachine,Knob::LiveAtmos};
        for(int g=0;g<6;++g)d.SetKnob(knobs[g],float((toggle&&f>frames/2)||(lifecycle&&f>=130&&f<150)?0:(group<0||group==g)?(g>=4?std::min(level,1):std::min(level,(g==0||g==1)?3:2)):0));
#endif
        if(cycles&&f%cycles==0)d.SetSkyPlayfield((f/cycles)%2?565:zone);
        if(lateContext&&f==40)d.SetLivingContext(zone,true,1);
        if(lifecycle){if(f==40)d.SetLivingContext(zone,false,2);if(f==70)d.SetLivingContext(zone,true,3);if(f==110)d.SetLivingContext(zone,true,4);if(f==200){d.SetLivingContext(565,true,5);d.SetSkyPlayfield(565);}if(f==225){d.SetLivingContext(zone,true,6);d.SetSkyPlayfield(zone);}}
        if(tour){customEye=customAt=true;switch((f/360)%5){case 0:overrideEye={432.57615f,26.7f,320.17865f};overrideAt={422.57615f,25.9f,302.85814f};break;case 1:overrideEye={418,61,302};overrideAt={418,34,339};break;case 2:overrideEye={410,49,332};overrideAt={412,47,358};break;case 3:overrideEye={340,130,300};overrideAt={325,151,360};break;default:overrideEye={392,106,282};overrideAt={382,104,302};break;}fixtureFov=.95f;}
        if(!film.empty()||animate) {std::string t=std::to_string(double(f)/60+(timeJump&&f>=100&&f<150?30.:0.));_putenv_s("RANDYVK_LIVING_TIME",t.c_str());}
        d.BeginFrame();const auto start=std::chrono::steady_clock::now();worldSkipPart=fanDrop&&f>=100&&f<107?946:-1;Scene(d,night,camera);
        if(occlude)Box(d,{0,3,0},{80,10,.3f},0x535952);
#ifdef LIVING_RUNTIME_CHECK
        if(liveStress)LivingRuntimeProbe::Stress(d,f);
        if(!skyOnly)LivingRuntimeProbe::Control(d);if(f==90)LivingRuntimeProbe::State(d);d.OverlayBegin();
#endif
        if(!film.empty()&&f%4==0){char n[64];std::snprintf(n,sizeof(n),"/frame-%04d.bmp",f);d.RequestScreenshot(film+n);}
        if(f==frames-1)d.RequestScreenshot(shot);d.EndFrame();
#ifdef LIVING_RUNTIME_CHECK
        LivingRuntimeProbe::Frame(d);if(skyOnly||skyFirst)LivingRuntimeProbe::Camera(d,skyOnly,overrideEye);if(lifecycle)LivingRuntimeProbe::Lifecycle(d,f);LivingRuntimeProbe::Context(d,lateContext&&f<40,fog);if(timeJump&&(f==100||f==150))LivingRuntimeProbe::History(d);std::this_thread::sleep_for(std::chrono::milliseconds(8));
#endif
        submitMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
    d.ProfileWindow(false,"living fixture warm");d.ProfileManualEnd();
    std::printf("CONTROLLED FIXTURE (not gameplay); frames=%d level=%d group=%d HDR=%d night=%d; CPU wall submit+frame=%.4f ms (includes waits/readback; not isolated overhead); GPU smoothed=%.4f ms\n",frames,level,group,hdr,night,submitMs/frames,d.m_liveGpuMs.load());
#ifdef LIVING_RUNTIME_CHECK
    return LivingRuntimeProbe::Finish(d,level>0&&!indoor&&!unknownContext&&!expectNone&&(zone==0xfffffffeu||review||living::LocalReviewEnabled(scene)),groundMode>0,!skyOnly);
#else
    return 0;
#endif
}
