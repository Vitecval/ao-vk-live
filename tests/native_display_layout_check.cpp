#include "native/display_layout_policy.h"
#include "native/resize_queue_policy.h"
#include "window_callback_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>

int main() {
    assert(!rvk::presentation::InsideWindowCallback());
    {rvk::presentation::WindowCallbackScope first;
        assert(rvk::presentation::InsideWindowCallback());
        {rvk::presentation::WindowCallbackScope nested;assert(rvk::presentation::InsideWindowCallback());}
        assert(rvk::presentation::InsideWindowCallback());}
    assert(!rvk::presentation::InsideWindowCallback());
    using namespace rnative::displaylayout;
    Layout layout;layout.active=true;layout.requestedWidth=1280;layout.requestedHeight=720;
    layout.originalClientWidth=1274;layout.originalClientHeight=691;
    layout.actualWidth=3840;layout.actualHeight=2160;layout.previousWidth=2560;layout.previousHeight=1440;
    layout.clientWidth=3840;layout.clientHeight=2160;
    for(Rect asked:{Rect{0,0,1280,720},Rect{0,0,1274,691},Rect{0,0,2560,1440},Rect{0,0,3840,2160}}) {
        Rect actual=Effective(layout,true,asked);assert(actual.width==3840 && actual.height==2160);
        Rect offscreen=Effective(layout,false,asked);assert(offscreen.width==asked.width && offscreen.height==asked.height);
    }
    assert(Effective(layout,true,{100,0,1280,720}).width==1280);
    assert(Effective(layout,true,{0,0,1920,1080}).width==1920);
    layout.active=false;assert(Effective(layout,true,{0,0,1280,720}).width==1280);
    layout.active=true;layout.clientWidth=5120;layout.clientHeight=1440;
    assert(FullMain(layout,true,{0,0,5120,1440}));
    assert(Effective(layout,true,{0,0,5120,1440}).width==3840);
    layout.previousWidth=3840;layout.previousHeight=2160;layout.actualWidth=5120;layout.actualHeight=1440;
    assert(Effective(layout,true,{0,0,3840,2160}).width==5120);
    const Window game{-1.f,.5625f,1.f,-.5625f,16.f/9.f};
    for(auto dims:{Rect{0,0,1920,1080},Rect{0,0,2560,1440},Rect{0,0,3840,2160},Rect{0,0,5120,1440}}) {
        Window actual=PreserveHorizontal(game,dims.width,dims.height);
        assert(actual.left==game.left && actual.right==game.right);
        assert(std::fabs(actual.aspect-float(dims.width)/dims.height)<1e-6f);
        // World-label projection to output pixels and the matching pick ray.
        for(float x:{-.8f,0.f,.8f})for(float y:{-.15f,0.f,.15f}) {
            float px=(x-actual.left)/(actual.right-actual.left)*dims.width;
            float py=(actual.top-y)/(actual.top-actual.bottom)*dims.height;
            float pickX=(actual.right-actual.left)*(px/dims.width-.5f)+(actual.left+actual.right)*.5f;
            float pickY=(actual.top-actual.bottom)*(.5f-py/dims.height)+(actual.top+actual.bottom)*.5f;
            assert(std::fabs(pickX-x)<1e-6f && std::fabs(pickY-y)<1e-6f);
        }
    }
    auto unchanged=PreserveHorizontal(game,0,2160);assert(unchanged.aspect==game.aspect);
    using rnative::resizequeue::Queue;
    Queue queue;void* first=reinterpret_cast<void*>(uintptr_t(1));void* second=reinterpret_cast<void*>(uintptr_t(2));
    queue.Register(first);
    assert(queue.Enqueue(first,1920,1080));assert(queue.Enqueue(first,2560,1440));assert(queue.Enqueue(first,3840,2160));
    auto latest=queue.Take(first);assert(latest.owner==first && latest.width==3840 && latest.height==2160);
    assert(!queue.Take(first).owner && !queue.Enqueue(first,0,0) && !queue.Enqueue(first,65536,2160));
    queue.Enqueue(first,5120,1440);queue.Cancel();queue.Register(first);assert(!queue.Take(first).owner);
    queue.Register(second);assert(!queue.Enqueue(first,1920,1080));assert(queue.Enqueue(second,3440,1440));
    queue.Register(first);assert(!queue.Take(first).owner);
    assert(queue.Enqueue(first,3840,2160));assert(!queue.Take(second).owner);
    std::puts("Native main layout: full-main-only4K/ultrawide viewport, horizontal FOV, label/pick roundtrip; split/offscreen/default preserved; latest resize/generation cancellation passed");
}
