#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "display_mode_policy.h"
#include "display_dpi_win32.h"
#include "display_window_extent_policy.h"
#include "window_callback_policy.h"
#include <windows.h>
#include <cwchar>
#include <cstdio>
#include <new>

namespace rvk::display {
inline std::wstring Widen(const char* text) {
    if(!text || !*text)return {};
    int n=MultiByteToWideChar(CP_ACP,0,text,-1,nullptr,0);
    if(n<=1)return {};
    std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_ACP,0,text,-1,out.data(),n);out.resize(size_t(n-1));return out;
}
inline std::wstring Trim(std::wstring text) {
    auto space=[](wchar_t c){return c==L' ' || c==L'\t' || c==L'\r' || c==L'\n';};
    while(!text.empty() && space(text.back()))text.pop_back();
    size_t first=0;while(first<text.size() && space(text[first]))++first;
    return text.substr(first);
}
inline bool ParseDimension(const std::wstring& text,uint32_t& out) {
    if(text.empty())return false;
    uint64_t n=0;
    for(wchar_t c:text) {if(c<L'0' || c>L'9')return false;n=n*10+uint32_t(c-L'0');if(n>65535)return false;}
    out=uint32_t(n);return true;
}
inline Options ReadOptions(const char* iniPath) {
    Options out;
    const std::wstring path=Widen(iniPath);
    if(path.empty())return out;
    auto read=[&](const wchar_t* name,const wchar_t* fallback){
        wchar_t text[256]{};
        DWORD n=GetPrivateProfileStringW(L"Display",name,fallback,text,256,path.c_str());
        if(n>=255){out.valid=false;out.error="Display value is too long";}
        return Trim(text);
    };
    std::wstring mode=read(L"Mode",L"Game");
    if(!_wcsicmp(mode.c_str(),L"Game"))out.mode=WindowMode::Game;
    else if(!_wcsicmp(mode.c_str(),L"Windowed"))out.mode=WindowMode::Windowed;
    else if(!_wcsicmp(mode.c_str(),L"Borderless"))out.mode=WindowMode::Borderless;
    else {out.valid=false;out.error="Display Mode must be Game, Windowed or Borderless";}
    if(!ParseDimension(read(L"Width",L"0"),out.width) || !ParseDimension(read(L"Height",L"0"),out.height) ||
       ((out.width==0)!=(out.height==0)) || (out.width && !Dimensions(out.width,out.height))) {
        out.valid=false;out.error="Display Width/Height must both be zero or a safe positive size";
    }
    out.monitor=read(L"Monitor",L"");
    if(!out.monitor.empty()) {
        const wchar_t prefix[]=L"\\\\.\\DISPLAY";
        constexpr size_t length=sizeof(prefix)/sizeof(wchar_t)-1;
        bool good=out.monitor.size()>length && !_wcsnicmp(out.monitor.c_str(),prefix,length);
        for(size_t i=length;i<out.monitor.size();++i)good=good && out.monitor[i]>=L'0' && out.monitor[i]<=L'9';
        if(!good){out.valid=false;out.error="Display Monitor must be a Windows display device name";}
    }
    if(out.mode!=WindowMode::Game || out.width || out.height || !out.monitor.empty()){
        for(const wchar_t* part:{L"Device",L"Scene"}){
            wchar_t value[32]{};GetPrivateProfileStringW(L"Native",part,L"on",value,32,path.c_str());
            if(_wcsicmp(Trim(value).c_str(),L"on")){
                out.valid=false;out.error="Display overrides require the standard native device and scene paths";
            }
        }
    }
    return out;
}
struct StartupState {Options requested,fallback;bool disabled=false;};
inline StartupState& StartupData(const char* iniPath) {
    // Same inline local static across the proxy's translation units. Changing
    // this section while the client runs takes effect only after a restart.
    static StartupState state{ReadOptions(iniPath),Options{},false};
    return state;
}
inline const Options& StartupOptions(const char* iniPath) {
    auto& state=StartupData(iniPath);return state.disabled ? state.fallback:state.requested;
}
inline const Options& RequestedOptions(const char* iniPath) {return StartupData(iniPath).requested;}
inline void DisableOverride(const char* iniPath,const char* reason) {
    auto& state=StartupData(iniPath);state.disabled=true;state.fallback.error=reason ? reason:"Display override rejected";
}
struct Monitor {
    HMONITOR handle=nullptr;
    MONITORINFOEXW info{};
    Mode current;
    std::vector<Mode> modes;
    bool preferenceFound=true;
};
struct MonitorSearch {const wchar_t* name;HMONITOR found=nullptr;};
inline BOOL CALLBACK FindMonitor(HMONITOR m,HDC,LPRECT,LPARAM data) {
    auto* s=reinterpret_cast<MonitorSearch*>(data);MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(m,&info) && !_wcsicmp(s->name,info.szDevice)){s->found=m;return FALSE;}
    return TRUE;
}
inline Mode CurrentMode(HMONITOR handle) {
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(handle,&info))return {};
    DEVMODEW dm{};dm.dmSize=sizeof(dm);
    if(EnumDisplaySettingsExW(info.szDevice,ENUM_CURRENT_SETTINGS,&dm,0))
        return {dm.dmPelsWidth,dm.dmPelsHeight,32,dm.dmDisplayFrequency>1 ? dm.dmDisplayFrequency:0};
    return {uint32_t(info.rcMonitor.right-info.rcMonitor.left),uint32_t(info.rcMonitor.bottom-info.rcMonitor.top),32,0};
}
inline Monitor Discover(HWND window,const Options& options,bool enumerateModes=true) {
    Monitor out;
    out.preferenceFound=options.monitor.empty();
    MonitorSearch search{options.monitor.c_str()};
    if(options.valid && !options.monitor.empty()) {
        EnumDisplayMonitors(nullptr,nullptr,FindMonitor,reinterpret_cast<LPARAM>(&search));
        out.handle=search.found;out.preferenceFound=search.found!=nullptr;
    }
    if(!out.handle)out.handle=window && IsWindow(window) ? MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST):
        MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY);
    out.info.cbSize=sizeof(out.info);
    if(!GetMonitorInfoW(out.handle,&out.info))return out;
    out.current=CurrentMode(out.handle);
    if(!enumerateModes)return out;
    for(DWORD i=0;i<8192;++i) {
        DEVMODEW dm{};dm.dmSize=sizeof(dm);
        // No EDS_RAWMODE: list capabilities of this monitor, not modes from
        // another attached display or unsupported raw adapter dimensions.
        if(!EnumDisplaySettingsExW(out.info.szDevice,i,&dm,0))break;
        if(dm.dmDisplayFlags&DM_INTERLACED)continue;
        if(Dimensions(dm.dmPelsWidth,dm.dmPelsHeight))out.modes.push_back(
            {dm.dmPelsWidth,dm.dmPelsHeight,32,dm.dmDisplayFrequency>1 ? dm.dmDisplayFrequency:out.current.refresh});
    }
    return out;
}
inline Selection SelectSize(const Options& options,uint32_t w,uint32_t h,const Monitor& monitor,Limits limits={}) {
    if(!monitor.preferenceFound)return {w,h,false,true,"selected monitor is not connected; game selection retained"};
    if(options.valid && options.mode==WindowMode::Borderless) {
        if((options.width || options.height) &&
           (options.width!=monitor.current.width || options.height!=monitor.current.height))
            return {w,h,false,true,"borderless requires the selected monitor's current size"};
        if(!Dimensions(monitor.current.width,monitor.current.height,limits))
            return {w,h,false,true,"borderless monitor size exceeds safe display/GPU limits"};
        return {monitor.current.width,monitor.current.height,true,false,"borderless current monitor size"};
    }
    auto modes=monitor.modes;modes.push_back(monitor.current);
    return SelectSize(options,w,h,modes,limits);
}
struct WindowState {
    HWND window=nullptr;LONG_PTR style=0,exStyle=0;RECT rect{};bool valid=false;
};
inline WindowState CaptureWindow(HWND window) {
    WindowState state;state.window=window;
    state.valid=IsWindow(window) && GetWindowRect(window,&state.rect)!=FALSE;
    if(state.valid){state.style=GetWindowLongPtrW(window,GWL_STYLE);state.exStyle=GetWindowLongPtrW(window,GWL_EXSTYLE);}
    return state;
}
inline bool RestoreWindow(const WindowState& state) {
    if(!state.valid || !IsWindow(state.window))return false;
    RemovePropW(state.window,L"AO-VK.DisplayOptionsApplied");
    SetWindowLongPtrW(state.window,GWL_STYLE,state.style);SetWindowLongPtrW(state.window,GWL_EXSTYLE,state.exStyle);
    return SetWindowPos(state.window,nullptr,state.rect.left,state.rect.top,state.rect.right-state.rect.left,
        state.rect.bottom-state.rect.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)!=FALSE;
}
inline WindowState& AppliedWindowState() {static WindowState state;return state;}
inline const char*& WindowOptionsFailureStorage() {static thread_local const char* reason=nullptr;return reason;}
inline const char* WindowOptionsFailureReason() {return WindowOptionsFailureStorage();}
inline void WindowGeometryFailure(uint32_t width,uint32_t height,const RECT& actual,DWORD error) {
    static thread_local char reason[192];
    std::snprintf(reason,sizeof(reason),"Window client size rejected: requested %ux%u; observed %ldx%ld; Win32 error %lu",
        width,height,actual.right-actual.left,actual.bottom-actual.top,error);
    WindowOptionsFailureStorage()=reason;
}
// Only during our explicit SetWindowPos transaction. This also works before
// the normal input subclass is attached and leaves AO's procedure unchanged.
struct WindowTrackScope {
    HWND window=nullptr;
    static constexpr const wchar_t* property=L"AO-VK.Display.TrackTransaction";
private:
    struct Context {
        WNDPROC previous=nullptr;TrackExtent required;
        unsigned references=1;bool active=true,destroyed=false;
    };
    Context* context=nullptr;
    static SRWLOCK& Lock(){static SRWLOCK lock=SRWLOCK_INIT;return lock;}
    // The property, scope and in-flight callbacks each hold one reference.
    // No lock is held while calling AO or running a window transaction.
    static void Release(Context* c){if(!--c->references)delete c;}
public:
    static LRESULT CALLBACK Procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
        AcquireSRWLockExclusive(&Lock());
        auto* c=reinterpret_cast<Context*>(GetPropW(hwnd,property));
        if(!c || !c->previous){ReleaseSRWLockExclusive(&Lock());return DefWindowProcW(hwnd,message,w,l);}
        ++c->references;
        const auto previous=c->previous;
        if(message==WM_NCDESTROY) {
            c->destroyed=true;c->active=false;
            if(RemovePropW(hwnd,property)==reinterpret_cast<HANDLE>(c))Release(c);
        }
        ReleaseSRWLockExclusive(&Lock());
        rvk::presentation::WindowCallbackScope callback;
        const LRESULT result=CallWindowProcW(previous,hwnd,message,w,l);
        AcquireSRWLockExclusive(&Lock());
        if(message==WM_GETMINMAXINFO && l){
            auto& info=*reinterpret_cast<MINMAXINFO*>(l);
            if(c->active){
                const auto extent=ExpandTrackLimit({uint32_t(std::max(0L,info.ptMaxTrackSize.x)),uint32_t(std::max(0L,info.ptMaxTrackSize.y))},c->required);
                info.ptMaxTrackSize={LONG(extent.width),LONG(extent.height)};
            }
        }
        Release(c);
        ReleaseSRWLockExclusive(&Lock());
        return result;
    }
    WindowTrackScope(HWND hwnd,TrackExtent extent):window(hwnd) {
        AcquireSRWLockExclusive(&Lock());
        if(auto* retained=reinterpret_cast<Context*>(GetPropW(hwnd,property))) {
            if(retained->active || retained->destroyed)window=nullptr;
            else {context=retained;++context->references;context->active=true;context->required=extent;}
            ReleaseSRWLockExclusive(&Lock());return;
        }
        auto* created=new(std::nothrow) Context;
        if(!created){window=nullptr;ReleaseSRWLockExclusive(&Lock());return;}
        created->previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
        created->required=extent;
        if(!created->previous || !SetPropW(hwnd,property,reinterpret_cast<HANDLE>(created))){
            delete created;window=nullptr;ReleaseSRWLockExclusive(&Lock());return;
        }
        SetLastError(0);
        if(!SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&Procedure)) && GetLastError()){
            RemovePropW(hwnd,property);delete created;window=nullptr;
        }else{
            context=created;++context->references;
        }
        ReleaseSRWLockExclusive(&Lock());
    }
    void SetRequired(TrackExtent extent){
        AcquireSRWLockExclusive(&Lock());
        if(context && context->active && !context->destroyed)context->required=extent;
        ReleaseSRWLockExclusive(&Lock());
    }
    ~WindowTrackScope(){
        if(!context)return;
        AcquireSRWLockExclusive(&Lock());
        context->active=false;context->required={};
        if(!context->destroyed && IsWindow(window) &&
           reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))==&Procedure){
            SetLastError(0);
            if(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(context->previous)) || !GetLastError())
                if(RemovePropW(window,property)==reinterpret_cast<HANDLE>(context))Release(context);
        }
        // Another subclass may still chain through Procedure. Keep the inactive
        // property reference until WM_NCDESTROY instead of losing AO's handler.
        Release(context);
        ReleaseSRWLockExclusive(&Lock());
    }
    WindowTrackScope(const WindowTrackScope&)=delete;
    WindowTrackScope& operator=(const WindowTrackScope&)=delete;
};
inline bool ApplyWindowOptions(HWND window,const Options& options,const Monitor& monitor,WindowState* previous=nullptr) {
    WindowOptionsFailureStorage()=nullptr;
    if(!options.valid || !monitor.preferenceFound || !IsWindow(window))return false;
    if(options.mode==WindowMode::Game && !options.width && options.monitor.empty())return true;
    const auto dpiState=dpi::Inspect(window,monitor.handle);
    if(!dpiState.decision.nativeOverride) {
        WindowOptionsFailureStorage()=dpiState.decision.reason;
        return false; // no style/position/property changes and no saved INI write
    }
    dpi::WindowContext callerContext(window);
    // A real window may be associated after AO's initial NULL/NORMAL coop call.
    // Its property is destroyed with HWND; repeated association cannot fight
    // user resizing or WM_DPICHANGED later in the session.
    constexpr const wchar_t* marker=L"AO-VK.DisplayOptionsApplied";
    if(GetPropW(window,marker)) {
        if(previous && AppliedWindowState().window==window)*previous=AppliedWindowState();
        return true;
    }
    const WindowState saved=CaptureWindow(window);
    if(previous)*previous=saved;
    RECT oldRect{},client{};
    if(!GetWindowRect(window,&oldRect) || !GetClientRect(window,&client))return false;
    Selection size=SelectSize(options,uint32_t(client.right),uint32_t(client.bottom),monitor);
    if(size.rejected){WindowOptionsFailureStorage()=size.reason;return false;}
    LONG_PTR oldStyle=GetWindowLongPtrW(window,GWL_STYLE),oldEx=GetWindowLongPtrW(window,GWL_EXSTYLE);
    LONG_PTR style=oldStyle,ex=oldEx;
    if(options.mode==WindowMode::Windowed)style=(style&~WS_POPUP)|WS_OVERLAPPEDWINDOW;
    else if(options.mode==WindowMode::Borderless) {
        style=(style&~WS_OVERLAPPEDWINDOW)|WS_POPUP;ex&=~(WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE);
    }
    RECT desired{0,0,LONG(size.width),LONG(size.height)};
    using DpiFn=UINT(WINAPI*)(HWND);
    using AdjustFn=BOOL(WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT);
    auto user=GetModuleHandleW(L"user32.dll");
    auto dpi=reinterpret_cast<DpiFn>(GetProcAddress(user,"GetDpiForWindow"));
    auto adjust=reinterpret_cast<AdjustFn>(GetProcAddress(user,"AdjustWindowRectExForDpi"));
    bool adjusted=adjust && dpi ? adjust(&desired,DWORD(style),FALSE,DWORD(ex),dpi(window))!=FALSE:
        AdjustWindowRectEx(&desired,DWORD(style),FALSE,DWORD(ex))!=FALSE;
    if(!adjusted)return false;
    int width=desired.right-desired.left,height=desired.bottom-desired.top;
    WindowTrackScope exactExtent(window,{uint32_t(width),uint32_t(height)});
    if(!exactExtent.window){WindowOptionsFailureStorage()="Unable to prepare exact window extent";return false;}
    const RECT area=options.mode==WindowMode::Borderless ? monitor.info.rcMonitor:monitor.info.rcWork;
    int x=area.left+std::max<LONG>(0,(area.right-area.left-width)/2);
    int y=area.top+std::max<LONG>(0,(area.bottom-area.top-height)/2);
    auto set=[&](int index,LONG_PTR value){SetLastError(0);return SetWindowLongPtrW(window,index,value)!=0 || GetLastError()==0;};
    SetLastError(0);
    bool applied=set(GWL_STYLE,style) && set(GWL_EXSTYLE,ex) && SetWindowPos(window,nullptr,x,y,width,height,
        SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)!=FALSE;
    // A cross-monitor move can deliver WM_DPICHANGED and change frame metrics.
    // Measure the actual client afterward, then correct the frame size without
    // moving monitors again. No renderer work occurs inside either callback.
    RECT actual{},outer{};
    if(applied && GetClientRect(window,&actual) &&
       (actual.right!=LONG(size.width) || actual.bottom!=LONG(size.height))) {
        applied=GetWindowRect(window,&outer)!=FALSE;
        if(applied){
            const LONG correctedWidth=outer.right-outer.left+LONG(size.width)-actual.right;
            const LONG correctedHeight=outer.bottom-outer.top+LONG(size.height)-actual.bottom;
            exactExtent.SetRequired({uint32_t(std::max(0L,correctedWidth)),uint32_t(std::max(0L,correctedHeight))});
            applied=SetWindowPos(window,nullptr,0,0,correctedWidth,correctedHeight,
                SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)!=FALSE;
        }
    }
    applied=applied && GetClientRect(window,&actual) && actual.right==LONG(size.width) && actual.bottom==LONG(size.height);
    if(!applied) {
        WindowGeometryFailure(size.width,size.height,actual,GetLastError());
        SetWindowLongPtrW(window,GWL_STYLE,oldStyle);SetWindowLongPtrW(window,GWL_EXSTYLE,oldEx);
        SetWindowPos(window,nullptr,oldRect.left,oldRect.top,oldRect.right-oldRect.left,oldRect.bottom-oldRect.top,
            SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        return false;
    }
    SetPropW(window,marker,reinterpret_cast<HANDLE>(uintptr_t(1)));
    AppliedWindowState()=saved;
    return true;
}
} // namespace rvk::display
