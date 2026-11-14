#pragma once
#include "rock_clusters.h"
namespace rvk::rocks::clusters {
struct Surface {
    const void* vertices=nullptr;const uint16_t* indices=nullptr;
    uint32_t vertexCount=0,indexCount=0,stride=0,primitive=4,positionOffset=0;
    int normalOffset=-1,diffuseOffset=-1;
    float scale=1,strength=1;P up{0,1,0};uint32_t seed=1;
};
struct Dressing {Part mesh;uint32_t rocks=0;bool cliff=false;};
inline Dressing Build(const Surface& s){
    Dressing out;
    if(!s.vertices||!s.vertexCount||s.vertexCount>65535||s.positionOffset+12>s.stride||
       !std::isfinite(s.scale)||s.scale<.001f||!std::isfinite(s.strength)||s.strength<=0||
       !std::isfinite(dot(s.up,s.up))||std::fabs(dot(s.up,s.up)-1.f)>.01f)return out;
    std::vector<P> positions(s.vertexCount);P lo{1e30f,1e30f,1e30f},hi{-1e30f,-1e30f,-1e30f};float upHi=-1e30f,upLo=1e30f;
    for(uint32_t i=0;i<s.vertexCount;++i){P p;std::memcpy(&p,static_cast<const char*>(s.vertices)+size_t(i)*s.stride+s.positionOffset,12);
        if(!std::isfinite(p.x+p.y+p.z)||std::max({std::fabs(p.x),std::fabs(p.y),std::fabs(p.z)})>1e7f)return {};
        positions[i]=p;lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};upHi=std::max(upHi,dot(p,s.up));upLo=std::min(upLo,dot(p,s.up));}
    const P span=sub(hi,lo);const float maximum=std::max({span.x,span.y,span.z})*s.scale;if(maximum<.55f)return out;
    out.cliff=maximum>=7.5f||std::min({span.x,span.y,span.z})<std::max({span.x,span.y,span.z})*.15f;
    struct Triangle {uint32_t a,b,c;P normal;float end;};std::vector<Triangle> triangles;float area=0;
    const uint32_t count=s.indices?s.indexCount:s.vertexCount;auto at=[&](uint32_t i){return s.indices?uint32_t(s.indices[i]):i;};
    auto triangle=[&](uint32_t a,uint32_t b,uint32_t c){
        if(a>=positions.size()||b>=positions.size()||c>=positions.size())return;
        P n=cross(sub(positions[b],positions[a]),sub(positions[c],positions[a]));float len=std::sqrt(dot(n,n));if(len<1e-7f)return;n=mul(n,1.f/len);
        if(s.normalOffset>=0&&uint32_t(s.normalOffset)+12<=s.stride){P supplied{};for(auto i:{a,b,c}){P v;std::memcpy(&v,static_cast<const char*>(s.vertices)+size_t(i)*s.stride+s.normalOffset,12);supplied=add(supplied,v);}if(dot(n,supplied)<0)n=mul(n,-1);}
        // Preserve upward cliff/landing surfaces; floating undersides remain eligible.
        if(out.cliff&&dot(n,s.up)>.45f)return;
        area+=.5f*len;triangles.push_back({a,b,c,n,area});};
    if(s.primitive==4){for(uint32_t i=0;i+2<count;i+=3)triangle(at(i),at(i+1),at(i+2));}
    else if(s.primitive==5){for(uint32_t i=0;i+2<count;++i)triangle(at(i+(i&1)),at(i+1-(i&1)),at(i+2));}
    else if(s.primitive==6){for(uint32_t i=1;i+1<count;++i)triangle(at(0),at(i),at(i+1));}else return out;
    if(area<=1e-6f||triangles.empty())return out;
    const float worldArea=area*s.scale*s.scale;
    const uint32_t wanted=uint32_t(std::clamp(worldArea/(out.cliff?20.f:4.f),out.cliff?2.f:3.f,out.cliff?24.f:12.f));
    const float base=std::clamp(std::sqrt(worldArea/wanted)*.68f,.18f,std::min(6.f,maximum*.48f))/s.scale;
    Random random{s.seed};std::vector<P> placed;
    for(uint32_t attempt=0;attempt<wanted*18&&out.rocks<wanted;++attempt){
        const float pick=random.next()*area;auto found=std::lower_bound(triangles.begin(),triangles.end(),pick,[](const Triangle& t,float v){return t.end<v;});if(found==triangles.end())continue;const auto& t=*found;
        const float root=std::sqrt(random.next()),b=root*random.next(),c=root-b,a=1-root;
        const P anchor=add(add(mul(positions[t.a],a),mul(positions[t.b],b)),mul(positions[t.c],c));
        bool close=false;for(P old:placed)if(dot(sub(anchor,old),sub(anchor,old))<base*base*1.25f){close=true;break;}if(close)continue;
        P u=unit(cross(s.up,t.normal));if(dot(u,u)<.5f)u=unit(cross(P{1,0,0},t.normal));const P v=cross(t.normal,u);
        const float radius=base*random.range(.8f,1.18f)*std::clamp(s.strength,0.f,1.f),depth=radius*random.range(1.05f,1.4f);
        Model single;rock(single,{0,0,0},{radius*random.range(.9f,1.25f),radius*random.range(.8f,1.15f),depth},hash(s.seed+attempt),false,random.range(-.2f,.2f));
        const P centre=sub(anchor,mul(t.normal,depth*.3f));float top=-1e30f;
        for(auto& vertex:single.stone.v){P p=add(centre,add(add(mul(u,vertex.x),mul(v,vertex.y)),mul(t.normal,vertex.z)));
            P n=unit(add(add(mul(u,vertex.nx),mul(v,vertex.ny)),mul(t.normal,vertex.nz)));
            vertex.x=p.x;vertex.y=p.y;vertex.z=p.z;vertex.nx=n.x;vertex.ny=n.y;vertex.nz=n.z;top=std::max(top,dot(p,s.up));}
        const bool protectTop=out.cliff&&(upHi-upLo)*s.scale>.5f;
        const float lower=protectTop?std::max(0.f,top-(upHi-.12f/s.scale)):0.f;if(lower>radius*.8f)continue;
        uint32_t colour=0xFFFFFFFFu;
        if(s.diffuseOffset>=0&&uint32_t(s.diffuseOffset)+4<=s.stride){uint32_t colours[3],ids[3]={t.a,t.b,t.c};
            for(int i=0;i<3;++i)std::memcpy(&colours[i],static_cast<const char*>(s.vertices)+size_t(ids[i])*s.stride+s.diffuseOffset,4);
            colour=0xFF000000u;for(unsigned channel=0;channel<24;channel+=8)colour|=uint32_t(((colours[0]>>channel)&255)*a+((colours[1]>>channel)&255)*b+((colours[2]>>channel)&255)*c)<<channel;}
        if(out.mesh.v.size()+single.stone.v.size()>20000)break;const uint16_t first=uint16_t(out.mesh.v.size());
        for(auto vertex:single.stone.v){vertex.x-=s.up.x*lower;vertex.y-=s.up.y*lower;vertex.z-=s.up.z*lower;vertex.diffuse=colour;out.mesh.v.push_back(vertex);}
        for(auto i:single.stone.ix)out.mesh.ix.push_back(uint16_t(first+i));placed.push_back(anchor);++out.rocks;
    }
    return out;
}
}
