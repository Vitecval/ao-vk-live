// Exercises production eligibility, shared/registered geometry and rigid dressing.
template<typename D> void RunRockClusterScene(D& dev,int frames,const std::string& shot){
    using namespace cliffpreview;
    dev.SetHdr(true);dev.SetSkyPlayfield(655);dev.SetPixelLighting(true);
    dev.SetKnob(Knob::EnhancedRocks,1);dev.SetKnob(Knob::RockShape,(g_visualToggle&1)?1.f:0.f);
    dev.SetBloom(0,1);dev.SetTonemap(1,1);dev.SetDarkness(0,0);dev.SetBump(0);
    dev.SetNormalMaps(true,.65f);
    Device::PbrSettings pbr;pbr.enabled=true;pbr.specular=.35f;pbr.ambient=.3f;pbr.occlusion=.6f;dev.SetPbr(pbr);
    Device::TerrainSettings terrain;terrain.rock=false;terrain.cliff=0;dev.SetTerrain(terrain);
    dev.SetShadows((g_visualToggle&256)!=0);dev.SetShadowParams(.8f,200,3);dev.SetPointShadows(0);
    auto stonePixels=Checker(128,16,0xFF737A72,0xFF626A64);
    Texture* stone=dev.CreateTexture(128,128,stonePixels.data());
    const char* identity=std::getenv("AOVK_FIXTURE_ROCK_ID");uint32_t id=identity?uint32_t(std::strtoul(identity,nullptr,10)):5955;
    dev.SetTextureRdbId(stone,id,1010004,0);
    auto grassPixels=Checker(128,32,0xFF586C39,0xFF4B6032);Texture* grass=dev.CreateTexture(128,128,grassPixels.data());
    std::vector<Texture*> catalogTextures;
    if(g_visualToggle&1024)for(uint64_t key:materials::rockCatalog){const uint32_t pixel=0xFF777777;
        auto* texture=dev.CreateTexture(1,1,&pixel);dev.SetTextureRdbId(texture,uint32_t(key),uint32_t(key>>32),0);catalogTextures.push_back(texture);}
    Model c=dressedCliff(false),boulder,floating,control;
    rock(boulder,{-11,1.4f,8},{3,2.3f,2.7f},37121,false,0,false);
    rock(floating,{11,10,8},{3.8f,3.f,3.4f},89713,false,0,false);
    rock(control,{-21,1.4f,2},{2.2f,2.2f,2.2f},9871,false,0,false);
    struct Item{Part* part;Texture* texture;bool natural;uint32_t slot;};
    Item items[]={{&c.stone,stone,true,1},{&boulder.stone,stone,true,2},{&floating.stone,stone,true,3},
                  {&c.moss,grass,false,4},{&c.ground,grass,false,5},{&control.stone,grass,false,6}};
    struct Baked{float x,y,z;uint32_t diffuse;float u,v;};
    std::vector<std::shared_ptr<const std::vector<uint8_t>>> vertexData,indexData;
    for(auto& item:items){
        auto v=std::make_shared<std::vector<uint8_t>>();
        if(g_visualToggle&32){for(auto p:item.part->v){Baked baked{p.x,p.y,p.z,0xFFB0B0B0,p.u,p.v};
            auto size=v->size();v->resize(size+sizeof(baked));std::memcpy(v->data()+size,&baked,sizeof(baked));}}
        else{v->resize(item.part->v.size()*sizeof(Vertex));std::memcpy(v->data(),item.part->v.data(),v->size());}
        auto i=std::make_shared<std::vector<uint8_t>>(item.part->ix.size()*2);std::memcpy(i->data(),item.part->ix.data(),i->size());
        vertexData.push_back(v);indexData.push_back(i);
        dev.RegisterMesh(item.slot,TriangleList,(g_visualToggle&32)?0x142:kFvfMesh,v,0,uint32_t(item.part->v.size()),i,uint32_t(item.part->ix.size()));
    }
    const Material mat{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};
    for(int frame=0;frame<frames;++frame){
        if(frame==frames/2)dev.ProfileWindow(true,"");
        if(frame==frames-1){dev.ProfileWindow(false,"clusters steady: ");dev.RequestScreenshot(shot);dev.RequestFrameDump("clusters-frame.txt");}
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFFB2C7D6,1);
        Vector eye{-3,3,-10},target{1,6,20};
        if(g_visualToggle&2){eye={-9,8,-75};target={0,8,25};}
        if(g_visualToggle&4)eye.x+=std::sin(frame*.012f)*7;
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(World,Identity());
        dev.SetTransform(View,LookAtLH(eye,target,{0,1,0}));dev.SetTransform(Projection,PerspectiveLH(kPi*.32f,float(g_width)/g_height,.1f,600));
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_ALPHATESTENABLE,0);
        if(g_visualToggle&8192){dev.SetRenderState(RS_ALPHABLENDENABLE,1);dev.SetRenderState(RS_SRCBLEND,BLEND_SRCALPHA);dev.SetRenderState(RS_DESTBLEND,BLEND_INVSRCALPHA);}
        dev.SetRenderState(RS_LIGHTING,(g_visualToggle&32)?0:1);dev.SetRenderState(RS_NORMALIZENORMALS,1);dev.SetRenderState(RS_AMBIENT,0xFF687782);
        dev.SetRenderState(RS_COLORVERTEX,1);dev.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_COLOR1);dev.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_COLOR1);
        Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={-.48f,-.75f,.42f};sun.diffuse={1.05f,1.02f,.97f,1};dev.SetLight(0,sun);dev.LightEnable(0,true);
        dev.SetMaterial(mat);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        for(size_t i=0;i<std::size(items);++i){auto& item=items[i];dev.SetTexture(0,item.texture);
            auto kind=(g_visualToggle&8)&&item.natural?VisualKind::Character:(g_visualToggle&2048)?VisualKind::Unknown:(g_visualToggle&4096)?VisualKind::Room:VisualKind::Static;
            dev.SetDrawVisual(uint32_t(kind),"rigid-cluster-runtime-fixture",(g_visualToggle&8)&&item.natural?42:0,item.slot*131);
            const uint32_t fvf=(g_visualToggle&32)?0x142:kFvfMesh;
            if(g_visualToggle&64)dev.DrawMesh(item.slot);
            else dev.DrawIndexedPrimitive(TriangleList,fvf,vertexData[i]->data(),uint32_t(item.part->v.size()),item.part->ix.data(),uint32_t(item.part->ix.size()));
        }
        if(g_visualToggle&128){dev.SetTexture(0,stone);dev.SetDrawVisual(uint32_t(VisualKind::Static),"cluster-dense-fixture",0,2139);
            for(int z=0;z<8;++z)for(int x=0;x<12;++x){dev.SetTransform(World,Translate((x-6)*8.f,0,(z+1)*9.f));dev.DrawMesh(2);}}
        for(size_t i=0;i<catalogTextures.size();++i){dev.SetTexture(0,catalogTextures[i]);
            dev.SetDrawVisual(uint32_t(VisualKind::Static),"all-natural-rock-identities",0,uint32_t(i+1001));
            dev.SetTransform(World,Translate((int(i)%16-8)*8.f,0,(int(i)/16+1)*10.f));dev.DrawMesh(2);}
        dev.SetTransform(World,Identity());dev.SetDrawVisual(0,nullptr);dev.EndFrame();
        if(frame<80)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    for(auto& item:items)dev.ReleaseMesh(item.slot);for(auto* texture:catalogTextures)dev.DestroyTexture(texture);dev.DestroyTexture(stone);dev.DestroyTexture(grass);
}
