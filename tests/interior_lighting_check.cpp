#include "interior_lighting.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
int main(){
    using rvk::lighting::InteriorScale;
    for(float range:{583.f,587.f})assert(InteriorScale(4625,2,range,.05f,.00002f)==.05f);
    for(uint32_t zone:{0u,4872u,4624u,4626u})assert(InteriorScale(zone,2,583,.05f,.00002f)==1);
    for(uint32_t type:{1u,3u})assert(InteriorScale(4625,type,583,.05f,.00002f)==1);
    for(float range:{6.f,40.f,500.f,1041.f})assert(InteriorScale(4625,2,range,.05f,.00002f)==1);
    assert(InteriorScale(4625,2,583,1,.00002f)==1);
    assert(InteriorScale(4625,2,583,.05f,.002f)==1);
    assert(InteriorScale(4625,2,std::numeric_limits<float>::quiet_NaN(),.05f,.00002f)==1);
    std::puts("Temple light family corrected; other zones, sun, ordinary lamps and invalid ranges unchanged");
}
