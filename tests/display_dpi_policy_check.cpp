#ifdef NDEBUG
#undef NDEBUG
#endif
#include "proxy/display_dpi_policy.h"
#include <cassert>
#include <cstdio>

int main()
{
    using namespace rvk::display::dpi;
    Observation o{Awareness::Unaware,1920,1080,1920,1080,96,100,true,true};
    assert(Evaluate(o).nativeOverride);
    o.monitorScalePercent=150;assert(!Evaluate(o).nativeOverride);
    o.physicalWidth=2880;o.physicalHeight=1620;assert(!Evaluate(o).nativeOverride);
    o.awareness=Awareness::PerMonitor;assert(Evaluate(o).nativeOverride);
    o.awareness=Awareness::System;o.physicalWidth=1920;o.physicalHeight=1080;
    assert(!Evaluate(o).nativeOverride);
    o.windowDpi=144;assert(Evaluate(o).nativeOverride);
    o.monitorScalePercent=100;assert(!Evaluate(o).nativeOverride);
    o.windowDpi=96;assert(Evaluate(o).nativeOverride);
    o.monitorScalePercent=120;o.windowDpi=115;assert(Evaluate(o).nativeOverride);
    o.physicalKnown=false;assert(!Evaluate(o).nativeOverride);
    o.physicalKnown=true;o.monitorScaleKnown=false;assert(!Evaluate(o).nativeOverride);
    o.monitorScaleKnown=true;o.awareness=Awareness::Unknown;assert(!Evaluate(o).nativeOverride);
    o.awareness=Awareness::PerMonitor;o.logicalHeight=0;assert(!Evaluate(o).nativeOverride);
    std::puts("DPI native override checks: unaware 100/150%, scaled physical client, system-DPI monitor changes, PM windows and unavailable evidence passed");
}
