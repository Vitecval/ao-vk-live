#pragma once
#include <string_view>
namespace rvk::lightning {
constexpr bool WeatherVisual(std::string_view name) {
    for(auto type:{std::string_view("VisualCloudlight_t"),std::string_view("VisualLightning_t"),std::string_view("VisualSingleCloud_t")})
        if(name==type || (name.starts_with(type) && name.size()>type.size() && name[type.size()]=='<'))return true;
    return false;
}
// Capture the original pulse for timing/position, then use the single bounded
// replacement. Legacy glow geometry is only needed without our own flash.
constexpr bool SkipLegacyGlow(bool weather,float flashStrength,bool skyFlash,float weatherMode){
    return weather&&(!skyFlash||flashStrength>0.f||weatherMode>.5f);
}
}
