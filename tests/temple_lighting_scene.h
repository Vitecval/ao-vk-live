// Captured Redeemed temple light layout, with synthetic stone and geometry.
// No retail client or report service is touched by this fixture.
void RunTempleLightingFixture(Device& dev,int frames,const std::string& shot,bool marker,bool baked,bool sceneClear,bool matte,bool pbrOff) {
    std::vector<uint32_t> pixels(64*64);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        const uint32_t v=((x/8+y/8)&1) ? 175u:220u;
        pixels[y*64+x]=0xff000000u|(v<<16)|(v<<8)|v;
    }
    Texture* stone=dev.CreateTexture(64,64,pixels.data());
    if(matte){
        dev.SetTextureRdbId(stone,217909,1010004);uint32_t orm=0xfffe1000;
        dev.SetMaterialMaps(stone,dev.CreateTexture(1,1,&orm),nullptr);
        Device::PbrSettings p;p.enabled=!pbrOff;p.specular=4;p.ambient=4;p.reflections=1;dev.SetPbr(p);
    }
    Texture* white=dev.CreateTexture(8,8,std::vector<uint32_t>(64,0xffffffffu).data());
    const uint16_t ix[6]={0,1,2,0,2,3};
    VtxMesh floor[4]={{0,0,0,0,1,0,0xffffffff,0,0},{0,0,80,0,1,0,0xffffffff,0,4},{140,0,80,0,1,0,0xffffffff,4,4},{140,0,0,0,1,0,0xffffffff,4,0}};
    VtxMesh wall[4]={{0,0,80,0,0,-1,0xffffffff,0,0},{0,45,80,0,0,-1,0xffffffff,0,2},{140,45,80,0,0,-1,0xffffffff,4,2},{140,0,80,0,0,-1,0xffffffff,4,0}};
    if(baked){for(auto& v:floor)v.diffuse=0xff363a36u;for(auto& v:wall)v.diffuse=0xfff8fcf8u;}
    const Vector positions[4]={{26.82f,28.65f,39.13f},{120.88f,28.65f,39.13f},{71.70f,2.76f,39.14f},{71.70f,38.73f,39.14f}};
    const Vector dirs[4]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    for(int frame=0;frame<frames;++frame) {
        if(frame==frames-1){dev.RequestScreenshot(shot);dev.RequestFrameDump("temple-frame.txt");}
        dev.BeginFrame();dev.SetViewport({0,0,g_width,g_height,0,1});dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff24282cu,1);
        dev.SetTransform(World,Identity());dev.SetTransform(View,LookAtLH({38,9,8},{70,9,60},{0,1,0}));dev.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.3f,600));
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_LIGHTING,0);
        dev.SetTexture(0,stone);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        dev.SetDrawVisual(uint32_t(VisualKind::Sky),"GenericMeshObject_t");dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,wall,4,ix,6);
        if(sceneClear) {
            const float w=float(g_width),h=float(g_height);
            VtxRhwDiffuseTex q[4]={{0,0,1,1,0xff24282c,0,0},{0,h,1,1,0xff24282c,0,1},{w,0,1,1,0xff24282c,1,0},{w,h,1,1,0xff24282c,1,1}};
            dev.SetDrawVisual(uint32_t(VisualKind::Other),"VisualClearScreen_t");
            dev.SetRenderState(RS_ZWRITEENABLE,0);dev.SetRenderState(RS_ZFUNC,CMP_ALWAYS);dev.SetTexture(0,white);dev.DrawPrimitive(TriangleStrip,kFvfRhwDiffuseTex,q,4);
        }
        if(marker) {
            // Same state as Q41 in both retail temple captures. An offscreen
            // screen-space probe between sky and rooms must not finish HDR.
            VtxRhwDiffuseTex q[4]={{-12,-12,0,1,0xffff0000,0,0},{-12,-8,0,1,0xffff0000,0,1},{-8,-12,0,1,0xffff0000,1,0},{-8,-8,0,1,0xffff0000,1,1}};
            dev.SetRenderState(RS_ZWRITEENABLE,0);dev.SetRenderState(RS_ZFUNC,CMP_ALWAYS);dev.SetTexture(0,white);dev.DrawPrimitive(TriangleStrip,kFvfRhwDiffuseTex,q,4);
        }
        dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);dev.SetRenderState(RS_LIGHTING,1);dev.SetRenderState(RS_AMBIENT,0xff1e1e1eu);
        dev.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_MATERIAL);dev.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_MATERIAL);
        dev.SetRenderState(RS_EMISSIVEMATERIALSOURCE,baked ? MCS_COLOR1:MCS_MATERIAL);dev.SetRenderState(RS_COLORVERTEX,1);
        Material mat{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};dev.SetMaterial(mat);
        for(int i=0;i<4;++i){Light l{};l.type=LIGHT_SPOT;l.position=positions[i];l.direction=dirs[i];l.diffuse={1,1,1,1};l.range=583;l.attenuation0=.05f;l.attenuation2=.00002f;l.theta=kPi*.7f;l.phi=kPi*.95f;l.falloff=1;dev.SetLight(i,l);dev.LightEnable(i,true);}
        dev.SetDrawVisual(uint32_t(VisualKind::Room),"VisualRoom_t");dev.SetTexture(0,stone);dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,floor,4,ix,6);
        dev.SetDrawVisual(uint32_t(VisualKind::Static),"RTriMesh_t");dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,wall,4,ix,6);
        dev.SetDrawVisual(0,nullptr);dev.SetRenderState(RS_ZENABLE,0);dev.SetRenderState(RS_LIGHTING,0);dev.SetTexture(0,nullptr);dev.SetTextureStageState(0,TSS_COLOROP,TOP_SELECTARG2);
        const VtxRhwDiffuse ui[4]={{8,8,0,1,0xff40c080},{40,8,0,1,0xff40c080},{40,24,0,1,0xff40c080},{8,24,0,1,0xff40c080}};
        dev.DrawIndexedPrimitive(TriangleList,kFvfRhwDiffuse,ui,4,ix,6);dev.EndFrame();
    }
    dev.DestroyTexture(stone);dev.DestroyTexture(white);
}
