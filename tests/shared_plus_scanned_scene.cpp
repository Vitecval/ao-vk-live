// Compile this SAME source separately against each version's matching rvk.h/rvk.lib.
// This is a controlled source-data comparison, not an exact retail camera/light replay.
// No NativeGroundOn, tessellated replacement geometry, invented tile art, or live client access.
#include "rvk.h"
#include "material_maps.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rvk;
using namespace rvk::d3d;
namespace fs=std::filesystem;
namespace {
template<class T> T Read(std::ifstream& f) {
    T v{};if(!f.read(reinterpret_cast<char*>(&v),sizeof(v)))throw std::runtime_error("Truncated replay");return v;
}
template<class T> std::vector<T> ReadMany(std::ifstream& f,size_t n) {
    if(n>1000000)throw std::runtime_error("Unbounded replay");
    std::vector<T> v(n);if(n&&!f.read(reinterpret_cast<char*>(v.data()),n*sizeof(T)))throw std::runtime_error("Truncated array");return v;
}
struct LightMap {float fit[8];uint32_t w,h;Texture* texture=nullptr;};
struct Replay {
    uint32_t zone,x,z,lookupCount,typeCount,lightCount,originX,originZ;
    float cellX,cellZ,phaseX,phaseZ,eye[3];
    std::vector<float> heights;std::vector<uint16_t> codes,types;
    std::vector<uint32_t> lookup,selected;std::vector<LightMap> light;
};
Replay Load(const fs::path& path) {
    std::ifstream f(path,std::ios::binary);char magic[8]{};f.read(magic,8);
    if(std::memcmp(magic,"AOSCN1\0\0",8))throw std::runtime_error("Bad replay magic");
    Replay r{};
    r.zone=Read<uint32_t>(f);r.x=Read<uint32_t>(f);r.z=Read<uint32_t>(f);
    r.lookupCount=Read<uint32_t>(f);r.typeCount=Read<uint32_t>(f);r.lightCount=Read<uint32_t>(f);
    r.originX=Read<uint32_t>(f);r.originZ=Read<uint32_t>(f);
    if(r.x!=21||r.z!=21||r.lightCount>1000)throw std::runtime_error("Unexpected replay bounds");
    r.cellX=Read<float>(f);r.cellZ=Read<float>(f);r.phaseX=Read<float>(f);r.phaseZ=Read<float>(f);
    for(auto& v:r.eye)v=Read<float>(f);
    r.heights=ReadMany<float>(f,(r.x+1)*(r.z+1));r.codes=ReadMany<uint16_t>(f,r.x*r.z);
    r.lookup=ReadMany<uint32_t>(f,r.lookupCount);r.types=ReadMany<uint16_t>(f,r.typeCount);
    r.light.resize(r.lightCount);
    for(auto& m:r.light){for(auto& v:m.fit)v=Read<float>(f);m.w=Read<uint32_t>(f);m.h=Read<uint32_t>(f);}
    r.selected=ReadMany<uint32_t>(f,r.x*r.z);
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Unexpected replay trailing bytes");
    for(auto c:r.codes)if((c&0x3fff)>=r.lookup.size())throw std::runtime_error("Bad tile index");
    for(auto i:r.selected)if(i!=0xffffffff&&i>=r.lightCount)throw std::runtime_error("Bad map index");
    return r;
}
// Explicit fixture-owned mip generation, identical for both linked versions.
// Four clamped samples averaged as bytes; no resizing of the native top level.
Texture* Upload(Device& dev,const fs::path& path) {
    maps::Image image;std::string error;
    if(!maps::LoadImage(path.string(),&image,&error))throw std::runtime_error(error);
    uint32_t w=image.w,h=image.h,levels=1;for(auto s=std::max(w,h);s>1;s>>=1)++levels;
    auto* texture=dev.CreateTexture(w,h,Format::A8R8G8B8,levels);
    if(!texture)throw std::runtime_error("Texture allocation failed");
    std::vector<uint32_t> pixels(size_t(w)*h);
    for(size_t i=0;i<pixels.size();++i){auto* c=&image.rgba[i*4];pixels[i]=uint32_t(c[3])<<24|uint32_t(c[0])<<16|uint32_t(c[1])<<8|c[2];}
    for(uint32_t level=0;level<levels;++level){
        dev.UpdateTexture(texture,level,0,0,w,h,pixels.data(),w*4);
        if(level+1==levels)break;
        uint32_t nw=std::max(w/2,1u),nh=std::max(h/2,1u);std::vector<uint32_t> next(size_t(nw)*nh);
        for(uint32_t y=0;y<nh;++y)for(uint32_t x=0;x<nw;++x){
            uint32_t p=0;for(uint32_t shift=0;shift<32;shift+=8){uint32_t sum=0;
                for(uint32_t dy=0;dy<2;++dy)for(uint32_t dx=0;dx<2;++dx)
                    sum+=(pixels[size_t(std::min(2*y+dy,h-1))*w+std::min(2*x+dx,w-1)]>>shift)&255;
                p|=((sum+2)/4)<<shift;}next[size_t(y)*nw+x]=p;
        }pixels=std::move(next);w=nw;h=nh;
    }return texture;
}
Matrix Identity(){Matrix m{};for(int i=0;i<4;++i)m.m[i][i]=1;return m;}
Matrix TopView(float x,float y,float z) {
    // Downward gaze, +z at the top, D3D row-vector left-handed view.
    Matrix m{};m.m[0][0]=1;m.m[2][1]=1;m.m[1][2]=-1;
    m.m[3][0]=-x;m.m[3][1]=-z;m.m[3][2]=y;m.m[3][3]=1;return m;
}
Matrix Perspective(float aspect) {Matrix m{};float s=1/std::tan(3.14159265f/6);
    m.m[0][0]=s/aspect;m.m[1][1]=s;m.m[2][2]=300.f/299.9f;m.m[2][3]=1;m.m[3][2]=-.1f*300.f/299.9f;return m;}
Matrix Orthographic(float width,float height,float farPlane) {Matrix m{};
    m.m[0][0]=2/width;m.m[1][1]=2/height;m.m[2][2]=1/(farPlane-.1f);m.m[3][2]=-.1f/(farPlane-.1f);m.m[3][3]=1;return m;}
struct Vertex {float x,y,z,nx,ny,nz,u,v,lu,lv;};
struct CasterVertex {float x,y,z,nx,ny,nz,u,v;};
struct Cell {std::array<Vertex,6> vertices;Texture* tile;Texture* light;};
std::vector<CasterVertex> CasterCube(float x,float y,float z) {
    struct Face {Vector n,u,v;};const Face faces[]={
        {{0,0,-1},{1,0,0},{0,1,0}},{{0,0,1},{-1,0,0},{0,1,0}},
        {{1,0,0},{0,0,1},{0,1,0}},{{-1,0,0},{0,0,-1},{0,1,0}},
        {{0,1,0},{1,0,0},{0,0,1}},{{0,-1,0},{1,0,0},{0,0,-1}}};
    std::vector<CasterVertex> vertices;vertices.reserve(36);
    for(const auto& f:faces){std::array<CasterVertex,4> q{};
        for(unsigned k=0;k<4;++k){float u=(k==1||k==2)?1.f:-1.f,v=k>=2?-1.f:1.f;
            q[k]={x+1.5f*(f.n.x+u*f.u.x+v*f.v.x),y+1.5f*(f.n.y+u*f.u.y+v*f.v.y),z+1.5f*(f.n.z+u*f.u.z+v*f.v.z),f.n.x,f.n.y,f.n.z,(u+1)/2,(1-v)/2};}
        for(unsigned i:{0u,1u,2u,0u,2u,3u})vertices.push_back(q[i]);
    }return vertices;
}
}
int main(int argc,char** argv) {
    try {
        fs::path scan,output="scanned-ground.bmp",paints;int frames=80,hdGround=0,diagonal=0,debug=0;
        bool flat=false,normalsUp=false,orthographic=false,lighting=false,shadows=false,caster=false,ambientOnly=false;
        float ambient=-1,sunIntensity=1,areaLight=0;Vector sunDirection{.55f,-.7f,.45f};
        bool ambientRGBSet=false;std::array<float,3> ambientRGB{};
        for(int i=1;i<argc;++i){std::string a=argv[i];
            if(a=="--scan"&&i+1<argc)scan=argv[++i];
            else if(a=="--output"&&i+1<argc)output=argv[++i];
            else if(a=="--paints"&&i+1<argc)paints=argv[++i];
            else if(a=="--frames"&&i+1<argc)frames=std::stoi(argv[++i]);
            else if(a=="--hd-ground"&&i+1<argc){std::string value=argv[++i];if(value!="0"&&value!="1")throw std::runtime_error("--hd-ground must be 0 or 1");hdGround=value=="1"?1:0;}
            else if(a=="--normals-up")normalsUp=true;
            else if(a=="--diagonal"&&i+1<argc){std::string value=argv[++i];if(value!="0"&&value!="1")throw std::runtime_error("--diagonal must be 0 or 1");diagonal=value=="1"?1:0;}
            else if(a=="--orthographic")orthographic=true;
            else if(a=="--debug"&&i+1<argc){std::string value=argv[++i];if(value!="0"&&value!="20"&&value!="21"&&value!="22"&&value!="25"&&value!="26")throw std::runtime_error("--debug must be 0,20,21,22,25 or 26");debug=std::stoi(value);}
            else if(a=="--lighting")lighting=true;
            else if(a=="--ambient-only"){ambientOnly=true;lighting=true;}
            else if(a=="--area-light"&&i+1<argc){areaLight=std::stof(argv[++i]);if(!std::isfinite(areaLight)||areaLight<0||areaLight>1)throw std::runtime_error("--area-light must be 0..1");}
            else if(a=="--shadows"){shadows=true;lighting=true;caster=true;}
            else if(a=="--caster")caster=true;
            else if(a=="--ambient"&&i+1<argc){ambient=std::stof(argv[++i]);if(!std::isfinite(ambient)||ambient<0||ambient>1)throw std::runtime_error("--ambient must be 0..1");}
            else if(a=="--ambient-rgb"&&i+3<argc){for(auto& value:ambientRGB){value=std::stof(argv[++i]);if(!std::isfinite(value)||value<0||value>1)throw std::runtime_error("--ambient-rgb channels must be 0..1");}ambientRGBSet=true;}
            else if(a=="--sun-intensity"&&i+1<argc){sunIntensity=std::stof(argv[++i]);if(!std::isfinite(sunIntensity)||sunIntensity<0||sunIntensity>4)throw std::runtime_error("--sun-intensity must be 0..4");}
            else if(a=="--sun-dir"&&i+3<argc){sunDirection={std::stof(argv[++i]),std::stof(argv[++i]),std::stof(argv[++i])};
                float length=std::sqrt(sunDirection.x*sunDirection.x+sunDirection.y*sunDirection.y+sunDirection.z*sunDirection.z);
                if(!std::isfinite(length)||length<1e-6f)throw std::runtime_error("--sun-dir must be a finite nonzero vector");sunDirection.x/=length;sunDirection.y/=length;sunDirection.z/=length;}
            else if(a=="--albedo-only")flat=true;
            else throw std::runtime_error("Unknown/incomplete argument: "+a);
        }
        if(scan.empty()||frames<2||frames>1000)throw std::runtime_error("Use --scan <scan-directory> [--output out.bmp] [--albedo-only] [--paints pack-directory] [--frames 80] [--hd-ground 0|1] [--normals-up] [--diagonal 0|1] [--orthographic] [--debug 0|20|21|22|25|26] [--lighting] [--shadows] [--caster] [--ambient 0..1] [--ambient-rgb r g b] [--sun-intensity 0..4] [--sun-dir x y z]");
        auto r=Load(scan/"scene.replay.bin");Device dev;std::string error;
        if(!dev.Init(nullptr,960,720,&error))throw std::runtime_error(error);
        Device::SetFixedClock(1.0/60);dev.SetHdr(true);dev.SetTonemap(1,1);dev.SetBloom(0,1);dev.SetTaa(false,0);
        dev.SetPixelLighting(lighting||debug!=0);dev.SetLightOverride(false);dev.SetShadows(shadows);dev.SetPointShadows(0);
        if(lighting){dev.SetGameSun(true);dev.SetDarkness(0,0);dev.SetSkyTime(1000);}
        if(shadows)dev.SetShadowParams(1,220,4);
        dev.SetKnob(Knob::TerraBlend,2);dev.SetKnob(Knob::HDGround,float(hdGround));dev.SetKnob(Knob::ImprovedTiles,0);
        if(areaLight>0){dev.SetKnob(Knob::AreaLight,areaLight);dev.SetKnob(Knob::DayCycle,2);}
        dev.SetNativeGroundOn(false);dev.SetNativeGroundAo(0);dev.SetNativeGroundSun(0);
        dev.SetNativeGroundRocks(0);dev.SetNativeGroundPebbles(0);dev.SetSkyPlayfield(r.zone);
        Device::TerrainSettings settings;settings.round=0;settings.depth=0;
        settings.ripple=settings.dune=settings.glint=settings.prints=0;dev.SetTerrain(settings);
        Device::PbrSettings pbr;pbr.enabled=true;pbr.debug=uint32_t(debug);dev.SetPbr(pbr);
        auto ground=std::make_shared<NativeGround>();ground->id=r.zone;ground->cellsX=r.x;ground->cellsZ=r.z;
        ground->patch=21;ground->cellX=r.cellX;ground->cellZ=r.cellZ;ground->heights=r.heights;
        ground->codes=r.codes;ground->types=r.types;ground->tiles.resize(r.lookup.size());
        std::set<uint16_t> used;for(auto code:r.codes)used.insert(code&0x3fff);
        uint32_t paintCount=0;
        for(auto index:used){auto id=r.lookup[index];auto* tile=Upload(dev,scan.parent_path()/("1010006_"+std::to_string(id)+"-native.png"));
            dev.SetTextureRdbId(tile,id,1010006);ground->tiles[index]=tile;
            auto path=paints/("1010006_"+std::to_string(id)+"_d.png");
            if(!paints.empty()&&fs::is_regular_file(path)){dev.SetMaterialMaps(tile,nullptr,Upload(dev,path));++paintCount;}
        }
        dev.SetNativeGround(ground);dev.SetNativeGroundOn(false);
        uint32_t whitePixel=0xffffffff;auto* white=dev.CreateTexture(1,1,&whitePixel);
        for(uint32_t i=0;i<r.lightCount;++i)r.light[i].texture=Upload(dev,scan/("captured-light-"+std::to_string(i)+".png"));
        std::vector<Cell> cells(r.x*r.z);size_t uncovered=0;
        auto height=[&](int x,int z){return r.heights[size_t(std::clamp(z,0,int(r.z)))*(r.x+1)+std::clamp(x,0,int(r.x))];};
        for(uint32_t z=0;z<r.z;++z)for(uint32_t x=0;x<r.x;++x){
            auto n=size_t(z)*r.x+x;auto code=r.codes[n];auto& c=cells[n];c.tile=ground->tiles[code&0x3fff];
            const LightMap* lm=r.selected[n]==0xffffffff?nullptr:&r.light[r.selected[n]];
            c.light=flat||!lm?white:lm->texture;if(!lm)++uncovered;
            auto vertex=[&](uint32_t a,uint32_t b){float px=(x+a)*r.cellX,pz=(z+b)*r.cellZ,py=height(x+a,z+b);
                float nx=(height(int(x+a)-1,z+b)-height(x+a+1,z+b))/(2*r.cellX);
                float nz=(height(x+a,int(z+b)-1)-height(x+a,z+b+1))/(2*r.cellZ);float len=std::sqrt(nx*nx+1+nz*nz);
                const float u[4]={float(a),1.f-b,1.f-a,float(b)},v[4]={1.f-b,1.f-a,float(b),float(a)};
                float lu=.5f,lv=.5f;if(lm){auto f=lm->fit;float wx=px+r.phaseX,wz=pz+r.phaseZ;
                    lu=wx*f[0]+py*f[1]+wz*f[2]+f[3];lv=wx*f[4]+py*f[5]+wz*f[6]+f[7];}
                return Vertex{px,py,pz,normalsUp?0.f:nx/len,normalsUp?1.f:1/len,normalsUp?0.f:nz/len,u[code>>14],v[code>>14],lu,lv};};
            if(diagonal)c.vertices={vertex(0,0),vertex(0,1),vertex(1,1),vertex(0,0),vertex(1,1),vertex(1,0)};
            else c.vertices={vertex(0,0),vertex(0,1),vertex(1,0),vertex(1,0),vertex(0,1),vertex(1,1)};
        }
        float cx=r.x*r.cellX/2,cz=r.z*r.cellZ/2,top=*std::max_element(r.heights.begin(),r.heights.end())+85;
        const float casterHeight=(height(r.x/2,r.z/2)+height(r.x/2+1,r.z/2)+height(r.x/2,r.z/2+1)+height(r.x/2+1,r.z/2+1))*.25f+6.f;
        auto cube=caster?CasterCube(cx,casterHeight,cz):std::vector<CasterVertex>{};
        const float orthoHeight=std::max(r.z*r.cellZ*1.05f,r.x*r.cellX*1.05f/(960.f/720));
        const float farPlane=std::max(300.f,top-*std::min_element(r.heights.begin(),r.heights.end())+32);
        const float ambientValue=ambient>=0?ambient:lighting?.25f:1.f;
        const uint32_t ambientByte=uint32_t(std::lround(ambientValue*255));
        uint32_t ambientARGB=0xff000000|ambientByte<<16|ambientByte<<8|ambientByte;
        if(ambientRGBSet){ambientARGB=0xff000000;for(unsigned channel=0;channel<3;++channel)ambientARGB|=uint32_t(std::lround(ambientRGB[channel]*255))<<(16-8*channel);}
        std::printf("zone=%u cells=%ux%u tiles=%zu maps=%u attached-paints=%u missing-map-cells=%zu\n",r.zone,r.x,r.z,used.size(),r.lightCount,paintCount,uncovered);
        std::printf("SharedTerrain=2 HDGround=%d NativeGroundOn=0; local origin rebased from %.1f,%.1f; procedural world phase differs from retail\n",hdGround,r.phaseX,r.phaseZ);
        std::printf("source eye %.2f,%.2f,%.2f; CONTROLLED top-down eye %.2f,%.2f,%.2f; %s normals/full-cell triangles, alpha255, no retail shadow claim\n",r.eye[0],r.eye[1],r.eye[2],cx,top,cz,normalsUp?"AO-up":"derived");
        std::printf("fixture diagnostic: diagonal=%d orthographic=%u debug=%d lighting=%u shadows=%u caster=%u caster-centre=%.3f,%.3f,%.3f ambient=%.3f sun-intensity=%.3f; original AO grid/corner heights retained\n",diagonal,unsigned(orthographic),debug,unsigned(lighting),unsigned(shadows),unsigned(caster),cx,casterHeight,cz,ambientValue,sunIntensity);
        std::printf("fixture light controls: RS_AMBIENT=%08X custom-rgb=%u area-light=%.3f ambient-only=%u\n",ambientARGB,unsigned(ambientRGBSet),areaLight,unsigned(ambientOnly));
        for(int frame=0;frame<frames;++frame){
            if(frame==frames-1){dev.RequestScreenshot(output.string());dev.RequestFrameDump(output.string()+".txt");}
            dev.BeginFrame();dev.SetViewport({0,0,960,720,0,1});dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff202020,1);
            dev.SetTransform(World,Identity());dev.SetTransform(View,TopView(cx,top,cz));dev.SetTransform(Projection,orthographic?Orthographic(orthoHeight*(960.f/720),orthoHeight,farPlane):Perspective(960.f/720));
            dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_FOGENABLE,0);
            dev.SetRenderState(RS_ALPHATESTENABLE,0);dev.SetRenderState(RS_SPECULARENABLE,0);dev.SetRenderState(RS_AMBIENT,ambientARGB);
            if(lighting){Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction=sunDirection;sun.diffuse={.9f*sunIntensity,.85f*sunIntensity,.75f*sunIntensity,1};dev.SetLight(0,sun);dev.LightEnable(0,true);}
            const Material material{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};dev.SetMaterial(material);
            dev.SetTexture(1,nullptr);dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
            dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLOROP,TOP_SELECTARG1);
            dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);
            dev.SetTextureStageState(0,TSS_ADDRESSU,TADDRESS_CLAMP);dev.SetTextureStageState(0,TSS_ADDRESSV,TADDRESS_CLAMP);
            dev.SetTextureStageState(0,TSS_MINFILTER,TFN_LINEAR);dev.SetTextureStageState(0,TSS_MAGFILTER,TFG_LINEAR);
            dev.SetTextureStageState(0,TSS_MIPFILTER,TFP_LINEAR);
            dev.SetDrawVisual(uint32_t(VisualKind::Terrain),"AnarchyGround_t");
            // Every stable-pointer albedo draw precedes every corresponding lightmap draw.
            dev.SetRenderState(RS_LIGHTING,0);dev.SetRenderState(RS_ZWRITEENABLE,1);
            dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
            dev.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);
            for(auto& c:cells){dev.SetTexture(0,c.tile);dev.DrawPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(2<<8),c.vertices.data(),6);}
            if(ambientOnly){Material lightMaterial=material;lightMaterial.diffuse={0,0,0,1};dev.SetMaterial(lightMaterial);}
            dev.SetRenderState(RS_LIGHTING,1);dev.SetRenderState(RS_ZWRITEENABLE,0);
            dev.SetRenderState(RS_ALPHABLENDENABLE,1);dev.SetRenderState(RS_SRCBLEND,BLEND_ZERO);
            dev.SetRenderState(RS_DESTBLEND,BLEND_SRCCOLOR);dev.SetRenderState(RS_ZFUNC,CMP_EQUAL);
            dev.SetTextureStageState(0,TSS_TEXCOORDINDEX,1);
            if(lighting){dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);}
            for(auto& c:cells){dev.SetTexture(0,c.light);dev.DrawPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(2<<8),c.vertices.data(),6);}
            if(caster){dev.SetRenderState(RS_ALPHABLENDENABLE,0);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
                dev.SetRenderState(RS_LIGHTING,lighting?1:0);dev.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);
                dev.SetTextureStageState(0,TSS_COLOROP,lighting?TOP_MODULATE:TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
                dev.SetMaterial(material);dev.SetTexture(0,white);dev.SetDrawVisual(uint32_t(VisualKind::Static),"scanned-fixture-caster");
                dev.DrawPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(1<<8),cube.data(),uint32_t(cube.size()));}
            dev.SetDrawVisual(0,nullptr);dev.EndFrame();Sleep(5);
        }
        dev.SetTexture(0,nullptr);dev.SetTexture(1,nullptr);dev.SetNativeGround(nullptr);
        // NativeGround and cells borrow these fixture-owned textures. A tile
        // owns its attached painting, so destroying the tile releases that too.
        std::set<Texture*> owned{white};
        for(auto* texture:ground->tiles)if(texture)owned.insert(texture);
        for(auto& map:r.light)if(map.texture)owned.insert(map.texture);
        for(auto* texture:owned)dev.DestroyTexture(texture);
        return fs::is_regular_file(output)?0:3;
    }catch(const std::exception& e){std::fprintf(stderr,"scanned scene: %s\n",e.what());return 2;}
}
