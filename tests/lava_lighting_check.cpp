#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/lava_lighting.h"
#include "rvk/area_profiles.h"
#include "rvk/terrain_materials.h"
#include "rvk/environment_materials.h"
#include <cassert>
#include <cstdio>
struct Vertex{float pos[3];};
int main(){
    using namespace rvk;
    assert(environment::RetainLegacyFunctionalOriginal(1010004,292915));
    assert(!environment::RetainLegacyFunctionalOriginal(1010006,292915));
    assert(!environment::RetainLegacyFunctionalOriginal(1010004,292914));
    assert(areas::For(4005)==areas::Biome::Volcanic);
    assert(areas::For(4605)==areas::Biome::Volcanic);
    assert(areas::For(4676)==areas::Biome::Forest);
    for(uint32_t id:{437u,439u,442u,742u,747u,752u,758u})assert(materials::SnowGround(1010006,id));
    assert(materials::SnowGround(1010006,692)); // original white snow patch over cyan ice
    assert(materials::SnowGround(1010006,701));
    assert(!materials::SnowGround(1010006,697));
    assert(!materials::SnowGround(1010006,706)); // bare faceted ice
    assert(!materials::SnowGround(1010006,710));
    assert(materials::IceGround(1010006,692) && materials::IceGround(1010006,710));
    assert(!materials::IceGround(1010006,437) && !materials::IceGround(1010006,697));
    assert(materials::AuthoredGround(1010006,703) && materials::AuthoredGround(1010006,936));
    assert(!materials::AuthoredGround(1010006,706) && !materials::AuthoredGround(1010006,930));
    assert(!materials::SnowGround(1010006,774)); // void marker
    assert(!materials::SnowGround(1010004,437));
    Vertex v[]={{{-36,0,-36}},{{36,0,-36}},{{-36,0,36}}};
    uint16_t ix[]={0,1,2};float eye[]={0,3,0};
    auto points=lava::Samples(v,3,ix,3,eye);
    assert(points.size()==8);
    for(const auto& p:points){assert(p[0]>=-36 && p[2]>=-36 && p[0]+p[2]<=0);assert(p[1]==1.5f);}
    auto second=lava::Samples(v,3,ix,3,eye);assert(second==points);
    uint16_t invalid[]={0,1,7};assert(lava::Samples(v,3,invalid,3,eye).empty());
    Vertex upright[]={{{0,0,0}},{{0,20,0}},{{0,0,20}}};assert(lava::Samples(upright,3,ix,3,eye).empty());
    float above[]={0,200,0};assert(lava::Samples(v,3,ix,3,above).empty());
    assert(lava::Samples<Vertex>(nullptr,0,ix,3,eye).empty());
    std::puts("Lava classification, triangle containment, stable sampling, vertical/invalid/distant rejection passed.");
}
