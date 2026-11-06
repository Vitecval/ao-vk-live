// Prepare this release's pipelines on the owner's GPU without loading AO data.
#include "rvk.h"
#include <cstdio>
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=3 || std::string(argv[1])!="--cache") {std::fprintf(stderr,"Usage: ao-vk-cache-warm --cache <file>\n");return 2;}
    rvk::SetLogSink([](const char* line){std::printf("%s\n",line);std::fflush(stdout);});
    const bool automatic=std::string(argv[2])=="auto";
    rvk::SetPipelineCachePath(automatic ? "" : argv[2]);
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
        for(int frame=0;frame<8;++frame){
            device.BeginFrame();device.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff17212b,1);
            device.SetTransform(World,world);device.SetTransform(View,world);device.SetTransform(Projection,projection);
            device.SetRenderState(RS_ZENABLE,1);device.SetRenderState(RS_ZWRITEENABLE,1);device.SetRenderState(RS_LIGHTING,1);device.SetRenderState(RS_AMBIENT,0xff808080);device.SetRenderState(RS_CULLMODE,CULL_NONE);
            device.SetMaterial(material);device.SetDrawVisual(uint32_t(rvk::VisualKind::Static),"AO-VK cache preparation");
            device.DrawPrimitive(TriangleList,0x152,vertices,3);device.EndFrame();
        }
    } // Serialize after startup and after the representative HDR/post passes.
    std::ifstream cache(cachePath,std::ios::binary|std::ios::ate);
    if(!cache || cache.tellg()<32 || cache.tellg()>=(256ll<<20)){std::fprintf(stderr,"Cache was not saved within the supported bound.\n");return 1;}
    std::puts("GPU cache prepared locally. No game process or proprietary game data was used.");return 0;
}
