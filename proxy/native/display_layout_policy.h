#pragma once
#include <cstdint>
#include <cmath>
namespace rnative::displaylayout {
struct Layout {
    bool active=false;
    uint32_t requestedWidth=0,requestedHeight=0,originalClientWidth=0,originalClientHeight=0;
    uint32_t actualWidth=0,actualHeight=0,previousWidth=0,previousHeight=0,clientWidth=0,clientHeight=0;
};
struct Rect {uint32_t x=0,y=0,width=0,height=0;};
inline bool FullMain(const Layout& layout,bool mainTarget,Rect rect) {
    if(!layout.active || !mainTarget || rect.x || rect.y || !rect.width || !rect.height)return false;
    auto pair=[&](uint32_t w,uint32_t h){return w && h && rect.width==w && rect.height==h;};
    return pair(layout.requestedWidth,layout.requestedHeight) || pair(layout.originalClientWidth,layout.originalClientHeight) ||
        pair(layout.actualWidth,layout.actualHeight) || pair(layout.previousWidth,layout.previousHeight) || pair(layout.clientWidth,layout.clientHeight);
}
inline Rect Effective(const Layout& layout,bool mainTarget,Rect rect) {
    if(FullMain(layout,mainTarget,rect) && layout.actualWidth && layout.actualHeight) {
        rect.width=layout.actualWidth;rect.height=layout.actualHeight;
    }
    return rect;
}
struct Window {float left,top,right,bottom,aspect;};
inline Window PreserveHorizontal(Window source,uint32_t width,uint32_t height) {
    if(!width || !height || !std::isfinite(source.left) || !std::isfinite(source.right) ||
       !std::isfinite(source.top) || !std::isfinite(source.bottom))return source;
    float vertical=(source.right-source.left)*float(height)/float(width);
    float center=(source.top+source.bottom)*.5f;
    if(vertical<=0 || !std::isfinite(vertical))return source;
    source.top=center+vertical*.5f;source.bottom=center-vertical*.5f;source.aspect=float(width)/float(height);
    return source;
}
} // namespace rnative::displaylayout
