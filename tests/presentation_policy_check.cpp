#include "presentation_policy.h"
#include <cassert>
#include <cstdio>
int main(){
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
}
