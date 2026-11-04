#include "living.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sstream>

namespace rvk::living {
namespace {
std::map<std::string,std::string> fixtureResources;
bool FixturePath(const std::filesystem::path& path){return path.string().starts_with(":fixture:");}
std::unique_ptr<std::istream> Open(const std::filesystem::path& path,bool fixture=false){
    if(fixture&&FixturePath(path)){auto it=fixtureResources.find(path.filename().string());if(it==fixtureResources.end())throw std::runtime_error("missing RAM fixture resource");return std::make_unique<std::istringstream>(it->second,std::ios::binary);}
    return std::make_unique<std::ifstream>(path,std::ios::binary);
}
size_t Size(const std::filesystem::path& path,bool fixture=false){
    if(fixture&&FixturePath(path)){auto it=fixtureResources.find(path.filename().string());if(it==fixtureResources.end())throw std::runtime_error("missing RAM fixture resource");return it->second.size();}
    return std::filesystem::file_size(path);
}
Point Spline(const std::vector<Point>& p,float u,bool smoothFlight=false) {
    const int n=int(p.size()),i=int(u)%n;float t=u-std::floor(u),t2=t*t,t3=t2*t;
    const Point a=p[(i+n-1)%n],b=p[i],c=p[(i+1)%n],d=p[(i+2)%n];
    auto f=[&](float a,float b,float c,float d){return .5f*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t2+(-a+3*b-3*c+d)*t3);};
    float y=f(a.y,b.y,c.y,d.y);
    if(smoothFlight){auto slope=[](float a,float b,float c){float x=b-a,y=c-b;return x*y<=0?0.f:std::copysign(std::min({std::fabs((x+y)/2),3*std::fabs(x),3*std::fabs(y)}),x);};
        const float m=slope(a.y,b.y,c.y),n=slope(b.y,c.y,d.y);y=(2*t3-3*t2+1)*b.y+(t3-2*t2+t)*m+(-2*t3+3*t2)*c.y+(t3-t2)*n;}
    return {f(a.x,b.x,c.x,d.x),y,f(a.z,b.z,c.z,d.z)};
}
float Distance(Point a,Point b) {return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
void Require(bool ok,const char* msg) {if(!ok)throw std::runtime_error(msg);}
template<class T> void Read(std::istream& f,T* out,size_t count) {
    f.read(reinterpret_cast<char*>(out),std::streamsize(sizeof(T)*count));Require(bool(f),"truncated mesh");
}
bool Finite(float f,float limit=100000) {return std::isfinite(f)&&std::fabs(f)<=limit;}
void LoadMesh(const std::filesystem::path& path,Mesh& m,size_t& bytes,bool fixture) {
    const auto sz=Size(path,fixture);Require(sz<=32u*1024u*1024u&&bytes+sz<=MaxBytes+32u*1024u*1024u,"ambient source/staging budget exceeded");bytes+=size_t(sz);
    auto stream=Open(path,fixture);auto& f=*stream;uint32_t h[10];Read(f,h,10);
    const bool activities=h[0]==0x344d524cu,clips=h[0]==0x334d524cu||activities;
    Require(h[0]==0x324d524cu||clips,"mesh must be LRM2 or LRM3");
    m.vertexCount=h[1];m.dimension=h[5];m.bones=h[6];m.frameCount=h[7];std::memcpy(&m.fps,h+8,4);
    Require((clips||h[9]==0)&&m.vertexCount>=3&&m.vertexCount<=20000,"invalid vertex count or reserved field");
    Require(m.dimension>=4&&m.dimension<=1024&&(m.dimension&(m.dimension-1))==0,"texture must be a power of two, <=1024");
    Require(m.bones<=64&&m.frameCount<=(activities?256u:120u),"rig budget exceeded");
    Require((m.bones&&m.frameCount>=2&&m.fps>=1&&m.fps<=60)||(!m.bones&&!m.frameCount&&m.fps==0),"incomplete animation rig");
    const size_t stride=m.bones?sizeof(skin::TriVertex):sizeof(skin::Vertex);
    size_t clipBytes=0;std::vector<uint32_t> contactVertices;
    if(clips) {
        float timing[3];uint32_t count;Read(f,timing,3);Read(f,&count,1);
        m.walkFrames=h[9];m.walkSeconds=timing[0];m.idleSeconds=timing[1];m.stride=timing[2];
        Require(m.bones&&m.walkFrames>=2&&m.walkFrames+2<=m.frameCount&&count==m.walkFrames+1,"invalid animation clip partition");
        Require(Finite(m.walkSeconds,60)&&m.walkSeconds>.1f&&Finite(m.idleSeconds,120)&&m.idleSeconds>.1f&&Finite(m.stride,20)&&m.stride>.01f,"invalid animation timing/stride");
        m.gaitDistance.resize(count);Read(f,m.gaitDistance.data(),count);
        Require(m.gaitDistance[0]==0&&std::fabs(m.gaitDistance.back()-m.stride)<.001f,"invalid gait distance endpoints");
        for(size_t i=1;i<count;++i)Require(Finite(m.gaitDistance[i],20)&&m.gaitDistance[i]>m.gaitDistance[i-1],"gait phase distance must increase");
        clipBytes=16+count*4;
        m.idleFrames=m.frameCount-m.walkFrames;
        if(activities){uint32_t n;Read(f,&m.idleFrames,1);Read(f,&n,1);Require(n>=1&&n<=6&&m.idleFrames>=2&&m.walkFrames+m.idleFrames<m.frameCount,"invalid human activity partition");
            uint32_t first=m.walkFrames+m.idleFrames;uint32_t seen=0;
            for(uint32_t i=0;i<n;++i){Mesh::Clip c;Read(f,&c.kind,1);Read(f,&c.first,1);Read(f,&c.count,1);Read(f,&c.seconds,1);
                Require(c.kind<6&&!(seen&(1u<<c.kind))&&c.first==first&&c.count>=2&&c.count<=48&&Finite(c.seconds,15)&&c.seconds>.2f&&first+c.count<=m.frameCount,"invalid human activity clip");
                seen|=1u<<c.kind;first+=c.count;m.activities.push_back(c);}
            Require(first==m.frameCount,"unclaimed activity frames");clipBytes+=8+n*16;
            uint32_t contacts;Read(f,&contacts,1);Require(contacts>0&&contacts<=96,"invalid human contact budget");contactVertices.resize(contacts);Read(f,contactVertices.data(),contacts);
            for(auto index:contactVertices)Require(index<m.vertexCount,"invalid human contact vertex");clipBytes+=4+contacts*4;}
    }
    const size_t expected=40+clipBytes+stride*m.vertexCount+2ull*(h[2]+h[3]+h[4])+sizeof(skin::Bone)*m.bones*m.frameCount+16ull*m.dimension*m.dimension;
    Require(expected==sz,"mesh size mismatch");
    if(m.bones) {
        m.skin=std::make_shared<skin::Source>();m.skin->vertices.resize(m.vertexCount);Read(f,m.skin->vertices.data(),m.vertexCount);
        for(const auto& v:m.skin->vertices) {
            Require(v.boneA>=0&&uint32_t(v.boneA)<m.bones&&v.boneB>=0&&uint32_t(v.boneB)<m.bones&&v.weightA>=0&&v.weightA<=1,"invalid bone weights");
            const float* p=reinterpret_cast<const float*>(&v);for(int i=0;i<14;++i)Require(Finite(p[i]),"non-finite skin vertex");
            m.radius=std::max(m.radius,Distance({}, {v.bind[0],v.bind[1],v.bind[2]}));
        }
    } else {
        auto v=std::make_shared<std::vector<uint8_t>>(stride*m.vertexCount);Read(f,v->data(),v->size());
        for(uint32_t i=0;i<m.vertexCount;++i) {
            skin::Vertex p;std::memcpy(&p,v->data()+i*stride,stride);
            for(float x:p.pos)Require(Finite(x),"non-finite vertex");
            for(float x:p.normal)Require(Finite(x,2),"invalid normal");
            const float nl=p.normal[0]*p.normal[0]+p.normal[1]*p.normal[1]+p.normal[2]*p.normal[2];
            Require(nl>.8f&&nl<1.2f&&Finite(p.uv[0],100)&&Finite(p.uv[1],100),"normal or UV invalid");
            m.radius=std::max(m.radius,Distance({}, {p.pos[0],p.pos[1],p.pos[2]}));
        }
        m.vertices=std::move(v);
    }
    for(int l=0;l<3;++l) {
        Require(h[2+l]>=3&&h[2+l]<=60000&&h[2+l]%3==0,"invalid LOD index count");
        auto idx=std::make_shared<std::vector<uint16_t>>(h[2+l]);Read(f,idx->data(),idx->size());
        for(auto x:*idx)Require(x<m.vertexCount,"index out of range");m.indices[l]=idx;
        if(!m.bones){auto bytes=std::make_shared<std::vector<uint8_t>>(idx->size()*2);std::memcpy(bytes->data(),idx->data(),bytes->size());m.indexBytes[l]=bytes;}
    }
    if(m.bones) {
        for(int l=1;l<3;++l)Require(m.indices[l]->size()<=m.indices[0]->size()&&std::equal(m.indices[l]->begin(),m.indices[l]->end(),m.indices[0]->begin()),"rigged LODs must be prefixes of LOD0");
        m.frames.resize(m.bones*m.frameCount);Read(f,m.frames.data(),m.frames.size());
        for(auto& b:m.frames)for(float x:b.m)Require(Finite(x),"invalid animation palette");
        m.skin->indices=*m.indices[0];m.skin->Finish();
        m.poseScratch.resize(m.bones);bytes+=m.bones*sizeof(skin::Bone);
        float floor=1e9f;for(const auto& v:m.skin->vertices)floor=std::min(floor,v.bind[1]);
        if(activities){for(auto index:contactVertices)m.contacts.push_back(m.skin->vertices[index]);}
        else for(const auto& v:m.skin->vertices)if(v.bind[1]<=floor+.16f&&m.contacts.size()<96)m.contacts.push_back(v);
        bytes+=m.contacts.size()*sizeof(skin::TriVertex);

        // Animation bounds are conservative across every authored keyframe.
        for(const auto& b:m.frames)m.radius=std::max(m.radius,Distance({}, {b.m[9],b.m[10],b.m[11]})+2.f);
    }
    for(size_t j=0;j<4;++j){
        auto& p=m.pixels[j];p.resize(size_t(m.dimension)*m.dimension);Read(f,p.data(),p.size());m.mapDimensions[j]=m.dimension;
        // A constant map needs one texel, including its GPU mip chain. Preserve albedo resolution/alpha.
        if(j&&std::all_of(p.begin(),p.end(),[&](uint32_t x){return x==p[0];})){
            bytes-=(p.size()-1)*4;const auto colour=p[0];p.assign(1,colour);p.shrink_to_fit();m.mapDimensions[j]=1;
        }
    }
    for(auto p:m.pixels[0])if((p>>24)<240){m.cutout=true;break;}
}
}
bool ReadFixtureResources(std::istream& input,std::string& error){
    try{uint32_t magic,count;Read(input,&magic,1);Read(input,&count,1);Require(magic==0x314d5246&&count<=128,"invalid RAM fixture bundle");
        std::map<std::string,std::string> resources;size_t total=0;
        for(uint32_t i=0;i<count;++i){uint32_t n,size;Read(input,&n,1);Require(n>0&&n<64,"invalid RAM fixture name");std::string name(n,'\0');Read(input,name.data(),n);
            Require(name.find("..") == std::string::npos&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos,"unsafe RAM fixture name");
            Read(input,&size,1);total+=size;Require(size<=256u*1024u*1024u&&total<=768u*1024u*1024u,"RAM fixture budget exceeded");
            std::string bytes(size,'\0');Read(input,bytes.data(),size);Require(resources.emplace(name,std::move(bytes)).second,"duplicate RAM fixture resource");}
        fixtureResources=std::move(resources);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool FixtureResource(const std::string& name,std::string& bytes){auto it=fixtureResources.find(name);if(it==fixtureResources.end())return false;bytes=it->second;return true;}
bool Route::Finish() {
    arc.clear();length=0;if(points.empty()||points.size()>32)return false;
    for(auto p:points)if(!Finite(p.x)||!Finite(p.y)||!Finite(p.z))return false;
    if(points.size()==1)return true;
    if(linear){if(points.size()!=2)return false;length=Distance(points[0],points[1]);arc={0,length};return length>.01f&&Finite(length,1000000);}
    if(points.size()<4)return false;
    arc.reserve(points.size()*128+1);arc.push_back(0);Point a=points[0];
    for(size_t i=1;i<=points.size()*128;++i){Point b=Spline(points,float(i)/128,smoothFlight);length+=Distance(a,b);arc.push_back(length);a=b;}
    return length>.01f&&Finite(length,1000000);
}
Point Route::At(float d,Point* tangent) const {
    if(points.size()==1){if(tangent)*tangent={0,0,1};return points[0];}
    if(arc.empty()||!std::isfinite(d)){if(tangent)*tangent={0,0,1};return {};}
    if(linear){const auto a=points[0],b=points[1];const float t=std::clamp(d/length,0.f,1.f);if(tangent)*tangent={b.x-a.x,b.y-a.y,b.z-a.z};return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
    d=std::fmod(d,length);if(d<0)d+=length;
    auto it=std::upper_bound(arc.begin(),arc.end(),d);size_t i=std::clamp<size_t>(size_t(it-arc.begin()),1,arc.size()-1);
    float den=arc[i]-arc[i-1],u=(float(i-1)+(den>1e-8f?(d-arc[i-1])/den:0))/128;
    Point p=Spline(points,u,smoothFlight);
    if(tangent){Point q=Spline(points,u+.001f,smoothFlight);*tangent={q.x-p.x,q.y-p.y,q.z-p.z};}return p;
}
float Fade(float d,float range) {
    float x=std::clamp((range-d)/(range*.2f),0.f,1.f),near=std::clamp((d-1.5f)/1.5f,0.f,1.f);
    return x*x*(3-2*x)*near*near*(3-2*near);
}
bool InFrustum(const float vp[4][4],Point p,float r) {
    const float xyz[4]={p.x,p.y,p.z,1};float q[4]={};for(int j=0;j<4;++j)for(int i=0;i<4;++i)q[j]+=xyz[i]*vp[i][j];
    // D3D clip planes, including the near plane (z >= 0). Sphere-plane radius in clip space.
    for(int plane=0;plane<6;++plane) {
        const int axis=plane/2;const float sign=plane%2?-1.f:1.f;float n[4];
        for(int i=0;i<4;++i)n[i]=(plane==4?0:vp[i][3])+sign*vp[i][axis];
        const float value=plane==4?q[2]:q[3]+sign*q[axis];
        if(value < -r*std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]))return false;
    }return true;
}
bool LocalReviewEnabled(const std::string& scenePath){
    // Explicit per-client opt-in; never changes route verification or global settings.
    try{
        auto path=std::filesystem::path(scenePath).parent_path()/"review.ini";
        std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f||f.tellg()>2048)return false;f.seekg(0);
        std::string line;bool section=false;
        while(std::getline(f,line)){
            const auto first=line.find_first_not_of(" \t\r"),last=line.find_last_not_of(" \t\r");if(first==std::string::npos)continue;line=line.substr(first,last-first+1);
            if(line[0]=='['){section=line=="[LivingRubiKa]";continue;}
            if(section&&line=="ReviewUnverifiedRoutes=1")return true;
        }
    }catch(const std::exception&){}return false;
}
bool Load(const std::string& path,Scene& out,std::string& error,uint32_t expectedPlayfield,bool requireLiveVerification,const std::vector<std::string>* verifiedAssets) {
    try {
        const bool ram=expectedPlayfield==FixturePlayfield&&FixturePath(path);
        Scene s;auto stream=Open(path,ram);auto& f=*stream;std::string magic,provenance;uint32_t verified,fixture,mc,ac;
        const auto sceneBytes=Size(path,ram);Require(sceneBytes<512u*1024u,"scene exceeds 512 KiB");
        Require(bool(f>>magic>>s.playfield>>verified>>fixture>>mc>>ac>>provenance)&&(magic=="LRSCENE2"||magic=="LRSCENE3"||magic=="LRSCENE4"||magic=="LRSCENE5"||magic=="LRSCENE6"||magic=="LRSCENE7"||magic=="LRSCENE8"||magic=="LRSCENE9"),"bad scene header");
        const bool v9=magic=="LRSCENE9",v8=magic=="LRSCENE8"||v9,v7=magic=="LRSCENE7"||v8,v6=magic=="LRSCENE6"||v7,v5=magic=="LRSCENE5"||v6,v4=magic=="LRSCENE4"||v5,locomotion=magic!="LRSCENE2";
        Require(v5||sceneBytes<65536,"legacy scene exceeds 64 KiB");
        Require(verified>=1&&verified<=(v4?2u:1u)&&fixture<=1&&mc>0&&mc<=(v6?48u:16u)&&ac<=(v5?256u:64u),"unreviewed placement or invalid budget");
        s.liveVerified=v4&&verified==2;
        Require(!expectedPlayfield||s.playfield==expectedPlayfield,"playfield mismatch; asset loading refused");
        Require(!requireLiveVerification||fixture||s.liveVerified,"static-only route pack disabled; live verification pending (isolated review mode required)");
        s.fixture=fixture!=0;Require(s.playfield!=0&&((s.playfield==FixturePlayfield)==s.fixture),"fixture playfield mismatch");
        Require(provenance.size()>=8&&provenance!="unknown","placement requires provenance record");
        for(uint32_t i=0;i<mc;++i){
            std::string name;Require(bool(f>>name),"missing asset");Require(name.size()<64&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos&&name.find("..") == std::string::npos,"unsafe asset name");
            s.meshes.emplace_back();const auto before=s.bytes;
            try {
                Require(!verifiedAssets||std::find(verifiedAssets->begin(),verifiedAssets->end(),name)!=verifiedAssets->end(),"original asset missing or integrity check failed");
                LoadMesh(std::filesystem::path(path).parent_path()/name,s.meshes.back(),s.bytes,ram);
            }catch(const std::exception& e){
                if(!verifiedAssets)throw;
                s.meshes.back()=Mesh{};s.meshes.back().available=false;s.bytes=before;
                s.diagnostics.push_back(name+": "+e.what()+"; affected instances skipped");continue;
            }
            auto& current=s.meshes.back();
            for(uint32_t j=0;j<i;++j)if(s.meshes[j].available&&s.meshes[j].materialOwner==~0u&&s.meshes[j].dimension==current.dimension&&s.meshes[j].pixels==current.pixels){
                current.materialOwner=j;for(auto& pixels:current.pixels){s.bytes-=pixels.size()*4;pixels.clear();pixels.shrink_to_fit();}break;
            }
            Require(s.bytes<=MaxBytes,"64 MiB ambient source budget exceeded");
        }
        for(uint32_t i=0;i<ac;++i) {
            Actor a;uint32_t count;Require(bool(f>>a.group>>a.mesh>>a.tier>>a.motion>>a.scale>>a.speed>>a.phase>>a.range>>a.hover>>count),"bad actor");
            Require(a.group<6&&a.mesh<mc&&a.tier>=1&&a.tier<=3&&a.motion<=(v6?8u:v4?6u:locomotion?4u:2u)&&count>=1&&count<=32,"invalid actor indices");
            a.route.linear=a.motion>=7;
            Require(a.motion<7||(a.group==2&&count==2&&a.hover==0),"aerial corridor must be a straight flight without hover");
            Require(Finite(a.scale,20)&&a.scale>.01f&&Finite(a.speed,100)&&Finite(a.phase)&&Finite(a.range,1000)&&a.range>=10&&Finite(a.hover,1)&&a.hover>=0,"invalid actor parameters");
            Require(a.motion==0||a.motion>=3||count==1,"machinery/emitter must have a fixed anchor");
            // Grounded creatures require a reviewed independent animated asset, never static humanoids.
            Require(!s.meshes[a.mesh].available||(a.group!=0&&a.group!=3)||s.meshes[a.mesh].bones,"people/wildlife require a rigged animated asset");
            a.route.points.resize(count);for(auto& p:a.route.points)Require(bool(f>>p.x>>p.y>>p.z),"missing route knot");
            if(locomotion)Require(bool(f>>a.walkSeconds>>a.pauseSeconds>>a.heading)&&Finite(a.walkSeconds,120)&&a.walkSeconds>=2&&Finite(a.pauseSeconds,v6?3600.f:60.f)&&a.pauseSeconds>=0&&Finite(a.heading,100),"invalid locomotion schedule");
            a.idleYaw=a.heading;
            if(v4)Require(bool(f>>a.idleYaw>>a.lightRange>>a.lightColour>>a.replacementMask>>a.parent)&&Finite(a.idleYaw,100)&&Finite(a.lightRange,8)&&a.lightRange>=0&&a.lightColour<=0xffffff&&a.replacementMask<(v5?0x1000000u:256u)&&a.parent>=-1&&a.parent<int32_t(i),"invalid activity, attachment or service light");
            if((a.motion==3||a.motion==4)&&s.meshes[a.mesh].available)Require(s.meshes[a.mesh].walkFrames>0,"locomotion requires walk/idle clips");
            if(v5){uint32_t surface;Require(bool(f>>surface)&&surface<=1,"invalid authored surface mode");a.surface=surface!=0;Require(!a.surface||a.motion==3,"authored support is only for grounded locomotion");}
            Require(a.route.Finish(),"invalid or degenerate route");
            s.bytes+=sizeof(Actor)+a.route.points.capacity()*sizeof(Point)+a.route.arc.capacity()*sizeof(float);Require(s.bytes<=MaxBytes,"route data exceeds source budget");s.actors.push_back(std::move(a));
        }
        if(v4){
            uint32_t count;Require(bool(f>>count)&&count<=(v5?24u:8u),"invalid replacement count");
            for(uint32_t i=0;i<count;++i){Replacement r;Require(bool(f>>r.vertices>>r.indices>>r.origin.x>>r.origin.y>>r.origin.z>>r.hash)&&r.vertices>=3&&r.vertices<=20000&&r.indices>=3&&r.indices<=60000&&r.indices%3==0&&r.hash&&Finite(r.origin.x)&&Finite(r.origin.y)&&Finite(r.origin.z),"invalid original part fingerprint");s.replacements.push_back(r);}
            for(const auto& a:s.actors)Require(a.replacementMask<(1u<<count),"replacement mask outside fingerprint list");
            std::string name;Require(bool(f>>name),"missing ground declaration");
            if(name!="-"){
                Require(name.size()<64&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos&&name.find("..") == std::string::npos,"unsafe ground name");
                auto groundPath=std::filesystem::path(path).parent_path()/name;auto groundStream=Open(groundPath,ram);auto& g=*groundStream;uint32_t h[4];Read(g,h,4);Read(g,&s.groundCell,1);
                Require(h[0]==0x3152474c&&(h[1]==s.playfield||s.fixture)&&h[2]>=2&&h[3]>=2&&h[2]<=(v5?2049u:1025u)&&h[3]<=(v5?2049u:1025u)&&s.groundCell>=1&&s.groundCell<=32,"invalid original ground field");
                s.groundWidth=h[2];s.groundHeight=h[3];s.ground.resize(size_t(h[2])*h[3]);Require(Size(groundPath,ram)==20+s.ground.size()*4,"ground size mismatch");Read(g,s.ground.data(),s.ground.size());
                for(float x:s.ground)Require(Finite(x),"invalid ground height");s.bytes+=s.ground.size()*4;Require(s.bytes<=MaxBytes,"ground exceeds source budget");
            }
        }
        if(v6){std::string token;uint32_t count;Require(bool(f>>token>>count)&&token=="APPEARANCES"&&count<=32,"invalid appearance pool");
            for(uint32_t i=0;i<count;++i){Scene::Appearance appearance;Require(bool(f>>appearance.mesh>>appearance.role)&&appearance.mesh<mc&&appearance.role<=3,"invalid appearance descriptor");
                Require(!s.meshes[appearance.mesh].available||s.meshes[appearance.mesh].bones,"appearance requires original animated body");
                for(const auto& old:s.appearances)Require(old.mesh!=appearance.mesh,"duplicate appearance descriptor");s.appearances.push_back(appearance);}
            if(count){uint32_t roster;Require(bool(f>>token>>roster)&&token=="ROSTER"&&roster<=ac,"missing verified appearance clearance roster");
                std::vector<bool> seen(ac);for(uint32_t i=0;i<roster;++i){uint32_t actor,mask;Require(bool(f>>actor>>mask)&&actor<ac&&!seen[actor]&&s.actors[actor].group==0&&(count==32||mask<(1u<<count)),"invalid appearance clearance mask");seen[actor]=true;s.actors[actor].appearanceMask=mask;}
                for(uint32_t i=0;i<ac;++i)Require(s.actors[i].group!=0||seen[i],"person missing appearance clearance");}}
        if(v7){std::string token;uint32_t nodes,edges,lanes;
            Require(bool(f>>token>>nodes>>edges)&&token=="WALKGRAPH"&&nodes<=768&&edges<=4096,"invalid pedestrian graph budget");
            for(uint32_t i=0;i<nodes;++i){Scene::WalkNode n;Require(bool(f>>n.point.x>>n.point.y>>n.point.z>>n.restMask)&&Finite(n.point.x)&&Finite(n.point.y)&&Finite(n.point.z),"invalid pedestrian node");s.walkNodes.push_back(n);}
            for(uint32_t i=0;i<edges;++i){Scene::WalkEdge e;Require(bool(f>>e.a>>e.b>>e.mask)&&e.a<nodes&&e.b<nodes&&e.a!=e.b&&e.mask,"invalid pedestrian edge");float d=Distance(s.walkNodes[e.a].point,s.walkNodes[e.b].point);Require(d>=1&&d<=18,"pedestrian edge outside validated length");s.walkEdges.push_back(e);}
            Require(bool(f>>token>>lanes)&&token=="FLIGHTLANES"&&lanes<=16,"invalid flight lane budget");
            for(uint32_t i=0;i<lanes;++i){Route lane;lane.linear=true;lane.points.resize(2);for(auto& p:lane.points)Require(bool(f>>p.x>>p.y>>p.z),"missing lane endpoint");
                if(v8){uint32_t low;Require(bool(f>>low)&&low<=1,"invalid low-flight mode");lane.lowFlight=low!=0;}
                Require(lane.Finish()&&lane.length>=(lane.lowFlight?24.f:100.f)&&lane.length<=440,"invalid flight lane");s.flightLanes.push_back(std::move(lane));}
            s.bytes+=nodes*sizeof(Scene::WalkNode)+edges*sizeof(Scene::WalkEdge)+lanes*128;Require(s.bytes<=MaxBytes,"behavior data exceeds budget");s.humanBehavior=true;
        }
        if(v9){for(auto pair:{std::make_pair("FLIGHTLOOPS",&s.flightLoops),std::make_pair("LARGELOOPS",&s.largeLoops)}){
            std::string token;uint32_t count;Require(bool(f>>token>>count)&&token==pair.first&&count<=16,"invalid continuous-flight budget");
            for(uint32_t i=0;i<count;++i){Route route;route.smoothFlight=true;uint32_t low,points;Require(bool(f>>low>>points)&&low<=1&&points>=4&&points<=16,"invalid closed flight plan");route.lowFlight=low!=0;route.points.resize(points);
                for(auto& p:route.points)Require(bool(f>>p.x>>p.y>>p.z),"missing flight plan control point");
                Require(route.Finish()&&route.length>=80&&route.length<=10000,"invalid closed flight length");s.bytes+=sizeof(Route)+points*sizeof(Point)+route.arc.size()*sizeof(float);pair.second->push_back(std::move(route));}
        }Require(s.bytes<=MaxBytes,"continuous flight plans exceed budget");}
        std::string trailing;Require(!(f>>trailing),"trailing scene data");out=std::move(s);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void SelectAppearances(Scene& s,uint32_t armoredLevel) {
    s.armoredLevel=std::min(armoredLevel,2u);
    if(s.appearances.empty())return;
    const float weights[3][4]={{72,28,0,0},{64,23,10,3},{52,23,20,5}};
    std::array<uint32_t,4> roleCounts{};std::vector<uint32_t> counts(s.meshes.size());
    struct Placed{Point point;uint32_t mesh;};std::vector<Placed> placed;uint32_t total=0;
    auto hash=[](uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);};
    for(size_t index=0;index<s.actors.size();++index){auto& actor=s.actors[index];if(actor.group!=0)continue;
        Point p{};for(auto q:actor.route.points){p.x+=q.x;p.z+=q.z;}p.x/=actor.route.points.size();p.z/=actor.route.points.size();
        float best=-1e9f;const Scene::Appearance* selected=nullptr;
        for(size_t k=0;k<s.appearances.size();++k){const auto& choice=s.appearances[k];if(!(actor.appearanceMask&(1u<<k))||!s.meshes[choice.mesh].available||weights[s.armoredLevel][choice.role]==0)continue;
            uint32_t alternatives=0;for(auto c:s.appearances)alternatives+=c.role==choice.role&&s.meshes[c.mesh].available;
            const float roleDeficit=float(total+1)*weights[s.armoredLevel][choice.role]/100.f-roleCounts[choice.role];
            float repeats=0;for(const auto& old:placed)if(old.mesh==choice.mesh)repeats+=std::max(0.f,1-std::hypot(p.x-old.point.x,p.z-old.point.z)/24.f);
            const float noise=float(hash(s.playfield*773u+uint32_t(index)*997u+choice.mesh*37u)&65535)/65535.f;
            const float score=roleDeficit*3-float(counts[choice.mesh])/std::max(1u,alternatives)-repeats*8+noise*.35f;
            if(score>best){best=score;selected=&choice;}}
        if(selected){actor.mesh=selected->mesh;++roleCounts[selected->role];++counts[selected->mesh];++total;placed.push_back({p,selected->mesh});}
        else actor.appearanceMask=0; // no permitted, available body: skip this instance
    }
}

double Timeline::Advance(double wall) {
    if(!std::isfinite(wall))return time;
    const double elapsed=last<0?0:wall-last;last=wall;
    if(elapsed>0&&elapsed<=.25)time+=std::min(elapsed,.05);
    return time;
}
Movement Locomotion(double time,const Actor& a) {
    Movement result;
    if(a.speed<=0||a.route.points.size()==1)return result;
    const double walk=a.walkSeconds,pause=a.pauseSeconds,ramp=std::min(.65,walk*.2),period=walk+pause;
    const double elapsed=std::max(0.,time+double(a.phase)),cycle=std::floor(elapsed/period),t=elapsed-cycle*period;
    auto smooth=[](double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);};
    auto integral=[](double x){return x*x*x-.5*x*x*x*x;};
    double d=walk-ramp,v=0;
    if(t<ramp){const double x=t/ramp;d=ramp*integral(x);v=smooth(x);}
    else if(t<walk-ramp){d=t-ramp*.5;v=1;}
    else if(t<walk){const double x=(walk-t)/ramp;d=walk-ramp-ramp*integral(x);v=smooth(x);}
    result.distance=(cycle*(walk-ramp)+d)*a.speed;result.speed=float(v*a.speed);result.walkWeight=float(smooth(v));return result;
}
float GaitFrame(const Mesh& m,double distance) {
    if(!m.walkFrames||m.gaitDistance.size()<2||m.stride<=0)return 0;
    double d=std::fmod(distance,double(m.stride));if(d<0)d+=m.stride;
    auto at=std::upper_bound(m.gaitDistance.begin(),m.gaitDistance.end(),float(d));
    size_t i=std::clamp<size_t>(size_t(at-m.gaitDistance.begin()),1,m.gaitDistance.size()-1);
    return float(i-1)+float((d-m.gaitDistance[i-1])/(m.gaitDistance[i]-m.gaitDistance[i-1]));
}
uint64_t GeometryHash(const void* vertices,uint32_t vc,uint32_t stride,const uint16_t* indices,uint32_t ic,const float w[4][4]) {
    uint64_t h=14695981039346656037ull;
    auto word=[&](uint32_t x){for(int k=0;k<4;++k){h^=uint8_t(x>>(k*8));h*=1099511628211ull;}};
    for(uint32_t i=0;i<vc;++i){float p[3];std::memcpy(p,static_cast<const uint8_t*>(vertices)+size_t(i)*stride,12);
        for(int j=0;j<3;++j){double q=double(p[0])*w[0][j]+double(p[1])*w[1][j]+double(p[2])*w[2][j]+w[3][j];if(!std::isfinite(q)||std::fabs(q)>100000)return 0;word(uint32_t(int32_t(std::round(q*200.))));}}
    for(uint32_t i=0;i<ic;++i)word(indices[i]);return h;
}
bool GroundHeight(const Scene& s,float x,float z,float& h){
    if(s.ground.empty())return false;float u=x/s.groundCell,v=z/s.groundCell;
    if(!std::isfinite(u)||!std::isfinite(v)||u<0||v<0||u>=s.groundWidth-1||v>=s.groundHeight-1)return false;
    uint32_t ix=uint32_t(u),iz=uint32_t(v);u-=ix;v-=iz;size_t i=size_t(iz)*s.groundWidth+ix;
    const float a=s.ground[i],b=s.ground[i+1],c=s.ground[i+s.groundWidth],d=s.ground[i+s.groundWidth+1];
    const float ad=std::fabs(a-d),bc=std::fabs(b-c);
    if((ad>s.groundCell||bc>s.groundCell)&&ad<bc-.01f)h=v>u?a+v*(c-a)+u*(d-c):a+u*(b-a)+v*(d-b);
    else h=u+v<1?a+u*(b-a)+v*(c-a):d+(1-u)*(c-d)+(1-v)*(b-d);return true;
}
float TurnTowards(float previous,float desired,float seconds,float rate){
    const float delta=std::atan2(std::sin(desired-previous),std::cos(desired-previous));
    const float step=std::clamp(seconds,0.f,.1f)*std::max(0.f,rate);return previous+std::clamp(delta,-step,step);
}
float ContactOffset(const Scene& scene,const Mesh& mesh,const skin::Palette& palette,const float w[4][4],bool authoredSurface){
    if(mesh.contacts.empty())return 0;
    std::array<skin::Vertex,96> feet{};skin::SkinVertices(mesh.contacts.data(),uint32_t(mesh.contacts.size()),feet.data(),palette,false,nullptr,nullptr);
    float correction=-1e9f;
    for(size_t i=0;i<mesh.contacts.size();++i){const auto& p=feet[i].pos;float q[3];for(int j=0;j<3;++j)q[j]=p[0]*w[0][j]+p[1]*w[1][j]+p[2]*w[2][j]+w[3][j];
        float ground=w[3][1];if(!authoredSurface&&!scene.ground.empty()){if(!GroundHeight(scene,q[0],q[2],ground))continue;ground+=.008f;}
        correction=std::max(correction,ground-q[1]);
    }
    const float limit=mesh.activities.empty()?.08f:.16f;
    return correction<-100?0:std::clamp(correction,-limit,limit);
}

}
