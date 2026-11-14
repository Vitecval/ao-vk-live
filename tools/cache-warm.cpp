// Prepare this release's pipelines on the owner's GPU without loading AO data.
#include "rvk.h"
#include "win32_path.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>
int wmain(int argc,wchar_t** argv) {
    if(argc!=3 || std::wstring(argv[1])!=L"--cache") {std::fprintf(stderr,"Usage: ao-vk-cache-warm --cache <file>\n");return 2;}
    rvk::SetLogSink([](const char* line){std::printf("%s\n",line);std::fflush(stdout);});
    const bool automatic=std::wstring(argv[2])==L"auto";
    const std::string requested=rvk::win32::Utf8(argv[2]);
    if(!automatic&&requested.empty()){std::fprintf(stderr,"Invalid cache file path.\n");return 2;}
    // This isolated tool was explicitly asked to prepare a cache. An inherited
    // game/debug override (including "off") must not redirect the installer.
    if(_wputenv_s(L"RANDYVK_PIPELINE_CACHE",L"")!=0){std::fprintf(stderr,"Cannot select the requested cache destination.\n");return 2;}
    rvk::SetPipelineCachePath(automatic ? "" : requested.c_str());
    std::string cachePath;
    {
        rvk::Device device;std::string error;
        if(!device.Init(nullptr,320,240,&error)){std::fprintf(stderr,"Cache preparation failed: %s\n",error.c_str());return 1;}
        cachePath=device.PipelineCacheFilePath();
        using namespace rvk::d3d;
        device.SetHdr(true);device.SetShadows(true);device.SetKnob(rvk::Knob::WindowDepth,2);
        Matrix world{};for(int i=0;i<4;++i)world.m[i][i]=1;
        Matrix projection{};projection.m[0][0]=projection.m[1][1]=1;projection.m[2][2]=100.f/99.9f;projection.m[2][3]=1;projection.m[3][2]=-.1f*100.f/99.9f;
        struct Vertex {float x,y,z,nx,ny,nz;uint32_t color;float u,v;};
        const Vertex vertices[]={{-1,-1,3,0,0,-1,0xffffffff,0,1},{1,-1,3,0,0,-1,0xffffffff,1,1},{0,1,3,0,0,-1,0xffffffff,.5f,0}};
        Material material{};material.diffuse=material.ambient={1,1,1,1};
        const uint32_t stonePixel=0xff707478;
        auto* stone=device.CreateTexture(1,1,&stonePixel);
        device.SetTextureRdbId(stone,5955,1010004,0);
        device.SetKnob(rvk::Knob::EnhancedRocks,1);device.SetKnob(rvk::Knob::RockShape,1);
        for(int frame=0;frame<8;++frame){
            std::printf("Preparing HDR and post-process cache: sample frame %d of 8\n",frame+1);std::fflush(stdout);
            device.BeginFrame();device.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff17212b,1);
            device.SetTransform(World,world);device.SetTransform(View,world);device.SetTransform(Projection,projection);
            device.SetRenderState(RS_ZENABLE,1);device.SetRenderState(RS_ZWRITEENABLE,1);device.SetRenderState(RS_LIGHTING,1);device.SetRenderState(RS_AMBIENT,0xff808080);device.SetRenderState(RS_CULLMODE,CULL_NONE);
            device.SetMaterial(material);device.SetDrawVisual(uint32_t(rvk::VisualKind::Static),"AO-VK cache preparation");
            device.SetTexture(0,frame>=4?stone:nullptr);
            device.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);
            device.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);device.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
            device.DrawPrimitive(TriangleList,0x152,vertices,3);device.EndFrame();
        }
        device.DestroyTexture(stone);
    } // Serialize after startup and after the representative HDR/post passes.
    std::ifstream cache(std::filesystem::path(rvk::win32::FilePath(cachePath)),std::ios::binary|std::ios::ate);
    if(!cache || cache.tellg()<32 || cache.tellg()>=(256ll<<20)){std::fprintf(stderr,"Cache was not saved within the supported bound.\n");return 1;}
    std::puts("GPU cache prepared locally. No game process or proprietary game data was used.");return 0;
}
