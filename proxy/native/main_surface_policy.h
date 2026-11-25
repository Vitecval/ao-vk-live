#pragma once
#include <utility>

namespace rnative::framebuffer {
template<class Publish>
bool RetainCallbackResize(bool callback,bool owned,Publish&& publish) {
    if(!callback || !owned)return false;
    publish();
    return true; // even a rejected/zero-sized request cannot enter COM here
}

// A native viewport retains a surface_t*, not its DirectDraw interface. Keep
// that wrapper (and its native user count) alive across a successful resize.
// Each wrapper continues owning exactly one interface lease; only the backing
// interfaces change owners. Invoke only after candidate allocation and bind.
template<class Surface,class ExchangeBacking>
void PreserveMainSurface(Surface*& previous,Surface*& candidate,ExchangeBacking&& exchange) {
    if(!previous || !candidate || previous==candidate)return;
    exchange(previous,candidate);
    std::swap(previous,candidate);
}

// Full main viewports describe the committed framebuffer. In windowed mode
// their primary surface descriptor can instead describe an older monitor size.
template<class Size>
Size MainViewportSize(bool fullMain,Size requested,Size framebuffer,Size surface) {
    if(fullMain && framebuffer.width && framebuffer.height)return framebuffer;
    requested.width=requested.width<=surface.width ? requested.width:surface.width;
    requested.height=requested.height<=surface.height ? requested.height:surface.height;
    return requested;
}
} // namespace rnative::framebuffer
