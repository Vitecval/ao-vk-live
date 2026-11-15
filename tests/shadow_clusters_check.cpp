#include "shadow_clusters.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <array>
int main(){
    std::vector<std::array<float,4>> vertices;
    std::vector<uint16_t> indices;
    for(int z=0;z<=32;++z)for(int x=0;x<=32;++x)vertices.push_back({float(x*4),float((x+z)%7),float(z*4),17.f});
    for(int z=0;z<32;++z)for(int x=0;x<32;++x){uint16_t a=uint16_t(z*33+x);indices.insert(indices.end(),{a,uint16_t(a+33),uint16_t(a+1),uint16_t(a+1),uint16_t(a+33),uint16_t(a+34)});}
    const auto ranges=rvk::shadow::TerrainClusters(vertices.data(),uint32_t(vertices.size()),sizeof(vertices[0]),indices.data(),uint32_t(indices.size()));
    assert(ranges.size()==22);uint32_t next=0;
    for(const auto& r:ranges){
        assert(r.first==next&&r.count%3==0&&r.count<=288);
        for(uint32_t i=r.first;i<r.first+r.count;++i)for(int j=0;j<3;++j){float v=vertices[indices[i]][j];assert(v>=r.lo[j]&&v<=r.hi[j]);}
        next+=r.count;
    }
    assert(next==indices.size());
    auto bad=indices;bad[10]=65535;
    assert(rvk::shadow::TerrainClusters(vertices.data(),uint32_t(vertices.size()),16,bad.data(),uint32_t(bad.size())).empty());
    const float old=vertices[0][0];vertices[0][0]=std::numeric_limits<float>::quiet_NaN();
    assert(rvk::shadow::TerrainClusters(vertices.data(),uint32_t(vertices.size()),16,indices.data(),uint32_t(indices.size())).empty());vertices[0][0]=old;
    assert(rvk::shadow::TerrainClusters(vertices.data(),uint32_t(vertices.size()),16,indices.data(),6).empty());
    assert(rvk::shadow::TerrainClusters(nullptr,100,16,indices.data(),uint32_t(indices.size())).empty());
    std::puts("PASS: clusters cover every original index once, contain all triangle corners, and reject invalid inputs.");
}
