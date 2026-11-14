#include "lightning_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace rvk::lightning;
    for(auto name:{"VisualCloudlight_t","VisualLightning_t<RRefFrame_t","VisualSingleCloud_t<RRefFrame_t"})assert(WeatherVisual(name));
    for(auto name:{"","RTriMesh_t","VisualSingleCloud_t_extra","RCATMesh_t"})assert(!WeatherVisual(name));
    assert(SkipLegacyGlow(true,1,true,0)); // automatic storms must not draw both flash paths
    assert(!SkipLegacyGlow(true,0,true,0)); // retain the game's effect without replacement
    assert(!SkipLegacyGlow(false,1,true,0)); // original mode
    assert(SkipLegacyGlow(true,0,false,0));
    assert(SkipLegacyGlow(true,0,true,1));
    std::puts("Lightning: one flash path, legacy fallback, original mode and weather visual eligibility passed");
}
