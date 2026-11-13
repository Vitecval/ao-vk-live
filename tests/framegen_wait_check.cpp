#include <Windows.h>
#include <cstdio>
#include <cstdint>
void waitForPerformanceCount(int64_t targetCount);
static uint64_t ThreadCpu(){
    FILETIME created,exited,kernel,user;
    if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user))return UINT64_MAX;
    ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;return k.QuadPart+u.QuadPart;
}
int main(){
    LARGE_INTEGER frequency,start,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
    waitForPerformanceCount(0);waitForPerformanceCount(start.QuadPart-1);
    const uint64_t cpuStart=ThreadCpu();if(cpuStart==UINT64_MAX)return 1;
    int64_t worstLate=0;
    for(int i=0;i<64;++i){
        QueryPerformanceCounter(&now);const int64_t target=now.QuadPart+frequency.QuadPart*(5+i%4)/1000;
        waitForPerformanceCount(target);QueryPerformanceCounter(&now);
        if(now.QuadPart<target)return 2;
        if(now.QuadPart-target>worstLate)worstLate=now.QuadPart-target;
    }
    const double wall=double(now.QuadPart-start.QuadPart)/frequency.QuadPart;
    const double cpu=double(ThreadCpu()-cpuStart)/1e7;
    std::printf("PACING WAIT wall_ms=%.3f cpu_ms=%.3f cpu_percent=%.2f worst_late_ms=%.3f\n",wall*1000,cpu*1000,cpu/wall*100,1000.*worstLate/frequency.QuadPart);
    if(cpu/wall>.5){std::puts("FAIL: pacing occupies more than half a CPU core");return 3;}
    DWORD before=0,after=0;if(!GetProcessHandleCount(GetCurrentProcess(),&before))return 4;
    for(unsigned i=0;i<16;++i){
        HANDLE thread=CreateThread(nullptr,0,[](void*)->DWORD{
            LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);
            waitForPerformanceCount(n.QuadPart+f.QuadPart/1000);return 0;
        },nullptr,0,nullptr);
        if(!thread)return 5;
        const DWORD result=WaitForSingleObject(thread,1000);CloseHandle(thread);
        if(result!=WAIT_OBJECT_0)return 6;
    }
    if(!GetProcessHandleCount(GetCurrentProcess(),&after)||after>before+1)return 7;
    std::printf("pacing thread timer handles: before=%lu after=%lu\n",before,after);
    std::puts("PASS: deadlines reached without long busy-waiting");
}
