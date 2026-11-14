#pragma once
#include <cmath>
namespace rnative::camera {
// A sphere intersecting the camera/near plane has no bounded perspective
// rectangle. Let the frustum tests decide visibility; never feed a flipped or
// infinite centre-projected rectangle to the terrain occluder.
inline bool SafeOcclusionProjection(float depth,float radius,float nearPlane) {
    return std::isfinite(depth)&&std::isfinite(radius)&&std::isfinite(nearPlane)&&radius>=0 &&
        depth-radius>std::fmax(nearPlane,0.f)+std::fmax(1.f,std::fabs(depth))*1e-5f;
}
}

namespace rnative::occluder {
// A height sample behind the near plane cannot establish a screen horizon.
// Dividing its negative depth flips ground below the eye above the scene;
// clamping that result to +1 would hide every building in that column.
inline bool ProjectHorizonSample(const float p[3],float nearPlane,float aspect,float& x,float& y) {
    if(!std::isfinite(p[0])||!std::isfinite(p[1])||!std::isfinite(p[2])||
       !std::isfinite(nearPlane)||nearPlane<0||!std::isfinite(aspect)||aspect<=0||p[2]<=0)return false;
    const float depth=p[2]+nearPlane;
    x=p[0]/depth;y=(p[1]/depth)/aspect;
    return std::isfinite(x)&&std::isfinite(y);
}
}
