#include "native/main_surface_policy.h"
#include "native/display_layout_policy.h"
#include "native/resize_queue_policy.h"
#include "framebuffer_transaction.h"
#include "display_dpi_policy.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <initializer_list>
#include <vector>

namespace {
using rnative::displaylayout::Rect;
struct Backing {unsigned width,height,leases=1;};
struct Surface {
    Backing* backing;
    unsigned users=1;
    bool alive=true,dirty=true;
    Rect cached{};
    Rect Description() {
        assert(alive && backing->leases);
        if(dirty){cached={0,0,backing->width,backing->height};dirty=false;}
        return cached;
    }
};
struct Frame {Surface* color=nullptr;};
struct Fixture {
    std::vector<std::unique_ptr<Backing>> backings;
    std::vector<std::unique_ptr<Surface>> wrappers;
    Frame active;
    Backing* bound=nullptr;
    Surface* viewport=nullptr;
    Rect actual{};
    Surface* New(unsigned width,unsigned height) {
        auto backing=std::make_unique<Backing>();backing->width=width;backing->height=height;
        backings.push_back(std::move(backing));
        auto surface=std::make_unique<Surface>();surface->backing=backings.back().get();
        wrappers.push_back(std::move(surface));return wrappers.back().get();
    }
    void Destroy(Frame frame) {
        if(!frame.color)return;
        assert(frame.color->alive && frame.color->users);
        if(--frame.color->users==0) {
            assert(frame.color->backing->leases==1);
            --frame.color->backing->leases;frame.color->alive=false;
        }
    }
    Fixture() {
        active.color=New(1280,720);viewport=active.color;
        bound=active.color->backing;actual={0,0,1280,720};
    }
    bool Resize(unsigned width,unsigned height,bool failBuild=false,bool failBind=false) {
        Frame previous=active,candidate;
        return rnative::framebuffer::Replace(
            [&]{active={};},
            [&]{candidate.color=New(width,height);active=candidate;return !failBuild;},
            [&]{if(failBind)return false;bound=candidate.color->backing;return true;},
            [&]{active=previous;Destroy(candidate);bound=previous.color->backing;},
            [&]{
                rnative::framebuffer::PreserveMainSurface(previous.color,active.color,[](Surface* kept,Surface* replacement){
                    std::swap(kept->backing,replacement->backing);kept->dirty=replacement->dirty=true;
                });
                actual={0,0,width,height};Destroy(previous);
            });
    }
};
}

int main() {
    Fixture fixture;
    Surface* retained=fixture.viewport;
    const Rect primaryDescription{0,0,1280,720}; // persistent windowed primary
    assert(retained->Description().width==1280); // exercise cached metadata
    retained->users=3; // native callers share one COM lease, not one per user
    Backing* old=fixture.bound;
    assert(fixture.Resize(3840,2160));
    assert(fixture.active.color==retained && fixture.viewport==retained && retained->alive);
    assert(retained->users==3 && retained->backing->leases==1 && old->leases==0);
    assert(fixture.bound==retained->backing && retained->Description().width==3840);
    // D3D viewport, labels, picking and camera must all use the same committed
    // size, even when the persistent primary descriptor is smaller.
    Rect size=rnative::framebuffer::MainViewportSize(true,Rect{0,0,3840,2160},fixture.actual,primaryDescription);
    assert(size.width==3840 && size.height==2160);
    auto camera=rnative::displaylayout::PreserveHorizontal({-1,1,1,-1,1},size.width,size.height);
    assert(camera.aspect==float(size.width)/size.height);
    Rect offscreen=rnative::framebuffer::MainViewportSize(false,Rect{0,0,512,512},fixture.actual,Rect{0,0,256,256});
    assert(offscreen.width==256 && offscreen.height==256);
    // An offscreen-to-main transition stores this raw native wrapper. A second
    // successful resize must not turn it into a freed previous-set pointer.
    Surface* offscreenSurface=fixture.New(256,256);
    fixture.viewport=offscreenSurface;fixture.viewport=fixture.active.color;
    assert(fixture.Resize(5120,1440));
    assert(fixture.viewport==retained && fixture.viewport->Description().width==5120);
    assert(retained->users==3 && fixture.bound->leases==1);
    for(bool failBuild:{true,false}) {
        Backing* working=fixture.bound;
        assert(!fixture.Resize(7680,4320,failBuild,!failBuild));
        assert(fixture.active.color==retained && fixture.viewport->alive);
        assert(fixture.bound==working && working->leases==1);
        assert(fixture.actual.width==5120 && retained->Description().width==5120);
    }
    assert(fixture.Resize(1920,1080));
    size=rnative::framebuffer::MainViewportSize(true,Rect{0,0,5120,1440},fixture.actual,primaryDescription);
    assert(size.width==1920 && retained->Description().width==1920);

    rnative::resizequeue::Queue queue;int owner=0;queue.Register(&owner);
    unsigned comCalls=0,published=0;
    for(unsigned dimension:{0u,1u,16u,31u,1920u}) {
        bool returned=rnative::framebuffer::RetainCallbackResize(true,true,[&]{
            if(queue.Enqueue(&owner,dimension,dimension))++published;
        });
        if(!returned)++comCalls;
        assert(returned);
    }
    assert(comCalls==0 && published==1 && queue.Take(&owner).width==1920);
    // DPI rejection happens before queue/viewport/framebuffer publication.
    rvk::display::dpi::Observation scaled;
    scaled.awareness=rvk::display::dpi::Awareness::Unaware;
    scaled.logicalWidth=1920;scaled.logicalHeight=1080;
    scaled.physicalWidth=2880;scaled.physicalHeight=1620;scaled.physicalKnown=true;
    scaled.monitorScalePercent=150;scaled.monitorScaleKnown=true;
    assert(rnative::framebuffer::RetainCallbackResize(true,true,[&]{
        if(rvk::display::dpi::Evaluate(scaled).nativeOverride) {
            ++published;queue.Enqueue(&owner,2880,1620);
        }
    }));
    assert(published==1 && !queue.Take(&owner).owner);
    bool ordinaryCalled=false;
    assert(!rnative::framebuffer::RetainCallbackResize(false,true,[&]{ordinaryCalled=true;}));
    assert(!ordinaryCalled);
    std::puts("native resize integration: stable main wrapper/cache, real viewport sizes, rollback, callback retention and DPI queue gate passed");
}
