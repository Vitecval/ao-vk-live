// Exercise the shipping window subclass through real DirectDraw objects.
// Own hidden, non-activating windows only; no game executable is loaded.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <cstdio>
#include <cstdint>
#include <thread>

namespace {
unsigned erases = 0, paints = 0, destroys = 0, keyReleases = 0;
bool destroyPainting = true;
bool ProbeErase(HWND window, bool blocked);
constexpr UINT ProbeMessage = WM_APP + 81;
constexpr UINT MutationMessage = WM_APP + 82;
IDirectDraw7* callbackDraw=nullptr;
IDirect3D7* callback3d=nullptr;
IDirect3DDevice7* callbackDevice=nullptr;
IDirectDrawClipper* callbackClipper=nullptr;
IDirectDrawSurface7* callbackTarget=nullptr;
bool mutationRejected=false;
LRESULT CALLBACK LegacyWindow(HWND window, UINT message, WPARAM w, LPARAM l) {
    if(message==MutationMessage) {
        DDSURFACEDESC2 desc{};desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
        desc.dwWidth=desc.dwHeight=64;desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE;
        auto* surface=reinterpret_cast<IDirectDrawSurface7*>(uintptr_t(1));
        auto* device=reinterpret_cast<IDirect3DDevice7*>(uintptr_t(1));
        const HRESULT results[]={
            callbackDraw->SetCooperativeLevel(window,DDSCL_NORMAL),
            callbackDraw->SetDisplayMode(64,64,32,0,0),
            callbackDraw->RestoreDisplayMode(),
            callbackDraw->CreateSurface(&desc,&surface,nullptr),
            callbackDevice->SetRenderTarget(callbackTarget,0),
            callbackClipper->SetHWnd(0,window),
            callback3d->CreateDevice(IID_IDirect3DHALDevice,callbackTarget,&device),
        };
        mutationRejected=!surface && !device;
        for(HRESULT result:results)mutationRejected&=result==DDERR_SURFACEBUSY;
        return 2083;
    }
    if (message == WM_ERASEBKGND) {
        ++erases;
        RECT r{0,0,8,8};
        FillRect(reinterpret_cast<HDC>(w), &r, GetSysColorBrush(COLOR_WINDOW));
        return 1;
    }
    if (message == WM_PAINT) {
        ++paints;
        PAINTSTRUCT ps{}; BeginPaint(window,&ps); EndPaint(window,&ps);
        return 0;
    }
    if (message == ProbeMessage) return 2081;
    if (message == WM_KEYUP || message == WM_SYSKEYUP) { ++keyReleases; return 0; }
    if (message == WM_DESTROY) { ++destroys; destroyPainting &= ProbeErase(window,false); }
    return DefWindowProcW(window,message,w,l);
}
LRESULT CALLBACK OtherWindow(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == ProbeMessage) return 2082;
    return LegacyWindow(window,message,w,l);
}
bool ProbeErase(HWND window, bool blocked) {
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=8;info.bmiHeader.biHeight=-8;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    uint32_t* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
    if(!dc||!bitmap||!pixels)return false;
    HGDIOBJ old=SelectObject(dc,bitmap);
    for(unsigned i=0;i<64;++i)pixels[i]=0x0023496d;
    const unsigned before=erases;
    const LRESULT result=SendMessageW(window,WM_ERASEBKGND,reinterpret_cast<WPARAM>(dc),0);
    GdiFlush();
    const bool unchanged=pixels[0]==0x0023496d;
    const bool good=result==1 && unchanged==blocked && erases==before+(blocked?0:1);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    return good;
}

// Hold the real DLL's recursive backend lock on another thread, with a timed
// safety release. Blocking callback regressions fail timing instead of hanging.
template<class F> ULONGLONG WithBackendLock(IDirectDraw7* dd,F&& action) {
    HANDLE entered=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    struct Probe {HANDLE entered,release;} probe{entered,release};
    std::thread backend([&]{dd->EnumDisplayModes(0,nullptr,&probe,[](DDSURFACEDESC2*,void* context)->HRESULT {
        auto& p=*static_cast<Probe*>(context);SetEvent(p.entered);WaitForSingleObject(p.release,2000);return DDENUMRET_CANCEL;
    });});
    if(WaitForSingleObject(entered,3000)!=WAIT_OBJECT_0) {
        SetEvent(release);backend.join();CloseHandle(entered);CloseHandle(release);return ~ULONGLONG(0);
    }
    std::thread safety([&]{Sleep(600);SetEvent(release);});
    const ULONGLONG start=GetTickCount64();action();const ULONGLONG elapsed=GetTickCount64()-start;
    SetEvent(release);backend.join();safety.join();CloseHandle(entered);CloseHandle(release);
    return elapsed;
}
}

int main() {
    SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
    HMODULE stock=LoadLibraryA("randy31.dll"),addon=LoadLibraryA("ao-vk.dll");
    using Boot=BOOL(__cdecl*)(HMODULE,void**,void**);
    using Create=HRESULT(WINAPI*)(GUID*,void**,REFIID,IUnknown*);
    auto boot=addon?reinterpret_cast<Boot>(GetProcAddress(addon,"RvkBootstrap")):nullptr;
    void* create=nullptr,*enumerate=nullptr;
    if(!stock||!boot||!boot(stock,&create,&enumerate)||!create)return 2;
    IDirectDraw7* dd=nullptr;
    if(FAILED(reinterpret_cast<Create>(create)(nullptr,reinterpret_cast<void**>(&dd),IID_IDirectDraw7,nullptr)))return 3;
    WNDCLASSW wc{};wc.lpfnWndProc=LegacyWindow;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AOVKRepaintRegression";
    if(!RegisterClassW(&wc))return 4;
    auto make=[&]{return CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"isolated repaint regression",WS_OVERLAPPEDWINDOW,0,0,96,96,nullptr,nullptr,wc.hInstance,nullptr);};
    HWND window=make(),unrelated=make();
    if(unrelated)SetWindowLongPtrW(unrelated,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&OtherWindow));
    if(!window||!unrelated||FAILED(dd->SetCooperativeLevel(window,DDSCL_NORMAL)))return 5;
    bool good=ProbeErase(window,false);
    DDSURFACEDESC2 desc{};desc.dwSize=sizeof(desc);
    desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;desc.dwWidth=desc.dwHeight=64;
    desc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE;
    IDirectDrawSurface7* back=nullptr,*primary=nullptr;
    if(FAILED(dd->CreateSurface(&desc,&back,nullptr)))return 6;
    desc.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    if(FAILED(dd->CreateSurface(&desc,&primary,nullptr)))return 7;
    if(FAILED(primary->Flip(nullptr,0)))return 8;
    IDirect3D7* d3d=nullptr;IDirect3DDevice7* device=nullptr;IDirectDrawClipper* clipper=nullptr;
    if(FAILED(dd->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d))) ||
       FAILED(d3d->CreateDevice(IID_IDirect3DHALDevice,back,&device)) ||
       FAILED(dd->CreateClipper(0,&clipper,nullptr)) || FAILED(clipper->SetHWnd(0,window)))return 13;
    callbackDraw=dd;callback3d=d3d;callbackDevice=device;callbackClipper=clipper;callbackTarget=back;
    const ULONGLONG mutationMs=WithBackendLock(dd,[&]{good&=SendMessageW(window,MutationMessage,0,0)==2083;});
    good &=mutationMs<300 && mutationRejected;
    IDirectDrawSurface7* retained=nullptr;HWND clipWindow=nullptr;
    good &=SUCCEEDED(device->GetRenderTarget(&retained)) && retained==back;
    if(retained)retained->Release();
    good &=SUCCEEDED(clipper->GetHWnd(&clipWindow)) && clipWindow==window;
    // Hold the backend lock on a worker through its real EnumDisplayModes
    // callback. WM_SIZE on the window thread must not wait for that lock.
    HANDLE entered=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    struct LockProbe{HANDLE entered,release;} probe{entered,release};
    std::thread backend([&]{dd->EnumDisplayModes(0,nullptr,&probe,[](DDSURFACEDESC2*,void* context)->HRESULT {
        auto& p=*static_cast<LockProbe*>(context);SetEvent(p.entered);WaitForSingleObject(p.release,2000);return DDENUMRET_CANCEL;
    });});
    if(WaitForSingleObject(entered,3000)!=WAIT_OBJECT_0){SetEvent(release);backend.join();return 12;}
    std::thread safety([&]{Sleep(600);SetEvent(release);});
    const ULONGLONG beforeSize=GetTickCount64();SendMessageW(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(80,57));
    const ULONGLONG sizeMs=GetTickCount64()-beforeSize;
    good &=sizeMs<300;backend.join();safety.join();CloseHandle(entered);CloseHandle(release);
    SendMessageW(window,WM_SIZE,SIZE_MINIMIZED,0);good &=SUCCEEDED(primary->Flip(nullptr,0));
    SendMessageW(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(154,10));good &=SUCCEEDED(primary->Flip(nullptr,0));
    SendMessageW(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(80,57));good &=SUCCEEDED(primary->Flip(nullptr,0));
    std::printf("WM_SIZE while backend is locked: %llu ms; minimized/icon-sized/real-restore sequence exercised\n",static_cast<unsigned long long>(sizeMs));
    unsigned passed=0;
    for(unsigned i=0;i<120;++i)passed+=ProbeErase(window,true);
    good &=passed==120 && ProbeErase(unrelated,false);
    const unsigned before=paints;
    InvalidateRect(window,nullptr,FALSE);SendMessageW(window,WM_PAINT,0,0);
    good &=paints==before+1 && !GetUpdateRect(window,nullptr,FALSE);
    good &=SendMessageW(window,ProbeMessage,0,0)==2081;
    SendMessageW(window,WM_KEYUP,VK_SPACE,0);SendMessageW(window,WM_SYSKEYUP,VK_MENU,0);
    good &=keyReleases==2;
    // Two live windows with different original handlers. Late notifications for
    // the old window must keep their own procedure after a render-window change.
    if(FAILED(dd->SetCooperativeLevel(unrelated,DDSCL_NORMAL))||FAILED(primary->Flip(nullptr,0)))return 10;
    good &=ProbeErase(unrelated,true) && ProbeErase(window,false);
    good &=SendMessageW(unrelated,ProbeMessage,0,0)==2082;
    good &=SendMessageW(window,ProbeMessage,0,0)==2081;
    const ULONGLONG oldDestroyMs=WithBackendLock(dd,[&]{good&=DestroyWindow(window)!=FALSE;});
    DDSURFACEDESC2 currentMode{};currentMode.dwSize=sizeof(currentMode);
    good &=oldDestroyMs<300 && SUCCEEDED(dd->GetDisplayMode(&currentMode)) && SUCCEEDED(primary->Flip(nullptr,0));
    good &=ProbeErase(unrelated,true); // old HWND destruction cannot shut down the replacement
    window=make();good &=window && ProbeErase(window,false);
    if(FAILED(dd->SetCooperativeLevel(window,DDSCL_NORMAL))||FAILED(primary->Flip(nullptr,0)))return 11;
    good &=ProbeErase(window,true) && ProbeErase(unrelated,false);
    const ULONGLONG destroyMs=WithBackendLock(dd,[&]{good&=DestroyWindow(window)!=FALSE;});
    good &=destroyMs<300;
    // First safe ordinary COM call drains deferred shutdown after the callback
    // and backend-lock owner have returned. Release deliberately never joins.
    good &=SUCCEEDED(dd->GetDisplayMode(&currentMode));
    window=make();
    good &=window && ProbeErase(window,false);
    if(FAILED(dd->SetCooperativeLevel(window,DDSCL_NORMAL))||FAILED(primary->Flip(nullptr,0)))return 9;
    good &=ProbeErase(window,true) && SendMessageW(window,ProbeMessage,0,0)==2081;
    clipper->Release();device->Release();d3d->Release();
    primary->Release();back->Release();dd->Release();DestroyWindow(window);DestroyWindow(unrelated);
    good &=destroys==4 && destroyPainting;
    std::printf("Seven callback mutations rejected before lock: %s (%llu ms); old/current destroy while locked: %llu/%llu ms; deferred drain and replacement retained\n",
        mutationRejected?"PASS":"FAIL",static_cast<unsigned long long>(mutationMs),
        static_cast<unsigned long long>(oldDestroyMs),static_cast<unsigned long long>(destroyMs));
    std::printf("Vulkan background erase protection: %u/120; startup/unrelated/native WM_PAINT/message forwarding/window replacement: %s\n",passed,good?"PASS":"FAIL");
    return good?0:1;
}
