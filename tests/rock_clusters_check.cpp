#include "rock_cluster_surface.h"
#include "material_profiles.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>
#include <tuple>
#include <chrono>
using namespace rvk::rocks::clusters;
using Point=std::tuple<int,int,int>;
Point point(const Vertex& p){int values[3];std::memcpy(values,&p.x,sizeof(values));return {values[0],values[1],values[2]};}
void valid(const Dressing& d,bool closed=true){
    assert(d.mesh.v.size()<=20000&&d.rocks<=24&&d.mesh.ix.size()%3==0);
    std::map<std::pair<Point,Point>,int> edges;
    for(auto& p:d.mesh.v){assert(std::isfinite(p.x+p.y+p.z+p.nx+p.ny+p.nz+p.u+p.v));assert(std::fabs(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz-1)<.002f);}
    for(size_t i=0;i<d.mesh.ix.size();i+=3){
        for(int j=0;j<3;++j){assert(d.mesh.ix[i+j]<d.mesh.v.size());auto a=point(d.mesh.v[d.mesh.ix[i+j]]),b=point(d.mesh.v[d.mesh.ix[i+(j+1)%3]]);if(b<a)std::swap(a,b);if(a!=b)++edges[{a,b}];}}
    if(closed)for(auto& edge:edges)if(edge.second!=2){auto [ax,ay,az]=edge.first.first;auto [bx,by,bz]=edge.first.second;
        std::printf("bad edge %d: (%d,%d,%d) - (%d,%d,%d); %u rocks\n",edge.second,ax,ay,az,bx,by,bz,d.rocks);std::fflush(stdout);assert(edge.second==2);}
}
Surface surface(Part& p){Surface s;s.vertices=p.v.data();s.indices=p.ix.data();s.vertexCount=uint32_t(p.v.size());s.indexCount=uint32_t(p.ix.size());s.stride=sizeof(Vertex);s.normalOffset=12;s.diffuseOffset=24;return s;}
int main(){
    for(uint32_t id:{5955u,9863u,205836u,214375u,205675u,205838u,202640u})assert(rvk::materials::NaturalRock(1010004,id));
    for(uint64_t key:rvk::materials::rockCatalog)assert(rvk::materials::NaturalRock(uint32_t(key>>32),uint32_t(key)));
    assert(!rvk::materials::NaturalRock(1010004,0));
    Part wall;face(wall,{{-20,0,0},{-20,12,0},{20,12,0},{20,0,0}},17,.9f);
    auto s=surface(wall);auto a=Build(s),b=Build(s);assert(a.cliff&&a.rocks>=10);valid(a);
    assert(a.mesh.v.size()==b.mesh.v.size()&&a.mesh.ix==b.mesh.ix&&std::memcmp(a.mesh.v.data(),b.mesh.v.data(),a.mesh.v.size()*sizeof(Vertex))==0);
    for(auto p:a.mesh.v)assert(p.y<=11.8801f);
    s.seed=789;auto variant=Build(s);assert(variant.mesh.v.size()!=a.mesh.v.size()||std::memcmp(variant.mesh.v.data(),a.mesh.v.data(),a.mesh.v.size()*sizeof(Vertex))!=0);valid(variant);
    Part top;face(top,{{-20,12,0},{-20,12,20},{20,12,20},{20,12,0}},1,1);assert(Build(surface(top)).rocks==0);
    Part under;face(under,{{-10,100,0},{10,100,0},{10,100,20},{-10,100,20}},1,1);auto floating=Build(surface(under));assert(floating.rocks>0);valid(floating);
    bool below=false;for(auto p:floating.mesh.v){assert(p.y>75&&p.y<110);below|=p.y<100;}assert(below);
    Model rockModel;rock(rockModel,{0,0,0},{1.5f,1.4f,1.3f},123,false,0,false);auto small=Build(surface(rockModel.stone));assert(!small.cliff&&small.rocks>=3);valid(small);
    // Equivalent indexed list / fan / strip have valid stable dressing.
    Part plane;face(plane,{{-5,0,0},{-5,8,0},{5,8,0},{5,0,0}},1,1);
    uint16_t fan[]={0,1,2,3},strip[]={0,1,3,2};auto fanSurface=surface(plane);fanSurface.indices=fan;fanSurface.indexCount=4;fanSurface.primitive=6;
    auto stripSurface=fanSurface;stripSurface.indices=strip;stripSurface.primitive=5;
    auto fanBuild=Build(fanSurface),stripBuild=Build(stripSurface);assert(fanBuild.rocks&&stripBuild.rocks);valid(fanBuild);valid(stripBuild);
    auto baked=surface(wall);baked.normalOffset=-1;auto bakedBuild=Build(baked);assert(bakedBuild.rocks);valid(bakedBuild);
    for(auto p:bakedBuild.mesh.v)assert((p.diffuse>>24)==255);
    auto invalid=surface(wall);invalid.up={0,0,0};assert(Build(invalid).rocks==0);invalid=surface(wall);invalid.scale=0;assert(Build(invalid).rocks==0);
    invalid=surface(wall);invalid.strength=std::numeric_limits<float>::quiet_NaN();assert(Build(invalid).rocks==0);
    Part corrupt=wall;corrupt.v[0].x=std::numeric_limits<float>::infinity();assert(Build(surface(corrupt)).rocks==0);
    Part degenerate;degenerate.v.assign(3,{});degenerate.ix={0,1,2};assert(Build(surface(degenerate)).rocks==0);
    s=surface(wall);s.strength=0;assert(Build(s).rocks==0);
    s=surface(wall);s.scale=25;auto giant=Build(s);assert(giant.rocks>0);valid(giant);
    // Local attachment: moving the source up moves every generated vertex by
    // the same amount; there is no ground lookup or camera argument.
    Part raised=wall;for(auto& p:raised.v)p.y+=100;auto high=Build(surface(raised));assert(high.rocks==a.rocks&&high.mesh.v.size()==a.mesh.v.size());
    for(size_t i=0;i<a.mesh.v.size();++i){assert(std::fabs(high.mesh.v[i].x-a.mesh.v[i].x)<.001f);assert(std::fabs(high.mesh.v[i].y-a.mesh.v[i].y-100)<.001f);assert(std::fabs(high.mesh.v[i].z-a.mesh.v[i].z)<.001f);}
    std::printf("passed: rigid closed clusters; %u cliff rocks / %zu triangles; deterministic, floating, caps, topology, budgets, invalid input\n",a.rocks,a.mesh.ix.size()/3);
    auto start=std::chrono::steady_clock::now();for(int i=0;i<100;++i){s.seed=i+1;auto fixture=Build(s);assert(fixture.rocks>0);valid(fixture);}
    std::printf("100 unique cliff builds + closed-edge validation: %.1f ms; catalog identities %zu\n",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),std::size(rvk::materials::rockCatalog));
}
