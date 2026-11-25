#include "display_mode_win32.h"
#include "display_window_extent_policy.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <set>

namespace {
constexpr UINT ForwardProbe=WM_APP+93;
bool installCompeting=false;
WNDPROC competingPrevious=nullptr;
unsigned competingCalls=0,destroyedClean=0;
LRESULT CALLBACK CompetingWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==ForwardProbe)++competingCalls;
    return CallWindowProcW(competingPrevious,window,message,wp,lp);
}
LRESULT CALLBACK FixtureWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==ForwardProbe)return 2093;
    if(message==WM_WINDOWPOSCHANGED && installCompeting) {
        installCompeting=false;
        competingPrevious=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&CompetingWindowProc)));
    }
    if(message==WM_NCDESTROY && !GetPropW(window,rvk::display::WindowTrackScope::property))++destroyedClean;
    const LRESULT result=DefWindowProcW(window,message,wp,lp);
    // Deliberately cap the application's outer size. The production scoped
    // transaction must override this only while applying an explicit mode.
    if(message==WM_GETMINMAXINFO && lp) {
        auto* info=reinterpret_cast<MINMAXINFO*>(lp);
        info->ptMaxTrackSize={900,700};
    }
    return result;
}
}

int main(int argc,char** argv) {
    using namespace rvk::display;
    assert(!HasOverride(Options{}));
    Options explicitSize;explicitSize.width=1920;explicitSize.height=1080;assert(HasOverride(explicitSize));
    static_assert(sizeof(uint16_t)==2,"AO resolution ABI");
    assert(Dimensions(1920,1080) && Dimensions(2560,1440) && Dimensions(3840,2160));
    assert(Dimensions(3440,1440) && Dimensions(5120,1440) && Dimensions(7680,2160));
    assert(!Dimensions(65536,1080) && !Dimensions(1920,0) && !Dimensions(65535,65535));
    assert(!Dimensions(7680,4320,Limits{4096,4096}));
    // Simulated nonclient growth from one DPI's frame metrics to another's.
    const auto correctedTrack=ExpandTrackLimit({1936,1119},{1944,1138});
    assert(correctedTrack.width==1944 && correctedTrack.height==1138);
    const auto boundedTrack=ExpandTrackLimit(correctedTrack,{UINT32_MAX,UINT32_MAX});
    assert(boundedTrack.width==1944 && boundedTrack.height==1138);
    const Mode current{3440,1440,32,144};
    const std::vector<Mode> physical={{1920,1080,32,60},{1920,1080,32,144},
        {1920,1080,32,144},{3440,1440,32,60},{3440,1440,32,144},{3840,2160,32,120}};
    auto modes=Enumerate(physical,current);
    assert(modes.size()==6); // dimensions x virtual16/32, no duplicate Hz
    assert(Enumerate(physical,current,{},true).size()==10);
    Filter f;f.width=true;f.wanted.width=1920;
    auto narrow=Enumerate(physical,current,f);
    assert(narrow.size()==2 && narrow[0].width==1920); // desktop cannot bypass filter
    f.height=true;f.wanted.height=1200;
    assert(Enumerate(physical,current,f).empty());
    f={};f.bits=true;f.wanted.bits=32;f.refresh=true;f.wanted.refresh=60;
    auto refresh=Enumerate(physical,current,f);
    assert(refresh.size()==2 && refresh[0].refresh==60 && refresh[1].refresh==60);
    f.wanted.bits=24;assert(Enumerate(physical,current,f).empty());
    f={};f.width=true;f.wanted.width=3840;
    assert(Enumerate(physical,current,f,false,Limits{2560,1600}).empty());
    auto fallback=Enumerate({},current);
    assert(fallback.size()>=2);
    f.width=true;f.wanted.width=1234;
    assert(Enumerate({},current,f).empty());
    std::vector<Mode> many;
    for(unsigned i=0;i<200;++i)many.push_back({640+i*16,480+i*8,32,60});
    auto bounded=Enumerate(many,current,{},false,{},kNativeCapacity);
    assert(bounded.size()==128);
    assert(std::any_of(bounded.begin(),bounded.end(),[&](const Mode& m){return m.width==current.width && m.height==current.height;}));
    struct Native {uint16_t width,height;uint8_t bits,pad;};
    static_assert(sizeof(Native)==6,"Do not alter DeviceDesc mode ABI");
    struct Guarded {uint32_t before=0x12345678;Native modes[128]{};uint32_t after=0x87654321;} guard;
    uint8_t count=0;
    for(const auto& m:Enumerate(many,current,{},true))InsertNative(guard.modes,count,m,current);
    assert(count==128 && guard.before==0x12345678 && guard.after==0x87654321);
    std::set<std::tuple<unsigned,unsigned,unsigned>> unique;
    for(unsigned i=0;i<count;++i)assert(unique.insert({guard.modes[i].width,guard.modes[i].height,guard.modes[i].bits}).second);
    InsertNative(guard.modes,count,{65536,1080,32,60},current);
    InsertNative(guard.modes,count,{1920,1080,8,60},current);
    assert(count==128 && guard.after==0x87654321);
    Options options;options.mode=WindowMode::Windowed;options.width=3840;options.height=2160;
    auto size=SelectSize(options,1280,720,physical);
    assert(size.overridden && size.width==3840 && size.height==2160);
    assert(SelectSize(options,1280,720,physical,Limits{2560,1600}).rejected);
    options.width=1234;assert(SelectSize(options,1280,720,physical).rejected);
    options={};size=SelectSize(options,1234,777,physical);
    assert(!size.overridden && !size.rejected && size.width==1234 && size.height==777);

    char directory[MAX_PATH]{},path[MAX_PATH]{};
    assert(GetTempPathA(MAX_PATH,directory) && GetTempFileNameA(directory,"rvd",0,path));
    const char* cases=argc>1 ? argv[1]:DISPLAY_MODE_CASES_PATH;
    std::ifstream input(cases);assert(input);
    std::string line;unsigned rows=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::vector<std::string> values;std::istringstream row(line);std::string value;
        while(std::getline(row,value,'\t'))values.push_back(value);
        assert(values.size()==6);
        WritePrivateProfileStringA("Display",nullptr,nullptr,path);
        const char* keys[]={"Mode","Width","Height","Monitor"};
        for(unsigned i=0;i<4;++i)if(values[i+1]!="-")assert(WritePrivateProfileStringA("Display",keys[i],values[i+1].c_str(),path));
        auto parsed=ReadOptions(path);
        assert(parsed.valid==(values[5]=="valid"));++rows;
    }
    assert(rows>=10);
    WritePrivateProfileStringA("Display",nullptr,nullptr,path);
    WritePrivateProfileStringA("Display","Mode","Windowed",path);
    WritePrivateProfileStringA("Native","Scene","off",path);
    assert(!ReadOptions(path).valid); // do not apply a bigger buffer without its viewport/picking closure
    WritePrivateProfileStringA("Native","Scene","on",path);
    const auto& frozen=StartupOptions(path);assert(frozen.mode==WindowMode::Windowed);
    WritePrivateProfileStringA("Display","Mode","Borderless",path);
    assert(StartupOptions(path).mode==WindowMode::Windowed); // requires client restart
    DisableOverride(path,"isolated failure test");
    assert(StartupOptions(path).mode==WindowMode::Game && !StartupOptions(path).width);
    assert(ReadOptions(path).mode==WindowMode::Borderless); // saved choice remains intact
    DeleteFileA(path);
    // Read-only real monitor smoke check; no desktop/window mode changes.
    auto monitor=Discover(nullptr,Options{});
    assert(monitor.handle && Dimensions(monitor.current.width,monitor.current.height));
    auto actual=Enumerate(monitor.modes,monitor.current);
    assert(!actual.empty());
    Options borderless;borderless.mode=WindowMode::Borderless;
    auto nativeSize=SelectSize(borderless,640,480,monitor);
    assert(nativeSize.overridden && nativeSize.width==monitor.current.width && nativeSize.height==monitor.current.height);
    borderless.width=monitor.current.width;borderless.height=monitor.current.height;
    assert(!SelectSize(borderless,640,480,monitor).rejected);
    borderless.width+=1;assert(SelectSize(borderless,640,480,monitor).rejected);
    WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=FixtureWindowProc;cls.lpszClassName=L"AO-VK.DisplayPolicyTest";
    assert(RegisterClassW(&cls));
    // Only our private fixture window opts into PMv2, via its creation thread.
    // No process-wide awareness or existing application HWND is changed.
    const auto& dpiApi=dpi::detail::Functions();
    HANDLE previousContext=dpiApi.setContext ? dpiApi.setContext(reinterpret_cast<HANDLE>(INT_PTR(-4))):nullptr;
    if(!previousContext){std::fputs("Private display fixture requires per-monitor window DPI APIs\n",stderr);return 2;}
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Private hidden display test",WS_OVERLAPPEDWINDOW,
        30,30,640,480,nullptr,nullptr,cls.hInstance,nullptr);
    dpiApi.setContext(previousContext);
    assert(window);
    dpi::WindowContext windowCoordinates(window);
    WindowState beforeWindow=CaptureWindow(window);assert(beforeWindow.valid);
    Options exact;exact.mode=WindowMode::Windowed;exact.width=monitor.current.width;exact.height=monitor.current.height;
    WindowState rollback;
    const auto inspected=dpi::Inspect(window,monitor.handle);
    bool applied=ApplyWindowOptions(window,exact,monitor,&rollback);
    std::printf("Private window: awareness %d; logical %ux%u physical %ux%u; monitor scale %u DPI %u; requested %ux%u; applied %u; %s\n",
        int(inspected.observed.awareness),inspected.observed.logicalWidth,inspected.observed.logicalHeight,
        inspected.observed.physicalWidth,inspected.observed.physicalHeight,inspected.observed.monitorScalePercent,
        inspected.observed.windowDpi,exact.width,exact.height,unsigned(applied),
        WindowOptionsFailureReason() ? WindowOptionsFailureReason():inspected.decision.reason);
    std::fflush(stdout);
    assert(applied);
    RECT client{};GetClientRect(window,&client);
    assert(client.right==LONG(exact.width) && client.bottom==LONG(exact.height));
    assert(ApplyWindowOptions(window,exact,monitor)); // idempotent, not a resize loop
    assert(RestoreWindow(rollback));
    WindowState restored=CaptureWindow(window);
    assert(restored.style==beforeWindow.style && EqualRect(&restored.rect,&beforeWindow.rect));
    assert(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))==FixtureWindowProc);
    assert(!GetPropW(window,WindowTrackScope::property));
    // Controlled outer-frame growth, representing a larger target monitor DPI.
    // SetRequired must lift the correction, then remove the limit afterward.
    {
        WindowTrackScope track(window,{2000,1200});assert(track.window);
        MINMAXINFO info{};SendMessageW(window,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&info));
        assert(info.ptMaxTrackSize.x==2000 && info.ptMaxTrackSize.y==1200);
        track.SetRequired({2024,1238});
        info={};SendMessageW(window,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&info));
        assert(info.ptMaxTrackSize.x==2024 && info.ptMaxTrackSize.y==1238);
    }
    MINMAXINFO unscoped{};SendMessageW(window,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&unscoped));
    assert(unscoped.ptMaxTrackSize.x==900 && unscoped.ptMaxTrackSize.y==700);
    // Install a later subclass from the real SetWindowPos callback while our
    // temporary handler is in its chain. After scope return it must still reach
    // AO's old procedure, with the temporary size override switched off.
    using ThreadContextFn=HANDLE(WINAPI*)();using EqualContextFn=BOOL(WINAPI*)(HANDLE,HANDLE);
    const auto user=GetModuleHandleW(L"user32.dll");
    const auto threadContext=reinterpret_cast<ThreadContextFn>(GetProcAddress(user,"GetThreadDpiAwarenessContext"));
    const auto equalContext=reinterpret_cast<EqualContextFn>(GetProcAddress(user,"AreDpiAwarenessContextsEqual"));
    assert(threadContext && equalContext);
    const HANDLE originalCaller=threadContext();
    const HANDLE changed=dpiApi.setContext(reinterpret_cast<HANDLE>(INT_PTR(-1)));assert(changed);
    const HANDLE unawareCaller=threadContext();
    installCompeting=true;
    assert(ApplyWindowOptions(window,exact,monitor));
    assert(equalContext(unawareCaller,threadContext())); // Apply's PM HWND scope restored the unaware caller
    dpiApi.setContext(originalCaller);
    assert(competingPrevious && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))==CompetingWindowProc);
    assert(GetPropW(window,WindowTrackScope::property)); // durable inactive forwarding context
    assert(SendMessageW(window,ForwardProbe,0,0)==2093 && competingCalls==1);
    unscoped={};SendMessageW(window,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&unscoped));
    assert(unscoped.ptMaxTrackSize.x==900 && unscoped.ptMaxTrackSize.y==700);
    assert(RestoreWindow(rollback));
    // A second bounded transaction reuses the inactive context without another
    // subclass layer or loss of the competing handler's saved forwarder.
    assert(ApplyWindowOptions(window,exact,monitor));
    assert(SendMessageW(window,ForwardProbe,0,0)==2093 && competingCalls==2);
    DestroyWindow(window);UnregisterClassW(cls.lpszClassName,cls.hInstance);
    assert(destroyedClean==1);
    std::printf("Display policy passed: %u schema cases; %zu real monitor modes; current %ux%u @%u;128-slot ABI/filter/refresh/GPU/fallback checks\n",
        rows,actual.size(),monitor.current.width,monitor.current.height,monitor.current.refresh);
}
