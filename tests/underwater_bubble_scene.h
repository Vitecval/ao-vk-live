// Captured-water coverage and optional ambient life, with no native game data.
int g_bubbleTest=0;
void RunUnderwaterBubbleTest(Device& dev,int frames,const std::string& shot)
{
    dev.SetHdr(true);dev.SetSkyPlayfield(g_bubbleTest==8?4605:4873);
    dev.SetKnob(Knob::AmbientMotes,g_bubbleTest==2||g_bubbleTest==10?0.f:g_bubbleTest==12?.001f:2.f);
    dev.SetKnob(Knob::AmbientBright,g_bubbleTest==7?0.f:1.5f);
    dev.SetKnob(Knob::AmbientStyle,g_bubbleTest==6?2.f:0.f);
    dev.SetDarkness(0,0);Device::WaterParams wp;wp.enable=true;wp.waves=0;dev.SetWaterParams(wp);
    const uint32_t colour=0xFF182C38;auto* texture=dev.CreateTexture(1,1,&colour);
    struct V{float x,y,z,nx,ny,nz,u,v;};
    const V wall[]={{-40,45,20,0,0,-1,0,0},{40,45,20,0,0,-1,1,0},{-40,110,20,0,0,-1,0,1},{40,110,20,0,0,-1,1,1}};
    const uint16_t ix[]={0,2,1,1,2,3};
    const float shift=g_bubbleTest==4||g_bubbleTest==10?80.f:0.f;
    std::vector<Device::WaterVertex> water={{{-32+shift,80,-32},0xFF456D86},{{32+shift,80,-32},0xFF456D86},{{-32+shift,80,32},0xFF456D86},{{32+shift,80,32},0xFF456D86}};
    std::vector<uint16_t> waterIndices(ix,ix+6);
    if(g_bubbleTest==14){
        water={{{-32,80,-32},0xFF456D86},{{-.8f,80,-32},0xFF456D86},{{-32,80,32},0xFF456D86},{{-.8f,80,32},0xFF456D86},
               {{.8f,79.6f,-32},0xFF456D86},{{32,79.6f,-32},0xFF456D86},{{.8f,79.6f,32},0xFF456D86},{{32,79.6f,32},0xFF456D86}};
        for(unsigned i=0;i<6;++i)waterIndices.push_back(uint16_t(ix[i]+4));
    }
    for(int frame=0;frame<frames;++frame){
        if(frame==frames-1){dev.RequestScreenshot(shot);dev.RequestFrameDump("bubble-frame.txt",1);}
        dev.BeginFrame();dev.SetViewport({0,0,g_width,g_height,0,1});dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFF182C38,1);
        const float ey=g_bubbleTest==3||g_bubbleTest==9?82.f:g_bubbleTest==14?79.2f:73.f;
        const float ex=g_bubbleTest==13?31.9f:g_bubbleTest==14?-2.f:0.f;
        dev.SetTransform(View,LookAtLH({ex,ey,0},{ex,ey,20},{0,1,0}));
        dev.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.1f,200));dev.SetTransform(World,Identity());
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_LIGHTING,0);
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_ALPHATESTENABLE,0);
        dev.SetTextureStageState(0,TSS_COLOROP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);
        dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);dev.SetTexture(1,nullptr);dev.SetTexture(0,texture);
        dev.DrawIndexedPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(1u<<8),wall,4,ix,6);
        dev.DrawWater(water.data(),uint32_t(water.size()),waterIndices.data(),uint32_t(waterIndices.size()));dev.EndFrame();
    }
    std::printf("Bubble physical context: %u; different zone: %u\n",unsigned(dev.UnderwaterLive(g_bubbleTest==8?4605:4873)),unsigned(dev.UnderwaterLive(152)));
    dev.DestroyTexture(texture);
}
