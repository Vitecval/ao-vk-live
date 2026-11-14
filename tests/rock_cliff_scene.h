// World-scale art fixture; source meshes are simple like AO's cliffs.
template<typename D> void RunRockCliffScene(D& dev,int frames,const std::string& shot)
{
    dev.SetHdr(true);dev.SetSkyPlayfield(655);dev.SetPixelLighting(true);
    dev.SetKnob(Knob::EnhancedRocks,1);dev.SetKnob(Knob::RockShape,(g_visualToggle&1)?1.f:0.f);
    dev.SetBloom(0,1);dev.SetTonemap(1,1);dev.SetDarkness(0,0);
    dev.SetNormalMaps(true,.8f);
    Device::PbrSettings pbr;pbr.enabled=true;pbr.specular=.4f;pbr.ambient=.35f;pbr.occlusion=.5f;dev.SetPbr(pbr);
    Device::TerrainSettings terrain;terrain.rock=false;terrain.cliff=0;dev.SetTerrain(terrain);
    dev.SetShadows(false);
    std::vector<uint32_t> stonePixels(128*128,0xFF747B73u);
    Texture* stone=dev.CreateTexture(128,128,stonePixels.data());dev.SetTextureRdbId(stone,5955,1010004,0);
    auto grassPixels=Checker(128,32,0xFF586C39,0xFF4B6032);
    Texture* grass=dev.CreateTexture(128,128,grassPixels.data());
    auto dirtPixels=Checker(128,32,0xFF807B63,0xFF77725C);
    Texture* dirt=dev.CreateTexture(128,128,dirtPixels.data());
    struct Part{std::vector<VtxMesh> v;std::vector<uint16_t> ix;Texture* texture;};
    std::vector<Part> parts;
    auto quad=[](Part& part,Vector a,Vector b,Vector c,Vector d){
        Vector u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        Vector n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        float l=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);n={n.x/l,n.y/l,n.z/l};
        uint16_t start=uint16_t(part.v.size());
        for(auto p:{a,b,c,d})part.v.push_back({p.x,p.y,p.z,n.x,n.y,n.z,0xFFFFFFFF,p.x*.25f,p.z*.25f});
        part.ix.insert(part.ix.end(),{start,uint16_t(start+1),uint16_t(start+2),uint16_t(start+2),uint16_t(start+1),uint16_t(start+3)});
    };
    const float xs[]={-15,-9,-4,2,7,13,18},depths[]={12,10,11.5f,10.5f,13,16,19},tops[]={12,11.2f,13,10.6f,9.1f,7,5.5f};
    for(int k=0;k<6;++k){
        Part face{{},{},stone};
        for(int band=0;band<3;++band){
            float t0=float(band)/3,t1=float(band+1)/3;
            float z0=(band==0?.5f:band==1?-.25f:0),z1=(band==0?-.25f:band==1?0:.3f);
            quad(face,{xs[k],t0*tops[k],depths[k]+z0},{xs[k],t1*tops[k],depths[k]+z1},
                {xs[k+1],t0*tops[k+1],depths[k+1]+z0},{xs[k+1],t1*tops[k+1],depths[k+1]+z1});
        }
        parts.push_back(std::move(face));
        Part cap{{},{},grass};quad(cap,{xs[k],tops[k],depths[k]+.3f},{xs[k],tops[k],depths[k]+5},
            {xs[k+1],tops[k+1],depths[k+1]+.3f},{xs[k+1],tops[k+1],depths[k+1]+5});parts.push_back(std::move(cap));
    }
    auto boulder=[&](Vector at,Vector radius){
        Part p{{},{},stone};constexpr int sides=7,rings=5;
        for(int r=0;r<=rings;++r)for(int k=0;k<=sides;++k){
            float th=kPi*r/rings,ph=2*kPi*k/sides;
            float x=std::sin(th)*std::cos(ph),y=std::cos(th),z=std::sin(th)*std::sin(ph);
            p.v.push_back({at.x+x*radius.x,at.y+y*radius.y,at.z+z*radius.z,x,y,z,0xFFFFFFFF,float(k)/sides,float(r)/rings});
        }
        for(int r=0;r<rings;++r)for(int k=0;k<sides;++k){
            uint16_t a=uint16_t(r*(sides+1)+k),b=uint16_t(a+sides+1);
            if(r)p.ix.insert(p.ix.end(),{a,b,uint16_t(a+1)});
            if(r<rings-1)p.ix.insert(p.ix.end(),{uint16_t(a+1),b,uint16_t(b+1)});
        }
        parts.push_back(std::move(p));
    };
    boulder({-9,1,6},{3.3f,2.5f,2.4f});boulder({8,1.4f,6},{4,3.8f,3});boulder({12,.3f,1},{2.1f,1.4f,2});
    Part ground{{},{},grass};quad(ground,{-50,-.06f,-30},{-50,-.06f,45},{50,-.06f,-30},{50,-.06f,45});parts.push_back(std::move(ground));
    Part path{{},{},dirt};quad(path,{-4,-.04f,-20},{-2,-.04f,8},{2,-.04f,-20},{3,-.04f,8});parts.push_back(std::move(path));
    const Material material{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};
    for(int frame=0;frame<frames;++frame){
        if(frame==frames-1){dev.RequestScreenshot(shot);dev.RequestFrameDump("cliff-frame.txt");}
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFFBBCBD7,1);
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(World,Identity());
        dev.SetTransform(View,LookAtLH({-4,2.7f,-6},{0,3.5f,9},{0,1,0}));
        dev.SetTransform(Projection,PerspectiveLH(kPi*.31f,float(g_width)/g_height,.1f,180));
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_CULLMODE,CULL_NONE);
        dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_LIGHTING,1);dev.SetRenderState(RS_NORMALIZENORMALS,1);
        dev.SetRenderState(RS_AMBIENT,0xFF606970);dev.SetRenderState(RS_DIFFUSEMATERIALSOURCE,MCS_MATERIAL);
        dev.SetRenderState(RS_AMBIENTMATERIALSOURCE,MCS_MATERIAL);dev.SetRenderState(RS_COLORVERTEX,0);
        Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={-.45f,-.8f,.4f};sun.diffuse={1.1f,1.06f,.98f,1};
        dev.SetLight(0,sun);dev.LightEnable(0,true);
        dev.SetMaterial(material);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);
        dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);
        dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        for(auto& p:parts){
            dev.SetTexture(0,p.texture);dev.SetDrawVisual(uint32_t(VisualKind::Static),"rock-cliff-preview");
            dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,p.v.data(),uint32_t(p.v.size()),p.ix.data(),uint32_t(p.ix.size()));
        }
        dev.SetDrawVisual(0,nullptr);std::this_thread::sleep_for(std::chrono::milliseconds(1));dev.EndFrame();
    }
    dev.DestroyTexture(stone);dev.DestroyTexture(grass);dev.DestroyTexture(dirt);
}
