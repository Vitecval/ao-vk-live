#include "rock_topology.h"
#include <cassert>
#include <cstdio>
int main(){
    struct V{float x,y,z;};
    const V quad[]={{0,0,0},{1,0,0},{0,1,0},{1,1,0}};
    const uint16_t ix[]={0,1,2,2,1,3};
    auto flags=rvk::rocks::Borders(quad,4,sizeof(V),0,ix,6);
    assert(flags.size()==2&&flags[0]==6&&flags[1]==3); // diagonal is not pinned
    const V split[]={{0,0,0},{1,0,0},{0,1,0},{0,1,0},{1,0,0},{1,1,0}};
    assert(rvk::rocks::Borders(split,6,sizeof(V),0,nullptr,0)==flags);
    struct VN{float x,y,z,nx,ny,nz;};
    const VN hard[]={{0,0,0,0,0,1},{1,0,0,0,0,1},{0,1,0,0,0,1},
        {0,1,0,0,0,1},{1,0,0,0,1,0},{1,1,0,0,1,0}};
    auto creases=rvk::rocks::Borders(hard,6,sizeof(VN),0,nullptr,0,12);
    assert((creases[0]&7)==6&&(creases[1]&7)==3&&(creases[0]&8)&&(creases[1]&32));
    const V tetra[]={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
    const uint16_t ti[]={0,2,1,0,1,3,1,2,3,2,0,3};
    for(auto f:rvk::rocks::Borders(tetra,4,sizeof(V),0,ti,12))assert(f==0);
    const uint16_t bad[]={0,1,9};assert(rvk::rocks::Borders(quad,4,sizeof(V),0,bad,3).empty());
    const uint16_t triple[]={0,1,2,0,1,2,0,1,2};
    for(auto f:rvk::rocks::Borders(quad,4,sizeof(V),0,triple,9))assert(f==7);
    for(uint32_t n=1;n<=8000;++n){float l=rvk::rocks::Level(n);assert(n*l*l<=8000);}
    assert(rvk::rocks::Level(8001)==0);
    std::puts("Rock topology: welded seams, open/non-manifold borders, index safety and tessellation bounds passed");
}
