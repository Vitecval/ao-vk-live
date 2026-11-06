// Production DirectDraw COM implementation in an isolated dependency fixture.
// No AO executable is loaded or launched; only this program's window is changed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <cstdio>

int main() {
    HMODULE stock=LoadLibraryA("randy31.dll"),addon=LoadLibraryA("ao-vk.dll");
    if(!stock||!addon){std::printf("dependency load failed %lu\n",GetLastError());return 1;}
    using Boot=BOOL(__cdecl*)(HMODULE,void**,void**);
    using Create=HRESULT(WINAPI*)(GUID*,void**,REFIID,IUnknown*);
    auto boot=reinterpret_cast<Boot>(GetProcAddress(addon,"RvkBootstrap"));void* create=nullptr;void* enumerate=nullptr;
    if(!boot||!boot(stock,&create,&enumerate)||!create)return 2;
    IDirectDraw7* dd=nullptr;
    if(FAILED(reinterpret_cast<Create>(create)(nullptr,reinterpret_cast<void**>(&dd),IID_IDirectDraw7,nullptr))||!dd)return 3;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AOVKExclusiveFallbackCheck";RegisterClassW(&wc);
    HWND h=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"AO-VK isolated window-mode check",WS_OVERLAPPEDWINDOW,90,100,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    if(!h)return 4;
    DEVMODEW before{};before.dmSize=sizeof(before);EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&before);
    const LONG_PTR style=GetWindowLongPtrW(h,GWL_STYLE);RECT original{};GetWindowRect(h,&original);
    for(int i=0;i<20;++i){
        if(FAILED(dd->SetCooperativeLevel(h,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)))return 5;
        if(!(GetWindowLongPtrW(h,GWL_STYLE)&WS_POPUP))return 6;
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&monitor);
        RECT full{};GetWindowRect(h,&full);if(!EqualRect(&full,&monitor.rcMonitor))return 7;
        if(FAILED(dd->SetDisplayMode(1280,720,32,144,0)))return 8;
        if(FAILED(dd->SetCooperativeLevel(h,DDSCL_NORMAL)))return 9;
        RECT restored{};GetWindowRect(h,&restored);
        if(GetWindowLongPtrW(h,GWL_STYLE)!=style||!EqualRect(&original,&restored))return 10;
        if(FAILED(dd->RestoreDisplayMode()))return 11;
    }
    DEVMODEW after{};after.dmSize=sizeof(after);EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&after);
    if(before.dmPelsWidth!=after.dmPelsWidth||before.dmPelsHeight!=after.dmPelsHeight||before.dmDisplayFrequency!=after.dmDisplayFrequency)return 12;
    dd->Release();DestroyWindow(h);
    std::puts("20 production COM fullscreen-request/borderless/normal cycles passed; window state restored and desktop resolution/refresh unchanged. True exclusive ownership was NOT tested or claimed.");
}
