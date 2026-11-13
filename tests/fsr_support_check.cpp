#include "fsr_support.h"
#include <cstdio>
#include <initializer_list>
int main() {
    using namespace rvk::detail;
    const FsrHardware good{true,true,true,true,true,256,256,16,4,32,3};
    unsigned checks=0;
    auto expect=[&](FsrHardware c,bool fsr2,bool fg,bool supported) {
        ++checks; return (FsrUnsupported(c,fsr2,fg)==nullptr)==supported;
    };
    for(auto size:{16u,32u,64u}) {
        auto c=good;c.subgroupSize=size;
        if(!expect(c,false,false,true)||!expect(c,true,false,size!=16)||!expect(c,false,true,true)) return 1;
    }
    for(unsigned field=0;field<11;++field) {
        auto c=good;
        switch(field) {
        case 0:c.computeSubgroups=false;break;case 1:c.quad=false;break;
        case 2:c.extendedStorage=false;break;case 3:c.invocations=128;break;
        case 4:c.workgroupX=128;break;case 5:c.workgroupY=4;break;
        case 6:c.arithmetic=false;break;case 7:c.timeline=false;break;
        case 8:c.queues=1;break;case 9:c.queues=2;break;case 10:c.workgroupZ=2;break;
        }
        if(!expect(c,false,true,false)||!expect(c,false,false,field>=6&&field<=9)) return 2;
    }
    std::printf("FSR hardware requirement profiles: %u passed (simulated capabilities, not physical GPUs)\n",checks);
}
