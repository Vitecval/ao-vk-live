#ifdef NDEBUG
#undef NDEBUG
#endif
#include "presentation_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

int main()
{
    using rvk::presentation::LightingContext;
    assert(LightingContext(0,-1,false)==1); // fresh start before N3 engine exists
    assert(LightingContext(0,0,true)==1);
    assert(LightingContext(1,0,false)==1);
    assert(LightingContext(0,1,true)==1);
    assert(LightingContext(2,1,true)==2);
    assert(LightingContext(1,2,true)==1);
    assert(LightingContext(0,2,true)==2);
    assert(LightingContext(0,1,false)==1);
    assert(LightingContext(0,2,false)==1); // a replacement device has no prior world
    int context=-1;
    const int reported[]={0,0,1,2,0,2,1,0,1};
    const int expectedContext[]={1,1,1,2,2,2,1,1,1};
    for(unsigned i=0;i<9;++i) {
        context=LightingContext(reported[i],context,true);
        assert(context==expectedContext[i]);
    }
    using rvk::presentation::SuspendForFocus;
    assert(!rvk::presentation::UsableClient(154,10));
    assert(!rvk::presentation::UsableClient(0,0));
    assert(rvk::presentation::UsableClient(640,480));
    assert(!SuspendForFocus(true,false,false,false)); // launcher, login, character selection
    assert(SuspendForFocus(true,false,false,true));   // logged-in alt account
    assert(!SuspendForFocus(true,true,false,true));  // active character
    assert(!SuspendForFocus(true,false,true,true));  // explicit background rendering
    assert(!SuspendForFocus(false,false,false,true)); // minimization handled separately
    const bool characterPresent[]={false,false,true,true,false,false,true,false};
    const bool expected[]={false,false,true,true,false,false,true,false};
    for(unsigned i=0;i<8;++i)assert(SuspendForFocus(true,false,false,characterPresent[i])==expected[i]);
    std::puts("Presentation: login stays live; background restriction follows the current local character, focus and preference; logout resumes");

    using namespace rvk::presentation;
    const auto native=AspectFit(3840,2160,3840,2160);
    assert(native.x==0 && native.y==0 && native.width==3840 && native.height==2160);
    const auto wide=AspectFit(1920,1080,2560,1080);
    assert(wide.x==320 && wide.y==0 && wide.width==1920 && wide.height==1080);
    const auto tall=AspectFit(3440,1440,1920,1080);
    assert(tall.x==0 && tall.y==138 && tall.width==1920 && tall.height==803);
    assert(!AspectFit(0,1080,1920,1080).width);
    assert(!ToFramebuffer(0,0,1920,1080,0,0).inside);
    assert(!ToFramebuffer(319,100,1920,1080,2560,1080).inside);
    assert(ToFramebuffer(320,0,1920,1080,2560,1080).inside);
    assert(!ToFramebuffer(2240,100,1920,1080,2560,1080).inside);
    for(const auto size : {Rect{0,0,1920,1080},Rect{0,0,2560,1440},Rect{0,0,3840,2160},Rect{0,0,3440,1440}}) {
        for(uint32_t scale : {50u,67u,75u,85u,100u}) {
            const auto s=Targets(size.width,size.height,scale,16384,16384,16384);
            assert(s.valid && s.outputW==size.width && s.outputH==size.height);
            assert(s.sceneW==(uint64_t(size.width)*scale+50)/100);
            for(const auto client : {Rect{0,0,1920,1080},Rect{0,0,1080,1920},Rect{0,0,2559,1439}}) {
                for(double f : {.001,.25,.5,.999}) {
                    const auto screen=ToClient(size.width*f,size.height*f,size.width,size.height,client.width,client.height);
                    const auto back=ToFramebuffer(screen.x,screen.y,size.width,size.height,client.width,client.height);
                    assert(screen.inside && back.inside);
                    assert(std::abs(back.x-size.width*f)<1e-8 && std::abs(back.y-size.height*f)<1e-8);
                }
            }
        }
    }
    assert(!Targets(3840,2160,100,2048,8192,8192).valid); // output, even if reduced scene fits
    assert(!Targets(3840,2160,50,2048,8192,8192).valid);
    assert(!Targets(3840,2160,100,8192,3000,8192).valid);
    assert(!Targets(3840,2160,100,8192,8192,2000).valid);
    assert(!Targets(UINT32_MAX,UINT32_MAX,100,UINT32_MAX,UINT32_MAX,UINT32_MAX).valid);
    assert(!Targets(1920,1080,200,16384,16384,16384).valid);
    assert(!Targets(0,1080,100,16384,16384,16384).valid);
    std::puts("Presentation dimensions, output/scene GPU limits, overflow, letterbox inverse input, native aspect checks passed");
}
