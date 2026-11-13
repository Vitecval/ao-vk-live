// Windows WSI may synchronously message the HWND owner from a presenter thread.
// Reproduce that dependency through the real producer/worker ring without a GPU.
#include "threaded.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <malloc.h>

namespace {
constexpr UINT Sent=WM_APP+51,Posted=WM_APP+52;
unsigned sentCalls=0,postedCalls=0;
std::atomic<bool> acknowledged{false};
LRESULT CALLBACK Window(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==Sent){++sentCalls;return 2026;}
    if(msg==Posted){++postedCalls;return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
}
namespace rvk {
struct ThreadedRegressionAccess {
    static int Run(const char* kind){
        WNDCLASSW wc{};wc.lpfnWndProc=Window;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AOVKSentMessageRegression";RegisterClassW(&wc);
        HWND window=CreateWindowW(wc.lpszClassName,L"isolated WSI wait regression",0,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
        if(!window)return 1;
        auto* d=new ThreadedDevice;
        d->m_window=window;
        d->m_ring=static_cast<uint8_t*>(_aligned_malloc(ThreadedDevice::kQueueBytes,64));
        d->m_thread=CreateThread(nullptr,0,ThreadedDevice::WorkerMain,d,0,nullptr);
        MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        PostMessageW(window,Posted,7,0);PostQuitMessage(13);
        struct Request{ThreadedDevice* device;HWND window;};
        auto* request=reinterpret_cast<Request*>(d->Reserve(sizeof(Request)));
        *request={d,window};
        reinterpret_cast<ThreadedDevice::Header*>(reinterpret_cast<uint8_t*>(request)-sizeof(ThreadedDevice::Header))->run=[](void* data){
            const auto& request=*static_cast<Request*>(data);
            Sleep(10); // caller enters its render wait first
            DWORD_PTR result=0;
            acknowledged=SendMessageTimeoutW(request.window,Sent,0,0,SMTO_BLOCK|SMTO_ABORTIFHUNG,350,&result)!=0 && result==2026;
            request.device->m_framesDone.store(3,std::memory_order_release);WakeByAddressAll(&request.device->m_framesDone);
        };
        d->Commit();
        DWORD start=GetTickCount();
        if(std::strcmp(kind,"sync")==0)d->Sync();
        else if(std::strcmp(kind,"frame")==0){d->m_framesQueued.store(4);d->EndFrame();}
        else if(std::strcmp(kind,"queue")==0){
            for(int i=0;i<8;++i){auto* p=d->Reserve(ThreadedDevice::kQueueBytes/3);reinterpret_cast<ThreadedDevice::Header*>(p-sizeof(ThreadedDevice::Header))->run=[](void*){};d->Commit();}
            d->Sync();
        } else d->StopWorker();
        DWORD elapsed=GetTickCount()-start;
        d->StopWorker();
        bool postedKept=postedCalls==0 && PeekMessageW(&msg,window,Posted,Posted,PM_REMOVE) && msg.message==Posted && msg.wParam==7;
        bool quitKept=PeekMessageW(&msg,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE) && msg.message==WM_QUIT && msg.wParam==13;
        const bool ok=acknowledged && sentCalls==1 && postedKept && quitKept && elapsed<250;
        std::printf("%s: sent=%u acknowledged=%u posted-kept=%u quit-kept=%u elapsed=%lu ms\n",kind,sentCalls,unsigned(acknowledged.load()),unsigned(postedKept),unsigned(quitKept),elapsed);
        delete d;DestroyWindow(window);
        return ok?0:7;
    }
};
}
int main(int argc,char** argv){
    if(argc==3)return rvk::ThreadedRegressionAccess::Run(argv[2]);
    char exe[MAX_PATH];GetModuleFileNameA(nullptr,exe,MAX_PATH);
    for(const char* kind:{"sync","frame","queue","shutdown"}){
        char command[2*MAX_PATH];std::snprintf(command,sizeof(command),"\"%s\" --case %s",exe,kind);
        STARTUPINFOA si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
        if(!CreateProcessA(nullptr,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 1;
        DWORD result=WaitForSingleObject(pi.hProcess,4000),code=0;GetExitCodeProcess(pi.hProcess,&code);
        if(result!=WAIT_OBJECT_0)TerminateProcess(pi.hProcess,1);
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        if(result!=WAIT_OBJECT_0 || code){std::fprintf(stderr,"WSI sent-message %s failed: wait %lu exit %lu\n",kind,result,code);return 2;}
    }
    std::puts("PASS: synchronous driver messages finish during all four render waits; posted input and WM_QUIT are preserved.");
    return 0;
}
