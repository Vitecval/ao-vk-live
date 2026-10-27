// Exercise the real command ring without a GPU. A failed worker must wake every
// caller wait, including when a legacy exception handler blocks that worker.
#include "threaded.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <stdexcept>

namespace rvk {
struct ThreadedRegressionAccess {
    static LONG CALLBACK Fault(EXCEPTION_POINTERS* e)
    {
        if(e->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION)return EXCEPTION_CONTINUE_SEARCH;
        ReportRenderWorkerFault(e->ExceptionRecord->ExceptionCode,reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress));
        // Model AO's blocking crash handler after it has recorded the fault.
        Sleep(INFINITE);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static int Child(const char* kind,const char* wait)
    {
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
        auto* d=new ThreadedDevice;
        d->m_ring=static_cast<uint8_t*>(_aligned_malloc(ThreadedDevice::kQueueBytes,64));
        AddVectoredExceptionHandler(1,Fault);
        d->m_thread=CreateThread(nullptr,0,ThreadedDevice::WorkerMain,d,0,nullptr);
        const unsigned fault=std::strcmp(kind,"cpp")==0 ? 0:std::strcmp(kind,"av")==0 ? 1:2;
        auto* data=d->Reserve(sizeof(unsigned));
        std::memcpy(data,&fault,sizeof(fault));
        reinterpret_cast<ThreadedDevice::Header*>(data-sizeof(ThreadedDevice::Header))->run=[](void* p) {
            Sleep(20); // let the caller enter its wait first
            const unsigned fault=*static_cast<unsigned*>(p);
            if(fault==0)throw std::runtime_error("injected render failure");
            if(fault==1)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
            ExitThread(0); // worker exited without publishing progress
        };
        d->Commit();
        if(std::strcmp(wait,"sync")==0)d->Sync();
        else if(std::strcmp(wait,"frame")==0) {
            d->m_framesQueued.store(4);
            d->EndFrame();
        } else if(std::strcmp(wait,"queue")==0) {
            for(int i=0;i<8;++i) {
                auto* p=d->Reserve(ThreadedDevice::kQueueBytes/3);
                reinterpret_cast<ThreadedDevice::Header*>(p-sizeof(ThreadedDevice::Header))->run=[](void*){};
                d->Commit();
            }
        } else d->StopWorker();
        return 3; // no failed operation may return normally
    }
};
}

int main(int argc,char** argv)
{
    if(argc==4)return rvk::ThreadedRegressionAccess::Child(argv[2],argv[3]);
    char exe[MAX_PATH];GetModuleFileNameA(nullptr,exe,MAX_PATH);
    for(const char* kind:{"cpp","av","exit"})for(const char* wait:{"sync","frame","queue","shutdown"}) {
        char command[2*MAX_PATH];std::snprintf(command,sizeof(command),"\"%s\" --child %s %s",exe,kind,wait);
        STARTUPINFOA si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
        if(!CreateProcessA(nullptr,command,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 1;
        const DWORD started=GetTickCount();
        const DWORD result=WaitForSingleObject(pi.hProcess,5000);
        DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);
        if(result!=WAIT_OBJECT_0)TerminateProcess(pi.hProcess,1);
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        if(result!=WAIT_OBJECT_0 || code!=0xE052564Bu) {
            std::fprintf(stderr,"%s/%s failed: wait %lu exit %08lX\n",kind,wait,result,code);return 2;
        }
        std::printf("%s/%s: failed promptly (%lu ms), original fault retained\n",kind,wait,GetTickCount()-started);
    }
    std::puts("12 render-worker failure/wait cases passed");
}
