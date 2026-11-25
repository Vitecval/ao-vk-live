#pragma once
#include <cstdint>

namespace rvk::display::dpi {
enum class Awareness : int { Unknown=-1,Unaware=0,System=1,PerMonitor=2 };
struct Observation {
    Awareness awareness=Awareness::Unknown;
    uint32_t logicalWidth=0,logicalHeight=0,physicalWidth=0,physicalHeight=0;
    uint32_t windowDpi=0,monitorScalePercent=0;
    bool physicalKnown=false,monitorScaleKnown=false;
};
struct Decision { bool nativeOverride=false; const char* reason="DPI state unavailable"; };

// The policy never changes awareness. A window keeps the context it had when
// it was created; changing the caller's context cannot remove its bitmap scale.
constexpr Decision Evaluate(const Observation& o)
{
    if(!o.logicalWidth || !o.logicalHeight)return {false,"Display override deferred: zero client area"};
    if(o.awareness==Awareness::PerMonitor)return {true,"Per-monitor-aware native pixel window"};
    if(o.awareness==Awareness::Unknown || !o.physicalKnown || !o.monitorScaleKnown)
        return {false,"Display override unavailable: native DPI coordinates unconfirmed"};
    if(o.logicalWidth!=o.physicalWidth || o.logicalHeight!=o.physicalHeight)
        return {false,"Display override unavailable: legacy window is DPI bitmap scaled"};
    if(o.awareness==Awareness::Unaware && o.monitorScalePercent!=100)
        return {false,"Display override unavailable: DPI-unaware window on scaled monitor"};
    if(o.awareness==Awareness::System && (!o.windowDpi ||
        o.windowDpi!=(uint64_t(o.monitorScalePercent)*96+50)/100))
        return {false,"Display override unavailable: system-aware window on different monitor DPI"};
    return {true,"Legacy window has matching physical pixels on this monitor"};
}
} // namespace rvk::display::dpi
