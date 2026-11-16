// Real Vulkan color-chart rendering. No game executable or proprietary data.
#include "rvk.h"
#include <filesystem>
#include <cstdio>
#include <string>
int main(int argc,char** argv){
    if(argc!=2)return 2;std::filesystem::path out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
    rvk::SetPipelineCachePath((out/"pipelines.cache").string().c_str());
    rvk::SetLogSink([](const char* s){std::puts(s);});
    rvk::Device d;std::string error;if(!d.Init(nullptr,256,128,&error)){std::puts(error.c_str());return 3;}
    using namespace rvk::d3d;
    d.SetHdr(true);d.SetTaa(false,0);d.SetKnob(rvk::Knob::LoginScene,1); // raw authored scene for an independent numeric reference
    Matrix identity{};for(int i=0;i<4;++i)identity.m[i][i]=1;
    const uint32_t colors[]={0xff000000,0xff202020,0xff606060,0xffa0a0a0,0xffeeeeee,0xffcc3333,0xff33bb55,0xff4466cc};
    const char* modes[]={"off","zero","on","off-again","static","static-bright","adapt-bright"};
    for(int mode=0;mode<7;++mode){
        d.SetKnob(rvk::Knob::VividTone,mode==0||mode==3?0:1);d.SetKnob(rvk::Knob::VividAmt,mode==1?0:.9f);
        d.SetKnob(rvk::Knob::VividStatic,mode==4||mode==5?1:0);
        for(int frame=0;frame<(mode==6?40:1);++frame){
            d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff000000,1);
            d.SetTransform(World,identity);d.SetTransform(View,identity);d.SetTransform(Projection,identity);
            d.SetRenderState(RS_LIGHTING,0);d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_CULLMODE,CULL_NONE);
            d.SetTexture(0,nullptr);d.SetTextureStageState(0,TSS_COLOROP,TOP_SELECTARG1);d.SetTextureStageState(0,TSS_COLORARG1,TA_DIFFUSE);
            for(int i=0;i<8;++i){
                struct V{float x,y,z;uint32_t c;};float left=-1.f+i*.25f,right=left+.25f;
                const uint32_t c=mode>=5 ? 0xffdddddd:colors[i];
                V v[]={{left,1,.5f,c},{right,1,.5f,c},{left,-1,.5f,c},{right,-1,.5f,c}};
                d.DrawPrimitive(TriangleStrip,0x42,v,4);
            }
            struct UI{float x,y,z,rhw;uint32_t c;};UI ui[]={{4,4,0,1,0xffb449d2},{28,4,0,1,0xffb449d2},{4,24,0,1,0xffb449d2},{28,24,0,1,0xffb449d2}};
            d.SetRenderState(RS_ZENABLE,0);d.DrawPrimitive(TriangleStrip,0x44,ui,4);
            if(frame==(mode==6?39:0))d.RequestScreenshot((out/(std::string(modes[mode])+".bmp")).string());
            d.EndFrame();
            if(mode==6)Sleep(16);
        }
    }
    std::puts("Rendered Vivid Tone fixtures: default off, zero strength, enabled, restore off, static and adapting brightness; untouched UI patch.");
}
