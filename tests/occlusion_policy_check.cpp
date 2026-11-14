#include "native/occlusion_policy.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <limits>
int main(){
    using rnative::camera::SafeOcclusionProjection;
    assert(SafeOcclusionProjection(50,10,.1f));
    assert(!SafeOcclusionProjection(0,10,.1f));
    assert(!SafeOcclusionProjection(-5,10,.1f));
    assert(!SafeOcclusionProjection(10.05f,10,.1f));
    assert(!SafeOcclusionProjection(10.1f,10,.1f));
    assert(SafeOcclusionProjection(10.2f,10,.1f));
    assert(!SafeOcclusionProjection(std::numeric_limits<float>::quiet_NaN(),10,.1f));
    // Sweep a tall object past the eye as the camera tilts upward: projection
    // is used only while the entire sphere stays in front of the near plane.
    for(int degrees=-89;degrees<=89;++degrees){float a=degrees*3.14159265f/180.f;
        float depth=20*std::cos(a)-35*std::sin(a);
        if(SafeOcclusionProjection(depth,30,.1f))assert(depth-30>.1f);
        else if(depth-30<=.1f)assert(!SafeOcclusionProjection(depth,30,.1f));}
    using rnative::occluder::ProjectHorizonSample;
    float sx=0,sy=0;
    const float front=.1f,farPlane=500.f,q=farPlane/(farPlane-front);
    bool reproduced=false;
    // Real follow-camera case: 10 m over flat ground, first sweep at 4 m.
    // The old divide/clamp abruptly changes a below-screen ground horizon to
    // the top edge near 22 degrees up, falsely hiding a building ahead.
    for(int pitch=0;pitch<=59;++pitch){
        const float angle=pitch*3.14159265f/180.f;
        const float z=4*std::cos(angle)-10*std::sin(angle);
        const float y=-4*std::sin(angle)-10*std::cos(angle);
        const float terrain[]={0,y,q*(z-front)};
        const bool usable=ProjectHorizonSample(terrain,front,1,sx,sy);
        if(z<=front)assert(!usable);else {assert(usable);assert(sy<0);}
        const float oldY=std::clamp((terrain[1]/(terrain[2]+front)),-1.f,1.f);
        const float buildingDepth=60*std::cos(angle)+10*std::sin(angle);
        const float buildingY=-60*std::sin(angle)+10*std::cos(angle);
        const float top=(buildingY+5)/(q*(buildingDepth-front));
        if(oldY==1 && top<oldY && SafeOcclusionProjection(buildingDepth,5,front)){
            assert(!usable);reproduced=true;
        }
    }
    assert(reproduced);
    const float valid[]={2,-3,10};assert(ProjectHorizonSample(valid,.1f,1.6f,sx,sy));
    assert(std::fabs(sx-2/10.1f)<1e-7f && std::fabs(sy-(-3/10.1f)/1.6f)<1e-7f);
    assert(!ProjectHorizonSample(valid,.1f,0,sx,sy));
    const float invalid[]={0,0,std::numeric_limits<float>::quiet_NaN()};
    assert(!ProjectHorizonSample(invalid,.1f,1,sx,sy));
    std::puts("Occlusion projection: camera/near-plane intersections remain visible; ordinary occlusion stays enabled; behind-camera terrain false occlusion reproduced and prevented");
}
