#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/area_profiles.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {
using RGB=std::array<float,3>;
float Luma(const RGB& c){return .3f*c[0]+.59f*c[1]+.11f*c[2];}
RGB MixToOwnLuma(RGB source,float amount){float y=Luma(source);for(auto& c:source)c+=(y-c)*amount;return source;}
float Chroma(const RGB& c){return *std::max_element(c.begin(),c.end())-*std::min_element(c.begin(),c.end());}
void Near(float a,float b,float epsilon=2e-7f){assert(std::abs(a-b)<epsilon);}
void Equal(const float (&actual)[3],const RGB& expected){for(unsigned c=0;c<3;++c)assert(actual[c]==expected[c]);}
}
int main(){
    using namespace rvk::areas;
    for(uint32_t id=0;id<7000;++id){
        const bool inferno=id==4005||id==4605;
        assert((LegacyAmbientNeutralization(id)>0)==inferno);
        if(inferno)Near(LegacyAmbientNeutralization(id),.85f);
        else assert(LegacyAmbientNeutralization(id)==0);
    }
    // Existing regional palettes are pinned: warmth comes only from retaining
    // five additional percentage points of actual Inferno ambient colour.
    auto volcanic=Lighting(4005);
    Equal(volcanic.sky,{.98f,1.f,1.03f});Equal(volcanic.ground,{1.01f,.99f,.96f});
    Equal(volcanic.sun,{1.035f,1.f,.97f});assert(volcanic.sideFill==.008f);
    auto garden=Lighting(4676);Equal(garden.sky,{.86f,1.01f,.96f});Equal(garden.sun,{1.025f,1.f,.96f});
    auto snow=Lighting(4006);Equal(snow.sky,{.88f,.98f,1.12f});Equal(snow.sun,{1.02f,1.f,.97f});
    auto desert=Lighting(560);Equal(desert.sky,{.91f,.99f,1.10f});
    auto city=Lighting(566);Equal(city.sky,{.91f,.98f,1.06f});

    // Saturated/neutral/dim input grid: convex desaturation must preserve the
    // exact source energy, never brighten/clip, and retain bounded chroma.
    for(float r:{0.f,.01f,.25f,1.f})for(float g:{0.f,.01f,.25f,1.f})for(float b:{0.f,.01f,.25f,1.f}){
        RGB source{r,g,b};auto old=MixToOwnLuma(source,.90f);auto candidate=MixToOwnLuma(source,LegacyAmbientNeutralization(4005));
        Near(Luma(source),Luma(old));Near(Luma(source),Luma(candidate));
        Near(Chroma(candidate),.15f*Chroma(source));
        Near(Chroma(candidate)-Chroma(old),.05f*Chroma(source));
        for(unsigned c=0;c<3;++c){assert(candidate[c]>=0&&candidate[c]<=1);assert(std::abs(candidate[c]-old[c])<=.05f+1e-7f);}
        assert(MixToOwnLuma(source,LegacyAmbientNeutralization(4676))==source);
    }
    // Real captured Inferno ambient 0xA23232, at area strengths0,.3,.6.
    const RGB source{162/255.f,50/255.f,50/255.f};
    for(float area:{0.f,.3f,.6f}){
        const float weight=std::clamp(area/.6f,0.f,1.f);
        auto old=MixToOwnLuma(source,.90f*weight);
        auto candidate=MixToOwnLuma(source,LegacyAmbientNeutralization(4005)*weight);
        Near(Luma(candidate),Luma(old));
        if(area==0)assert(old==candidate&&candidate==source);
        if(area==.6f){assert(candidate[0]/candidate[1]<1.22f);assert(Chroma(candidate)<.067f);}
        std::printf("area %.1f oldRGB %.6f %.6f %.6f candidateRGB %.6f %.6f %.6f Y %.6f/%.6f chroma %.6f/%.6f\n",
            area,old[0],old[1],old[2],candidate[0],candidate[1],candidate[2],Luma(old),Luma(candidate),Chroma(old),Chroma(candidate));
    }
    // A neutral rock's ambient remains neutral; neither lava RGB/emission nor
    // directional lights pass through this regional ambient coefficient.
    for(float grey:{0.f,.01f,.25f,1.f}){
        auto neutral=MixToOwnLuma({grey,grey,grey},LegacyAmbientNeutralization(4005));
        for(float c:neutral)Near(c,grey);
    }
    std::puts("Inferno warmth: source luminance preserved, residual chroma bounded, other regions/palettes unchanged.");
}
