#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "display_dpi_policy.h"
#include <windows.h>

namespace rvk::display::dpi {
namespace detail {
struct Api {
    using GetContext=HANDLE(WINAPI*)(HWND);
    using SetContext=HANDLE(WINAPI*)(HANDLE);
    using AwarenessOf=int(WINAPI*)(HANDLE);
    using WindowDpi=UINT(WINAPI*)(HWND);
    using ConvertPoint=BOOL(WINAPI*)(HWND,LPPOINT);
    using MonitorScale=HRESULT(WINAPI*)(HMONITOR,int*);
    GetContext context=nullptr;SetContext setContext=nullptr;AwarenessOf awareness=nullptr;WindowDpi windowDpi=nullptr;
    ConvertPoint logicalToPhysical=nullptr,physicalToLogical=nullptr;MonitorScale scale=nullptr;
    Api() {
        const auto user=GetModuleHandleW(L"user32.dll");
        context=reinterpret_cast<GetContext>(GetProcAddress(user,"GetWindowDpiAwarenessContext"));
        setContext=reinterpret_cast<SetContext>(GetProcAddress(user,"SetThreadDpiAwarenessContext"));
        awareness=reinterpret_cast<AwarenessOf>(GetProcAddress(user,"GetAwarenessFromDpiAwarenessContext"));
        windowDpi=reinterpret_cast<WindowDpi>(GetProcAddress(user,"GetDpiForWindow"));
        logicalToPhysical=reinterpret_cast<ConvertPoint>(GetProcAddress(user,"LogicalToPhysicalPointForPerMonitorDPI"));
        physicalToLogical=reinterpret_cast<ConvertPoint>(GetProcAddress(user,"PhysicalToLogicalPointForPerMonitorDPI"));
        // System-only load; no application-local DLL search or process DPI API.
        const auto shcore=LoadLibraryExW(L"shcore.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(shcore)scale=reinterpret_cast<MonitorScale>(GetProcAddress(shcore,"GetScaleFactorForMonitor"));
    }
};
inline const Api& Functions(){static const Api api;return api;}
}

// Read Win32 logical coordinates in the existing HWND's context, then restore
// the caller immediately. This does not alter process/window awareness.
class WindowContext {
    HANDLE previous=nullptr;
public:
    explicit WindowContext(HWND window) {
        const auto& api=detail::Functions();
        if(api.context && api.setContext) {
            const auto context=api.context(window);
            if(context)previous=api.setContext(context);
        }
    }
    ~WindowContext(){if(previous)detail::Functions().setContext(previous);}
    WindowContext(const WindowContext&)=delete;
    WindowContext& operator=(const WindowContext&)=delete;
};

inline bool LogicalScreenToPhysical(HWND window,POINT& point) {
    const auto fn=detail::Functions().logicalToPhysical;
    return fn && fn(window,&point)!=FALSE;
}
inline bool PhysicalScreenToLogical(HWND window,POINT& point) {
    const auto fn=detail::Functions().physicalToLogical;
    return fn && fn(window,&point)!=FALSE;
}
struct Snapshot { Observation observed;RECT physicalClient{};Decision decision; };
inline Snapshot Inspect(HWND window,HMONITOR targetMonitor=nullptr) {
    Snapshot snapshot;
    if(!IsWindow(window))return snapshot;
    const auto& api=detail::Functions();
    auto& o=snapshot.observed;
    if(api.context && api.awareness)o.awareness=Awareness(api.awareness(api.context(window)));
    if(api.windowDpi)o.windowDpi=api.windowDpi(window);
    if(!targetMonitor)targetMonitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    int scale=0;
    if(targetMonitor && api.scale && SUCCEEDED(api.scale(targetMonitor,&scale)) && scale>=100 && scale<=500) {
        o.monitorScalePercent=uint32_t(scale);o.monitorScaleKnown=true;
    }
    {
        WindowContext context(window);
        RECT client{};
        if(GetClientRect(window,&client) && client.right>client.left && client.bottom>client.top) {
            o.logicalWidth=uint32_t(client.right-client.left);o.logicalHeight=uint32_t(client.bottom-client.top);
            POINT lo{client.left,client.top},hi{client.right,client.bottom};
            if(ClientToScreen(window,&lo) && ClientToScreen(window,&hi) &&
               LogicalScreenToPhysical(window,lo) && LogicalScreenToPhysical(window,hi) && hi.x>lo.x && hi.y>lo.y) {
                snapshot.physicalClient={lo.x,lo.y,hi.x,hi.y};
                o.physicalWidth=uint32_t(hi.x-lo.x);o.physicalHeight=uint32_t(hi.y-lo.y);o.physicalKnown=true;
            }
        }
    }
    snapshot.decision=Evaluate(o);
    return snapshot;
}
} // namespace rvk::display::dpi
