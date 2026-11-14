#include "rock_clusters.h"
// Renderer fixture and production share the same rigid fractured-rock builder.
namespace cliffpreview {
using namespace rvk::rocks::clusters;
Model make(){
    Model m;Random random{734812u};
    // Buried backing volume closes the deep joints without showing empty sky.
    rock(m,{0,4.8f,25.5f},{25.5f,5.f,4.5f},8912,false);
    float x=-24;
    while(x<24){
        const float width=random.range(3.7f,5.8f),middle=x+width*.5f;
        const float top=13.f+2.4f*std::sin(middle*.17f)+random.range(-1.7f,1.7f);
        float y=-.5f;int layer=0;
        while(y<top){
            const float height=std::min(random.range(2.3f,4.4f),top-y+.3f);
            const float depth=random.range(2.6f,4.1f),front=19.4f+1.4f*std::sin(middle*.13f)+random.range(-1.2f,1.2f);
            rock(m,{middle+random.range(-.45f,.45f),y+height*.5f,front+depth},
                {width*random.range(.51f,.58f),height*.52f,depth},hash(uint32_t(m.blocks)+9191),y+height>=top-.2f,random.range(-.13f,.13f));
            y+=height*.94f;++layer;
        }
        x+=width*.94f;
    }
    // Broad shelves / buttresses interrupt the columns and break their rhythm.
    rock(m,{-12,2.6f,16.4f},{6.2f,2.8f,3.9f},21211,true,.12f);
    rock(m,{9.7f,4.1f,17},{4.1f,4.3f,3.2f},98983,false,-.2f);
    rock(m,{15.2f,7.8f,20.3f},{4.6f,2.2f,3.6f},9183,true,.14f);
    rock(m,{-4.8f,9,20},{3.8f,2,2.8f},73733,false,-.1f);
    // Distant formation: independently shaped pieces, with the full silhouette
    // retained. There is no distance deformation or camera-dependent remeshing.
    for(int i=0;i<7;++i){
        const float xx=-45.f+i*11.f,top=25.f+9.f*std::sin(i*.83f)+random.range(-3,3);
        rock(m,{xx,top*.36f,83.f+random.range(-4,7)},{random.range(8,11),top*.64f,random.range(10,15)},hash(i+1009),true,random.range(-.25f,.25f));
    }
    rock(m,{-10,1.6f,4.5f},{4.2f,2.5f,3.2f},41245,true,.3f);
    rock(m,{10.5f,2,8},{3.7f,3.3f,3.4f},54389,false,-.4f);
    rock(m,{14,.8f,1.5f},{2.7f,1.6f,2.1f},99433,false,.6f);
    for(int i=0;i<23;++i){
        const float side=random.next()<.5f?-1.f:1.f;
        const float xx=side*random.range(5,23),zz=random.range(-4,17),r=random.range(.15f,.65f);
        rock(m,{xx,r*.32f,zz},{r*random.range(1,1.8f),r*.7f,r},hash(i+97811),false,random.range(-2,2));
    }
    face(m.ground,{{-90,-.12f,-90},{-90,-.12f,150},{90,-.12f,150},{90,-.12f,-90}},223,.8f,true);
    return m;
}

Model dressedCliff(bool dress){
    Model m;
    // The coarse C-profile is deliberately retained in both views. Additional
    // rigid rock masses are dressing around it, never a camera-distance warp.
    const float heights[]={0,1.6f,5.8f,10,12};
    const float depths[]={17,20.5f,21.5f,20.5f,17};
    auto arc=[](float x){return 1.3f*(1.f-std::cos(x*.06f));};
    for(int section=0;section<6;++section){
        const float x=-24.f+section*8.f,xx=x+8;
        for(int band=0;band<4;++band){
            face(m.stone,{{x,heights[band],depths[band]+arc(x)},
                {x,heights[band+1],depths[band+1]+arc(x)},
                {xx,heights[band+1],depths[band+1]+arc(xx)},
                {xx,heights[band],depths[band]+arc(xx)}},33123+section,.91f);
        }
        face(m.moss,{{x,12,17+arc(x)},{x,12,32},{xx,12,32},{xx,12,17+arc(xx)}},12321,.85f,true);
    }
    face(m.ground,{{-90,-.12f,-90},{-90,-.12f,150},{90,-.12f,150},{90,-.12f,-90}},223,.8f,true);
    if(!dress)return m;
    Random random{873326u};
    const float baseX[]={-22,-14,-4,7,16,23},baseY[]={2.5f,1.5f,3.3f,2.6f,1.6f,3.4f};
    for(int i=0;i<6;++i){
        const float x=baseX[i];
        rock(m,{x+random.range(-.6f,.6f),baseY[i],17+arc(x)},
            {random.range(4.f,5.2f),random.range(3.4f,4.5f),random.range(3.f,4.3f)},hash(13091+i),false,random.range(-.25f,.25f));
    }
    const float middleX[]={-18,-8,3,13,21},middleY[]={6.1f,8.2f,5.6f,8.5f,6.4f};
    for(int i=0;i<5;++i){
        const float x=middleX[i];
        rock(m,{x+random.range(-.8f,.8f),middleY[i],20.5f+arc(x)},
            {random.range(3.3f,4.8f),random.range(2.7f,3.8f),random.range(3.2f,4.1f)},hash(45197+i),false,random.range(-.23f,.23f));
    }
    const float rimX[]={-24,-13,-2,9,21};
    for(int i=0;i<5;++i){
        const float x=rimX[i];
        rock(m,{x,random.range(10.8f,11.8f),18.f+arc(x)},
            {random.range(3.8f,5.2f),random.range(1.4f,2.4f),random.range(2.6f,3.6f)},hash(69857+i),i%3==0,random.range(-.2f,.2f));
    }
    rock(m,{5.4f,10.3f,18.8f},{3.6f,2.6f,3.8f},12213,false,.13f);
    rock(m,{-24,3.5f,18.5f},{4.5f,4.2f,5.1f},8137,false,-.3f);
    rock(m,{24,3.1f,19.5f},{4.2f,3.8f,4.9f},3729,false,.3f);
    rock(m,{-12,1.1f,9.5f},{3.5f,2.1f,3.1f},98331,false,.4f);
    rock(m,{10.5f,.9f,11},{3.2f,1.9f,2.6f},37338,false,-.6f);
    for(int i=0;i<19;++i){const float radius=random.range(.16f,.65f);
        rock(m,{random.range(-24,24),radius*.35f,random.range(11,16)},
            {radius*1.4f,radius*.8f,radius},hash(9543+i),false,random.range(-2,2));
    }
    return m;
}
}

template<typename D> void RunFracturedCliffScene(D& dev,int frames,const std::string& shot){
    using namespace cliffpreview;
    const char* location=std::getenv("AOVK_ROCK_PREVIEW_ASSETS");
    const std::string root=location?std::string(location):"rvk/assets/terrain";
    maps::Image colour,normal,orm;std::string error;
    if(!maps::LoadImage(root+"/rock_face_03_diff_2k.jpg",&colour,&error)||
       !maps::LoadImage(root+"/rock_face_03_nor_gl_2k.jpg",&normal,&error)||
       !maps::LoadImage(root+"/rock_face_03_arm_1k.jpg",&orm,&error)){
        std::fprintf(stderr,"Cliff preview asset: %s\n",error.c_str());return;
    }
    for(size_t i=0;i<colour.rgba.size();i+=4){
        const float l=.2126f*colour.rgba[i]+.7152f*colour.rgba[i+1]+.0722f*colour.rgba[i+2];
        for(int c=0;c<3;++c)colour.rgba[i+c]=uint8_t(std::clamp((.22f*colour.rgba[i+c]+.78f*l)*.98f,0.f,255.f));
    }
    maps::Decoded material;maps::NormalSpread spread;
    maps::MakeAlbedoChain(colour,&material.albedo);colour={};
    maps::MakeNormalChain(normal,&material.normal,&spread);normal={};
    std::vector<float> values(size_t(orm.w)*orm.h*3);
    for(size_t i=0;i<values.size()/3;++i){values[i*3]=orm.rgba[i*4]/255.f;values[i*3+1]=std::max(.78f,orm.rgba[i*4+1]/255.f);values[i*3+2]=0;}
    maps::MakeOrmChain(std::move(values),orm.w,orm.h,spread,&material.orm);orm={};
    const uint32_t white=0xFFFFFFFF;Texture* stone=dev.CreateTexture(1,1,&white);maps::Attach(dev,stone,material);material={};
    maps::Image grassImage;if(!maps::LoadImage(root+"/shared-hd-grass.png",&grassImage,&error)){std::fprintf(stderr,"%s\n",error.c_str());return;}
    for(size_t i=0;i<grassImage.rgba.size();i+=4){
        const float l=.2126f*grassImage.rgba[i]+.7152f*grassImage.rgba[i+1]+.0722f*grassImage.rgba[i+2];
        for(int c=0;c<3;++c)grassImage.rgba[i+c]=uint8_t((.5f*grassImage.rgba[i+c]+.5f*l)*.65f);
    }
    maps::MipChain grassChain;maps::MakeAlbedoChain(grassImage,&grassChain);Texture* grass=maps::Upload(dev,grassChain);grassImage={};grassChain={};
    Model model=(g_visualToggle&8)?dressedCliff(!(g_visualToggle&16)):make();
    if((g_visualToggle&8)&&!(g_visualToggle&16)){
        const Model backing=dressedCliff(false);
        for(const auto pair:{std::pair<const Part*,const Part*>{&backing.stone,&model.stone},{&backing.moss,&model.moss},{&backing.ground,&model.ground}}){
            const auto& a=*pair.first;const auto& b=*pair.second;
            if(a.v.size()>b.v.size()||a.ix.size()>b.ix.size()||std::memcmp(a.v.data(),b.v.data(),a.v.size()*sizeof(VtxMesh))||std::memcmp(a.ix.data(),b.ix.data(),a.ix.size()*2))std::abort();
        }
        std::puts("cliff dressing: original C-profile backing, ground and grass-cap vertices/indices preserved exactly");
    }
    for(auto* part:{&model.stone,&model.moss,&model.ground}){
        if(part->v.size()>65535 || part->ix.size()%3)std::abort();
        for(auto i:part->ix)if(i>=part->v.size())std::abort();
        for(auto vertex:part->v)if(!std::isfinite(vertex.x+vertex.y+vertex.z+vertex.nx+vertex.ny+vertex.nz+vertex.u+vertex.v))std::abort();
    }
    auto geometryHash=[&](){uint64_t h=1469598103934665603ull;for(auto* part:{&model.stone,&model.moss,&model.ground}){
        const auto* data=reinterpret_cast<const uint8_t*>(part->v.data());for(size_t i=0;i<part->v.size()*sizeof(VtxMesh);++i)h=(h^data[i])*1099511628211ull;
    }return h;};
    const uint64_t fixedGeometry=geometryHash();
    std::printf("cliff prototype: %u unique closed fracture blocks, %zu triangles; fixed geometry %016llx; no tessellation, parallax or camera displacement\n",
        model.blocks,(model.stone.ix.size()+model.moss.ix.size()+model.ground.ix.size())/3,(unsigned long long)fixedGeometry);
    dev.SetHdr(true);dev.SetSkyPlayfield(655);dev.SetPixelLighting(true);dev.SetTaa(true,.15f);
    dev.SetKnob(Knob::EnhancedRocks,0);dev.SetKnob(Knob::RockShape,0);dev.SetBump(0);
    dev.SetNormalMaps(true,.3f);dev.SetBloom(0,1);dev.SetTonemap(1,1);dev.SetDarkness(0,0);dev.SetAo(.6f,.5f);
    Device::PbrSettings pbr;pbr.enabled=true;pbr.specular=.4f;pbr.ambient=.25f;pbr.occlusion=.7f;pbr.probe=0;dev.SetPbr(pbr);
    dev.SetShadows(true);dev.SetShadowParams(.85f,180,3);dev.SetShadowResolution(2048,128);dev.SetPointShadows(0);
    const Material mat{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};
    for(int frame=0;frame<frames;++frame){
        if(frame==frames/2)dev.ProfileWindow(true,"");
        if(frame==frames-1){dev.ProfileWindow(false,"cliff steady: ");dev.RequestScreenshot(shot);dev.RequestFrameDump("fractured-cliff.txt");}
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFFB2C7D6,1);
        Vector eye{-3,2.4f,-8},target{1,5.2f,21};
        if(g_visualToggle&1){eye={-9,8,-62};target={0,10,30};}
        if(g_visualToggle&2){eye={-16,3.1f,5};target={-2,6.5f,22};}
        if(g_visualToggle&4)eye.x+=float(frame)*.02f;
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(World,Identity());
        dev.SetTransform(View,LookAtLH(eye,target,{0,1,0}));dev.SetTransform(Projection,PerspectiveLH(kPi*.32f,float(g_width)/g_height,.1f,450));
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_LIGHTING,1);
        dev.SetRenderState(RS_NORMALIZENORMALS,1);dev.SetRenderState(RS_AMBIENT,0xFF687782);
        dev.SetRenderState(RS_COLORVERTEX,1);dev.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_COLOR1);dev.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_COLOR1);
        Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={-.48f,-.75f,.42f};sun.diffuse={1.05f,1.02f,.97f,1};dev.SetLight(0,sun);dev.LightEnable(0,true);
        dev.SetMaterial(mat);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        for(auto* part:{&model.stone,&model.moss,&model.ground}){
            dev.SetTexture(0,part==&model.stone?stone:grass);dev.SetDrawVisual(uint32_t(VisualKind::Static),"fixed-fractured-cliff-prototype");
            dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,part->v.data(),uint32_t(part->v.size()),part->ix.data(),uint32_t(part->ix.size()));
        }
        dev.SetDrawVisual(0,nullptr);dev.EndFrame();
    }
    if(geometryHash()!=fixedGeometry)std::abort();
    std::printf("cliff prototype: geometry hash unchanged after %d frames; silhouette retained at every distance\n",frames);
    dev.DestroyTexture(stone);dev.DestroyTexture(grass);
}
