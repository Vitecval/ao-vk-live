#pragma once
#include <algorithm>
#include <cmath>
namespace rnative::camera {
inline float EffectiveRange(float view,float scenery) {
    view=std::isfinite(view) ? (std::clamp)(view,1.f,3.f):1.f;
    scenery=std::isfinite(scenery) ? (std::clamp)(scenery,1.f,3.f):1.f;
    return (std::max)(view,scenery);
}
inline float SceneryExtension(float view,float scenery) {
    return EffectiveRange(view,scenery)/(std::isfinite(view) ? (std::clamp)(view,1.f,3.f):1.f);
}
}
