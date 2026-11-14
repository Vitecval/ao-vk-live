#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
namespace rvk::rocks::clusters {
struct Vertex {float x,y,z,nx,ny,nz;uint32_t diffuse;float u,v;};
static_assert(sizeof(Vertex)==36);
struct P {float x,y,z;};
inline P add(P a,P b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline P sub(P a,P b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline P mul(P a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float dot(P a,P b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline P cross(P a,P b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline P unit(P p){return mul(p,1.f/std::max(std::sqrt(dot(p,p)),1e-8f));}
inline uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
struct Random {uint32_t seed;float next(){seed=hash(seed+0x9e3779b9u);return float(seed&0xFFFFFF)/16777216.f;}
    float range(float a,float b){return a+(b-a)*next();}};
struct Plane {P n;float d;};
struct Part {std::vector<Vertex> v;std::vector<uint16_t> ix;};
struct Model {Part stone,moss,ground;uint32_t blocks=0;};
inline uint32_t tint(float r,float g,float b){auto c=[](float x){return uint32_t(std::clamp(x,0.f,1.f)*255+.5f);};
    return 0xFF000000u|(c(r)<<16)|(c(g)<<8)|c(b);}

inline void face(Part& part,const std::vector<P>& points,uint32_t seed,float shade,bool green=false,
          const std::vector<P>* cornerNormals=nullptr,float round=0){
    if(points.size()<3)return;
    P area{};for(size_t i=1;i+1<points.size();++i)area=add(area,cross(sub(points[i],points[0]),sub(points[i+1],points[0])));
    P n=unit(area); // A clipped polygon may start with almost-collinear corners.
    P u=unit(cross(std::fabs(n.y)>.95f?P{0,0,1}:P{0,1,0},n)),v=cross(n,u);
    Random random{seed};const float phaseU=random.range(0,7),phaseV=random.range(0,7);
    const float scale=random.range(.18f,.245f),mirror=random.next()<.5f?-1.f:1.f;
    const uint16_t first=uint16_t(part.v.size());
    auto vertex=[&](P p,P normal){
        float macro=.96f+.035f*std::sin(p.x*.31f+p.z*.17f)+.025f*std::sin(p.y*.51f+p.x*.13f);
        const uint32_t colour=green?tint(shade*macro*.85f,shade*macro,shade*macro*.74f):
            tint(shade*macro*.98f,shade*macro,shade*macro*1.02f);
        part.v.push_back({p.x,p.y,p.z,normal.x,normal.y,normal.z,colour,dot(p,u)*scale*mirror+phaseU,dot(p,v)*scale+phaseV});
    };
    if(cornerNormals){
        P centre{};for(P p:points)centre=add(centre,p);centre=mul(centre,1.f/float(points.size()));
        for(size_t i=0;i<points.size();++i)vertex(sub(points[i],mul((*cornerNormals)[i],round)),(*cornerNormals)[i]);
        for(P p:points){P towards=sub(centre,p);float length=std::sqrt(dot(towards,towards));vertex(add(p,mul(towards,std::min(.35f,round*3/std::max(length,.001f)))),n);}
        const uint16_t inner=uint16_t(first+points.size());
        for(size_t k=0;k<points.size();++k){const uint16_t j=uint16_t((k+1)%points.size());
            part.ix.insert(part.ix.end(),{uint16_t(first+k),uint16_t(first+j),uint16_t(inner+k),uint16_t(inner+k),uint16_t(first+j),uint16_t(inner+j)});}
        for(size_t k=1;k+1<points.size();++k)part.ix.insert(part.ix.end(),{inner,uint16_t(inner+k),uint16_t(inner+k+1)});
    }else{
        for(P p:points)vertex(p,n);
        for(size_t k=1;k+1<points.size();++k)part.ix.insert(part.ix.end(),{first,uint16_t(first+k),uint16_t(first+k+1)});
    }
}

inline void rock(Model& model,P at,P radii,uint32_t seed,bool moss=false,float turn=0,bool bevel=true){
    Random rnd{seed};
    std::vector<Plane> planes={{{1,.06f,.08f},1},{{-1,-.09f,.04f},1},{{.05f,1,-.05f},1},
        {{-.06f,-1,.07f},1},{{.11f,-.09f,1},1},{{rnd.range(-.16f,.16f),rnd.range(-.24f,.24f),-1},rnd.range(.78f,1.05f)}};
    for(int k=0;k<8;++k){
        // Unequal oblique cuts, rather than identical bevels on a repeated cube.
        const float x=(k&1)?1.f:-1.f,y=(k&2)?1.f:-1.f,z=(k&4)?1.f:-1.f;
        planes.push_back({{x*rnd.range(.65f,1.2f),y*rnd.range(.65f,1.2f),z*rnd.range(.65f,1.2f)},rnd.range(1.55f,2.25f)});
    }
    for(int side:{-1,1})planes.push_back({{float(side),rnd.range(-.15f,.15f),-rnd.range(.8f,1.2f)},rnd.range(1.4f,1.8f)});
    // Keep clipping intersections in double precision. Face membership must
    // be tighter than the inside test: admitting a nearby unrelated corner
    // creates a T-junction and leaves cracks between adjacent cut faces.
    struct Precise {double x,y,z;};
    auto dp=[](P p){return Precise{p.x,p.y,p.z};};
    auto dc=[](Precise a,Precise b){return Precise{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
    auto dd=[](Precise a,Precise b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    std::vector<Precise> vertices;
    for(size_t a=0;a<planes.size();++a)for(size_t b=a+1;b<planes.size();++b)for(size_t c=b+1;c<planes.size();++c){
        const auto& p=planes[a];const auto& q=planes[b];const auto& r=planes[c];
        auto pn=dp(p.n),qn=dp(q.n),rn=dp(r.n);double det=dd(pn,dc(qn,rn));if(std::fabs(det)<1e-10)continue;
        auto qr=dc(qn,rn),rp=dc(rn,pn),pq=dc(pn,qn);
        Precise v{(qr.x*p.d+rp.x*q.d+pq.x*r.d)/det,(qr.y*p.d+rp.y*q.d+pq.y*r.d)/det,(qr.z*p.d+rp.z*q.d+pq.z*r.d)/det};
        bool inside=true;for(auto pl:planes)if(dd(dp(pl.n),v)>pl.d+1e-9){inside=false;break;}
        if(!inside)continue;
        bool duplicate=false;for(auto old:vertices)if((v.x-old.x)*(v.x-old.x)+(v.y-old.y)*(v.y-old.y)+(v.z-old.z)*(v.z-old.z)<1e-18){duplicate=true;break;}
        if(!duplicate)vertices.push_back(v);
    }
    const float co=std::cos(turn),si=std::sin(turn);
    auto world=[&](P p){p={p.x*radii.x,p.y*radii.y,p.z*radii.z};return add(at,{p.x*co+p.z*si,p.y,-p.x*si+p.z*co});};
    auto worldNormal=[&](P p){p={p.x/radii.x,p.y/radii.y,p.z/radii.z};return unit({p.x*co+p.z*si,p.y,-p.x*si+p.z*co});};
    const float shade=rnd.range(.88f,.98f);
    for(auto plane:planes){
        std::vector<P> polygon;P centre{};
        for(auto precise:vertices)if(std::fabs(dd(dp(plane.n),precise)-plane.d)<1e-8){P p{float(precise.x),float(precise.y),float(precise.z)};polygon.push_back(p);centre=add(centre,p);}
        if(polygon.size()<3)continue;
        centre=mul(centre,1.f/float(polygon.size()));const P normal=unit(plane.n);
        const P u=unit(cross(std::fabs(normal.y)>.9f?P{0,0,1}:P{0,1,0},normal)),v=cross(normal,u);
        std::sort(polygon.begin(),polygon.end(),[&](P a,P b){a=sub(a,centre);b=sub(b,centre);return std::atan2(dot(a,v),dot(a,u))<std::atan2(dot(b,v),dot(b,u));});
        std::vector<P> cornerNormals;
        for(P& p:polygon){P average{};for(auto other:planes)if(std::fabs(dot(other.n,p)-other.d)<1e-6f)average=add(average,worldNormal(other.n));cornerNormals.push_back(unit(average));p=world(p);}
        const bool cap=moss&&normal.y>.68f;
        face(cap?model.moss:model.stone,polygon,seed,cap?.94f:shade,cap,bevel?&cornerNormals:nullptr,std::min(.08f,std::min({radii.x,radii.y,radii.z})*.1f));
    }
    ++model.blocks;
}

}
