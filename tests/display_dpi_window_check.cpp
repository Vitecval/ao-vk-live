#ifdef NDEBUG
#undef NDEBUG
#endif
#include "proxy/display_dpi_win32.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

int main()
{
    using namespace rvk::display::dpi;
    const auto user=GetModuleHandleW(L"user32.dll");
    using GetThreadContext=HANDLE(WINAPI*)();
    using EqualContext=BOOL(WINAPI*)(HANDLE,HANDLE);
    const auto current=reinterpret_cast<GetThreadContext>(GetProcAddress(user,"GetThreadDpiAwarenessContext"));
    const auto equal=reinterpret_cast<EqualContext>(GetProcAddress(user,"AreDpiAwarenessContextsEqual"));
    const auto& api=detail::Functions();
    if(!current || !equal || !api.setContext || !api.logicalToPhysical || !api.physicalToLogical) {
        std::fputs("DPI window test requires Windows10 per-window APIs; no native DPI proof\n",stderr);return 2;
    }
    const HANDLE initial=current();
    for(int mode : {-1,-2,-4}) {
        const HANDLE previous=api.setContext(reinterpret_cast<HANDLE>(INT_PTR(mode)));
        assert(previous);
        HWND window=CreateWindowExW(0,L"STATIC",L"AO-VK isolated DPI policy test",WS_POPUP,
            32,32,500,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        api.setContext(previous);
        assert(window);
        const auto before=current();
        const auto snapshot=Inspect(window);
        assert(equal(before,current()));
        assert(snapshot.observed.logicalWidth==500 && snapshot.observed.logicalHeight==300);
        assert(snapshot.observed.physicalKnown && snapshot.observed.monitorScaleKnown);
        assert(int(snapshot.observed.awareness)==(mode==-1?0:mode==-2?1:2));
        if(mode==-4)assert(snapshot.decision.nativeOverride);
        if(mode==-1 && snapshot.observed.monitorScalePercent>100)assert(!snapshot.decision.nativeOverride);
        POINT point{};
        {WindowContext scope(window);assert(ClientToScreen(window,&point));}
        const POINT logical=point;
        assert(LogicalScreenToPhysical(window,point));
        assert(PhysicalScreenToLogical(window,point));
        assert(std::abs(point.x-logical.x)<=1 && std::abs(point.y-logical.y)<=1);
        assert(equal(before,current()));
        std::printf("mode %d: logical %ux%u physical %ux%u monitor scale %u window DPI %u override %u; %s\n",
            mode,snapshot.observed.logicalWidth,snapshot.observed.logicalHeight,
            snapshot.observed.physicalWidth,snapshot.observed.physicalHeight,snapshot.observed.monitorScalePercent,
            snapshot.observed.windowDpi,unsigned(snapshot.decision.nativeOverride),snapshot.decision.reason);
        assert(DestroyWindow(window));
    }
    assert(equal(initial,current()));
    std::puts("Hidden fixture windows only; existing windows/process DPI/desktop settings unchanged. Physical mixed-DPI moves not tested.");
}
