#include "presentation_policy.h"
// Captured studio lights, with synthetic neutral panels and HD-scale detail.
void RunLoginStudioFixture(Device& d,int frames,const std::string& shot,int mode){
    const int context=mode==6 ? rvk::presentation::LightingContext(0,-1,false):
        mode==7 ? rvk::presentation::LightingContext(1,2,true):1;
    const bool stock=mode==0 || mode==4,login=mode!=2 && context==1;
    d.SetKnob(Knob::LoginScene,login?1.f:0.f);d.SetPixelLighting(!stock);d.SetLightOverride(true);
    d.SetHdr(!stock);d.SetHdrHeadroom(1.5f);d.SetPointLightIntensity(.25f,.25f);d.SetBloom(1,.5f);d.SetKnob(Knob::NightGlow,1);
    Device::PbrSettings pbr;pbr.enabled=true;pbr.specular=1;pbr.ambient=1;d.SetPbr(pbr);
    std::vector<uint32_t> px(128*128);
    for(int y=0;y<128;++y)for(int x=0;x<128;++x){
        uint32_t v=((x/4+y/4)&1)?190:210;
        px[y*128+x]=0xff000000u|v<<16|v<<8|v;
        if((mode==4 || mode==5) && x<32)px[y*128+x]=0xffb9ded9;
        if((mode==4 || mode==5) && x>96)px[y*128+x]=0xfffafafa;
    }
    auto* texture=d.CreateTexture(128,128,px.data());uint32_t orm=0xfffe1800;d.SetMaterialMaps(texture,d.CreateTexture(1,1,&orm),nullptr);
    const VtxMesh panel[]={{-8,-2,8,0,0,-1,0xff505050,0,1},{-8,6,8,0,0,-1,0xff505050,0,0},{8,6,8,0,0,-1,0xff505050,1,0},{8,-2,8,0,0,-1,0xff505050,1,1}};
    const uint16_t ix[]={0,1,2,0,2,3};
    for(int f=0;f<frames;++f){
        if(frames>=24 && f>=frames-16)d.RequestScreenshot(shot+"-"+std::to_string(f)+".bmp");
        else if(f==frames-1)d.RequestScreenshot(shot);
        if(f==frames-1)d.RequestFrameDump("studio-frame.txt");
        d.BeginFrame();d.SetViewport({0,0,g_width,g_height,0,1});d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff202020,1);
        d.SetTransform(World,Identity());d.SetTransform(View,LookAtLH({.66f,2.27f,-10.55f},{0,2,8},{0,1,0}));d.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.1f,300));
        d.SetRenderState(RS_LIGHTING,1);d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_CULLMODE,CULL_NONE);d.SetRenderState(RS_AMBIENT,0);d.SetRenderState(RS_COLORVERTEX,1);d.SetRenderState(RS_EMISSIVEMATERIALSOURCE,MCS_COLOR1);
        d.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_MATERIAL);d.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_MATERIAL);
        Material m{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};d.SetMaterial(m);
        for(int i=0;i<3;++i){Light l{};l.type=LIGHT_POINT;l.position={i==0?-30.32f:i==1?9.44f:158.11f,0,0};l.range=200;l.attenuation1=.005f;l.diffuse=i==0?Color{0,0,0,1}:i==1?Color{.71f,.91f,.87f,1}:Color{1,.97f,.91f,1};d.SetLight(i,l);d.LightEnable(i,true);}
        d.SetTexture(0,texture);d.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);d.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);d.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);d.SetDrawVisual(uint32_t(VisualKind::Static),"RTriMesh_t");d.DrawIndexedPrimitive(TriangleList,kFvfMesh,panel,4,ix,6);
        d.SetDrawVisual(0,nullptr);d.SetRenderState(RS_LIGHTING,0);d.SetRenderState(RS_ZENABLE,0);d.SetTexture(0,nullptr);d.SetTextureStageState(0,TSS_COLOROP,TOP_SELECTARG2);
        const VtxRhwDiffuse ui[]={{8,8,0,1,0xff40c080},{40,8,0,1,0xff40c080},{40,24,0,1,0xff40c080},{8,24,0,1,0xff40c080}};d.DrawIndexedPrimitive(TriangleList,kFvfRhwDiffuse,ui,4,ix,6);d.EndFrame();
    }
    d.DestroyTexture(texture);
}
