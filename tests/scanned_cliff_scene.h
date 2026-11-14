// Photogrammetry art preview. Original scan UVs and geometry remain intact;
// only fixed placement / uniform scale are applied, never camera deformation.
namespace scanpreview {
struct Asset {std::vector<cliffpreview::Part> parts;Texture* texture=nullptr;};
Asset geometry(const std::string& root,const std::string& name){
    Asset a;std::ifstream input(root+"/"+name+"/mesh.rvkmesh",std::ios::binary);
    char magic[8]{};uint32_t count=0;input.read(magic,8);input.read(reinterpret_cast<char*>(&count),4);
    if(std::memcmp(magic,"RVKSCN1\0",8)||!count||count>16)std::abort();
    a.parts.resize(count);
    for(auto& part:a.parts){uint32_t vertices=0,indices=0;input.read(reinterpret_cast<char*>(&vertices),4);input.read(reinterpret_cast<char*>(&indices),4);
        if(!vertices||vertices>60000||indices>1000000||indices%3)std::abort();
        part.v.resize(vertices);part.ix.resize(indices);input.read(reinterpret_cast<char*>(part.v.data()),vertices*sizeof(VtxMesh));
        input.read(reinterpret_cast<char*>(part.ix.data()),indices*2);if(!input)std::abort();
        for(auto i:part.ix)if(i>=vertices)std::abort();
    }
    return a;
}
Matrix placement(float x,float y,float z,float scale,float angle){
    Matrix m=Identity();const float c=std::cos(angle)*scale,s=std::sin(angle)*scale;
    m.m[0][0]=c;m.m[0][2]=-s;m.m[1][1]=scale;m.m[2][0]=s;m.m[2][2]=c;m.m[3][0]=x;m.m[3][1]=y;m.m[3][2]=z;return m;
}
struct Instance {uint32_t asset;Matrix world;};
}

template<typename D> void RunScannedCliffScene(D& dev,int frames,const std::string& shot){
    using namespace scanpreview;
    static_assert(sizeof(VtxMesh)==36);
    const char* path=std::getenv("AOVK_ROCK_SCAN_ROOT");const std::string root=path?path:"build/preview24/rocks-r48/scans";
    const char* terrainPath=std::getenv("AOVK_ROCK_PREVIEW_ASSETS");const std::string terrain=terrainPath?terrainPath:"rvk/assets/terrain";
    const char* names[]={"rock_face_01","rock_face_02","boulder_01"};std::vector<Asset> assets;
    const uint32_t white=0xFFFFFFFF;
    for(const char* name:names){
        Asset asset=geometry(root,name);maps::MaterialFiles files;const std::string folder=root+"/"+name+"/"+name;
        files.albedo=folder+"_diff_2k.jpg";files.normal=folder+"_nor_gl_2k.jpg";files.packed=folder+"_arm_2k.jpg";
        auto decoded=maps::Decode(files);if(decoded.decodeFailed||decoded.albedo.Empty()||decoded.normal.Empty()||decoded.orm.Empty()){
            std::fprintf(stderr,"scan material %s: %s\n",name,decoded.errors.c_str());std::abort();
        }
        // Keep the photographic variation, with a coherent neutral stone palette.
        // The first scan includes cyan surface marks: neutralize their chroma.
        for(auto& level:decoded.albedo.levels)for(auto& pixel:level){
            const float r=float((pixel>>16)&255),g=float((pixel>>8)&255),b=float(pixel&255),l=.2126f*r+.7152f*g+.0722f*b;
            const float chroma=g>r*1.18f&&b>r*1.18f?.04f:.35f;
            pixel=0xFF000000u|(uint32_t(l+(r-l)*chroma)<<16)|(uint32_t(l+(g-l)*chroma)<<8)|uint32_t(l+(b-l)*chroma);
        }
        asset.texture=dev.CreateTexture(1,1,&white);maps::Attach(dev,asset.texture,decoded);assets.push_back(std::move(asset));
    }
    maps::Image grassImage;std::string error;
    if(!maps::LoadImage(terrain+"/shared-hd-grass.png",&grassImage,&error))std::abort();
    for(size_t i=0;i<grassImage.rgba.size();i+=4){
        const float l=.2126f*grassImage.rgba[i]+.7152f*grassImage.rgba[i+1]+.0722f*grassImage.rgba[i+2];
        for(int c=0;c<3;++c)grassImage.rgba[i+c]=uint8_t((.5f*grassImage.rgba[i+c]+.5f*l)*.65f);
    }
    maps::MipChain chain;maps::MakeAlbedoChain(grassImage,&chain);Texture* grass=maps::Upload(dev,chain);grassImage={};chain={};
    cliffpreview::Part ground;cliffpreview::face(ground,{{-90,-.12f,-90},{-90,-.12f,150},{90,-.12f,150},{90,-.12f,-90}},223,.8f,true);
    std::vector<Instance> instances;
    auto add=[&](uint32_t asset,float x,float y,float z,float scale,float yaw,bool mirror=false){
        Matrix world=placement(x,y,z,scale,yaw);if(mirror)for(int k=0;k<3;++k)world.m[0][k]*=-1;
        instances.push_back({asset,world});
    };
    add(0,0,-1,29,5.5f,kPi-.04f);
    add(0,-13,-.2f,22,2.f,kPi+.16f);add(1,-4,-.18f,21,3.2f,kPi-.17f);
    add(0,5,-.2f,23,1.9f,kPi+.28f,true);add(1,14,-.15f,25,3.3f,kPi-.25f);
    add(1,-11,5.1f,25,3.f,kPi+.32f,true);add(0,-1,4.9f,26,2.2f,kPi-.13f);add(1,10,5.3f,28,3.15f,kPi+.2f,true);
    add(1,-8,13,33,2.5f,kPi+.25f);add(0,10,10,33,2.8f,kPi-.4f,true);
    add(0,-16,-.2f,18,1.1f,kPi-.45f);add(1,12,-.2f,17,1.9f,kPi+.45f);
    add(2,-8,-.18f,4,2.8f,.45f);add(2,9,-.18f,8,3.1f,2.1f);add(2,13,-.18f,1,1.4f,4.5f);
    add(0,-20,-.2f,83,5.2f,kPi+.31f);add(1,5,-.2f,89,7.7f,kPi-.22f);add(0,26,-.2f,99,4.8f,kPi+.5f);
    auto fingerprint=[&](){uint64_t h=1469598103934665603ull;auto append=[&](const void* at,size_t bytes){const auto* p=static_cast<const uint8_t*>(at);for(size_t i=0;i<bytes;++i)h=(h^p[i])*1099511628211ull;};
        for(const auto& a:assets)for(const auto& p:a.parts){append(p.v.data(),p.v.size()*sizeof(VtxMesh));append(p.ix.data(),p.ix.size()*2);}
        for(const auto& i:instances){append(&i.asset,4);append(&i.world,sizeof(i.world));}return h;};
    const uint64_t original=fingerprint();size_t triangles=0;
    for(const auto& i:instances)for(const auto& p:assets[i.asset].parts)triangles+=p.ix.size()/3;
    std::printf("scanned cliff preview: 3 source scans, %zu fixed instances, %zu triangles; geometry %016llx; original UVs; no displacement or distance remeshing\n",instances.size(),triangles,(unsigned long long)original);
    dev.SetHdr(true);dev.SetSkyPlayfield(655);dev.SetPixelLighting(true);dev.SetTaa(true,.15f);
    dev.SetKnob(Knob::EnhancedRocks,0);dev.SetKnob(Knob::RockShape,0);dev.SetBump(0);dev.SetNormalMaps(true,.8f);
    dev.SetBloom(0,1);dev.SetTonemap(1,1);dev.SetDarkness(0,0);dev.SetAo(.6f,.7f);
    Device::PbrSettings pbr;pbr.enabled=true;pbr.specular=.4f;pbr.ambient=.25f;pbr.occlusion=.65f;pbr.probe=0;dev.SetPbr(pbr);
    dev.SetShadows(true);dev.SetShadowParams(.85f,200,3);dev.SetShadowResolution(2048,128);dev.SetPointShadows(0);
    const Material material{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};
    for(int frame=0;frame<frames;++frame){
        if(frame==frames/2)dev.ProfileWindow(true,"");
        if(frame==frames-1){dev.ProfileWindow(false,"scan steady: ");dev.RequestScreenshot(shot);dev.RequestFrameDump("scanned-cliff.txt");}
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFFB2C7D6,1);
        Vector eye{-3,2.4f,-8},target{0,5,23};if(g_visualToggle&1){eye={-9,8,-62};target={0,9,31};}
        if(g_visualToggle&2){eye={-13,2.7f,8};target={-4,5,23};}if(g_visualToggle&4)eye.x+=frame*.02f;
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(View,LookAtLH(eye,target,{0,1,0}));
        dev.SetTransform(Projection,PerspectiveLH(kPi*.32f,float(g_width)/g_height,.1f,450));
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_LIGHTING,1);dev.SetRenderState(RS_NORMALIZENORMALS,1);
        dev.SetRenderState(RS_AMBIENT,0xFF687782);dev.SetRenderState(RS_COLORVERTEX,0);
        dev.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_MATERIAL);dev.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_MATERIAL);
        Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={-.48f,-.75f,.42f};sun.diffuse={1.05f,1.02f,.97f,1};dev.SetLight(0,sun);dev.LightEnable(0,true);dev.SetMaterial(material);
        dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        for(const auto& i:instances){dev.SetTransform(World,i.world);dev.SetTexture(0,assets[i.asset].texture);dev.SetDrawVisual(uint32_t(VisualKind::Static),"scanned-rock-prototype");
            for(const auto& p:assets[i.asset].parts)dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,p.v.data(),uint32_t(p.v.size()),p.ix.data(),uint32_t(p.ix.size()));}
        dev.SetTransform(World,Identity());dev.SetTexture(0,grass);dev.SetDrawVisual(uint32_t(VisualKind::Static),"preview-ground");
        dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,ground.v.data(),uint32_t(ground.v.size()),ground.ix.data(),uint32_t(ground.ix.size()));
        dev.SetDrawVisual(0,nullptr);dev.EndFrame();
    }
    if(fingerprint()!=original)std::abort();std::printf("scanned cliff preview: fixed geometry verified after %d frames\n",frames);
    for(auto& a:assets)dev.DestroyTexture(a.texture);dev.DestroyTexture(grass);
}
