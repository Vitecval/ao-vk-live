#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/ship_shadow_policy.h"
#include <cassert>
#include <cstdio>

int main(){
    using namespace rvk::ship;
    unsigned checked=0;
    // Exhaust the established SL family, not merely its snow/volcanic biome
    // labels: gardens, cities, coast and unknown SL cells are excluded too.
    for(uint32_t zone=4000;zone<5000;++zone){
        for(bool indoors:{false,true}){
            assert(!EnvironmentAllowed(zone,indoors));
            assert(!ImmersiveEnabled(1.f,zone,indoors));
            assert(TrafficDensity(2.f,zone,indoors)==0.f);
            ++checked;
        }
    }
    for(uint32_t zone:{386u,500u,551u,566u,655u,800u,3999u,5000u,6010u,6551u}){
        assert(EnvironmentAllowed(zone,false));
        assert(ImmersiveEnabled(1.f,zone,false));
        assert(!ImmersiveEnabled(0.f,zone,false));
        assert(!ImmersiveEnabled(.5f,zone,false));
        assert(TrafficDensity(1.f,zone,false)==1.f);
        assert(TrafficDensity(2.f,zone,false)==2.f);
        assert(TrafficDensity(3.f,zone,false)==2.f);
        assert(TrafficDensity(-1.f,zone,false)==0.f);
        assert(!EnvironmentAllowed(zone,true));
        assert(!ImmersiveEnabled(1.f,zone,true));
        assert(TrafficDensity(2.f,zone,true)==0.f);
        ++checked;
    }
    assert(!EnvironmentAllowed(0,false));
    assert(TrafficDensity(2.f,0,false)==0.f);
    // A zone transition changes eligibility without mutating the saved choice.
    const float savedImmersive=1.f,savedTraffic=2.f;
    for(uint32_t zone:{655u,4320u,4676u,4873u,4005u,655u}){
        const bool sl=zone>=4000&&zone<5000;
        assert(ImmersiveEnabled(savedImmersive,zone,false)==!sl);
        assert(TrafficDensity(savedTraffic,zone,false)==(sl?0.f:2.f));
    }
    assert(savedImmersive==1.f&&savedTraffic==2.f);
    // Hull classification/shadow safety remain independent of the theme gate.
    assert(Hull(1010004,99841));assert(!Hull(1010004,655));
    assert(LocalCube(32.f,256));assert(!LocalCube(1000.f,256));
    std::printf("ship_environment_check passed: %u environments; SL ships/traffic off; RK choices retained\n",checked);
}
