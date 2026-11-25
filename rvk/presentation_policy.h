#pragma once

#include <cstdint>
#include <limits>

namespace rvk::presentation {
// A fresh device starts in the login hall before the engine query is ready.
// Unknown is not a transition: retain a confirmed context on the same device.
constexpr int LightingContext(int reported,int previous,bool sameDevice){
    return reported ? reported : sameDevice && previous==2 ? 2:1;
}
// Windows can report the minimized icon's 154x10 client rectangle while AO is
// changing styles. Neither that rectangle nor SIZE_MINIMIZED is a restore.
constexpr bool UsableClient(unsigned width,unsigned height){return width>=32&&height>=32;}
// Login/character selection must keep presenting even with the default-off
// background preference. Unknown session state follows that same live policy.
constexpr bool SuspendForFocus(bool drawable,bool focused,bool renderInBackground,bool hasLocalCharacter){
    return drawable&&!focused&&!renderInBackground&&hasLocalCharacter;
}


struct Rect { uint32_t x=0,y=0,width=0,height=0; };

// Integer arithmetic avoids overflow and gives rendering and input exactly the
// same centred rectangle. The remaining odd pixel stays on the right/bottom.
constexpr Rect AspectFit(uint32_t sourceW,uint32_t sourceH,uint32_t destinationW,uint32_t destinationH)
{
    if(!sourceW || !sourceH || !destinationW || !destinationH)return {};
    uint32_t w=destinationW,h=destinationH;
    if(uint64_t(destinationW)*sourceH>uint64_t(destinationH)*sourceW)
        w=uint32_t(uint64_t(destinationH)*sourceW/sourceH);
    else h=uint32_t(uint64_t(destinationW)*sourceH/sourceW);
    // An extreme but nonzero aspect ratio still needs a drawable pixel.
    if(!w)w=1;
    if(!h)h=1;
    return {(destinationW-w)/2,(destinationH-h)/2,w,h};
}

struct Point { double x=0,y=0; bool inside=false; };

inline Point ToFramebuffer(double clientX,double clientY,uint32_t framebufferW,uint32_t framebufferH,
                           uint32_t clientW,uint32_t clientH)
{
    const Rect r=AspectFit(framebufferW,framebufferH,clientW,clientH);
    if(!r.width || !r.height)return {};
    return {(clientX-r.x)*framebufferW/r.width,(clientY-r.y)*framebufferH/r.height,
        clientX>=r.x && clientY>=r.y && clientX<double(r.x)+r.width && clientY<double(r.y)+r.height};
}

inline Point ToClient(double framebufferX,double framebufferY,uint32_t framebufferW,uint32_t framebufferH,
                      uint32_t clientW,uint32_t clientH)
{
    const Rect r=AspectFit(framebufferW,framebufferH,clientW,clientH);
    if(!r.width || !r.height)return {};
    return {r.x+framebufferX*r.width/framebufferW,r.y+framebufferY*r.height/framebufferH,
        framebufferX>=0 && framebufferY>=0 && framebufferX<framebufferW && framebufferY<framebufferH};
}

struct TargetSizes { uint32_t outputW=0,outputH=0,sceneW=0,sceneH=0; bool valid=false; };

constexpr TargetSizes Targets(uint32_t w,uint32_t h,uint32_t scale,uint32_t imageLimit,
                              uint32_t framebufferWidthLimit,uint32_t framebufferHeightLimit)
{
    if(!w || !h || scale<50 || scale>100 || !imageLimit || !framebufferWidthLimit || !framebufferHeightLimit)return {};
    const uint64_t sw=(uint64_t(w)*scale+50)/100,sh=(uint64_t(h)*scale+50)/100;
    const uint32_t signedLimit=uint32_t(std::numeric_limits<int32_t>::max());
    const auto fits=[&](uint64_t width,uint64_t height) {
        return width && height && width<=imageLimit && height<=imageLimit &&
            width<=framebufferWidthLimit && height<=framebufferHeightLimit && width<=signedLimit && height<=signedLimit;
    };
    return fits(w,h) && fits(sw,sh) ? TargetSizes{w,h,uint32_t(sw),uint32_t(sh),true}:TargetSizes{};
}

} // namespace rvk::presentation
