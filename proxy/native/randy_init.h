// Randy_t::Initialize natively (randy_init.cpp), installed with the device layer ([Native] Device=on).
#pragma once

#include "native/native.h"
#include "native/display_layout_policy.h"

namespace rnative::randyinit {

void Install(HMODULE orig);
void DrainPendingResize(); // safe frame boundary only; never from a window callback
HWND DisplayWindow(); // existing AO window; never creates or changes DPI awareness
displaylayout::Layout StartupLayout();
bool IsMainSurface(void* surface); // original primary or target0 colour wrapper
bool DeferResize(void* randy,uint32_t width,uint32_t height); // DPI validation and atomic publish; no COM/GPU work
bool OwnsRandy(void* randy);
bool ResizeDisplayEligible(); // DPI gate before native layout/queue/target mutation

}  // namespace rnative::randyinit
