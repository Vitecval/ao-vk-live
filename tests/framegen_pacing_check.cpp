#include "FrameInterpolationSwapchainVK_Pacing.h"
#include <cstdio>

int main() {
    struct Case {int64_t previous,ready,complete;bool serial;int64_t expected;};
    const Case cases[]={{0,100,0,true,0},{100,90,110,true,0},{100,180,140,true,40},
        {100,180,140,false,80},{100,180,90,true,80},{100,180,190,true,80},
        {100,180,100,true,80},{100,180,180,true,0},{100,180,0,true,80}};
    for(const auto& c:cases)if(frameProductionInterval(c.previous,c.ready,c.complete,c.serial)!=c.expected)return 1;
    if(frameProductionInterval(100,180,160,true,60)!=60 ||
       frameProductionInterval(100,180,160,true,200)!=80 ||
       frameProductionInterval(100,180,160,false,60)!=80 ||
       frameProductionInterval(100,180,160,true,-1)!=20)return 3;
    // Constant 40 ms production with serialized pairs must keep a 20 ms half
    // frame target, rather than converging toward 40 ms through positive feedback.
    int64_t previous=1000,half=20;
    for(unsigned i=0;i<1000;++i){
        const int64_t complete=previous+half,ready=complete+40;
        half=frameProductionInterval(previous,ready,complete,true)/2;
        if(half!=20)return 2;
        previous=ready;
    }
    std::puts("PASS: serialized pacing excludes previous pair; stock/stale/reset timing and steady production remain bounded");
}
