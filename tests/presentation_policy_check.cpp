#include "presentation_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using rvk::presentation::SuspendForFocus;
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
