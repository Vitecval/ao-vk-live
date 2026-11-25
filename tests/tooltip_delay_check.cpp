#include "native/tooltip_delay.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace rnative { void Log(const char* fmt,...) {va_list a;va_start(a,fmt);std::vprintf(fmt,a);std::puts("");va_end(a);} }
static void Check(bool ok,const char* why){if(!ok){std::printf("FAILED %s (%lu)\n",why,GetLastError());std::exit(1);}}
static void Write(void* p,const void* data,size_t n){DWORD old=0,ignored=0;Check(VirtualProtect(p,n,PAGE_EXECUTE_READWRITE,&old)!=0,"test write permission");std::memcpy(p,data,n);Check(FlushInstructionCache(GetCurrentProcess(),p,n)!=0,"test flush");Check(VirtualProtect(p,n,old,&ignored)!=0,"test restore permission");}
int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"pass the audited GUI.dll path");
    // Occupy the preferred image base so the real Windows loader must relocate
    // this non-executed test mapping. No GUI entry point/import is called.
    void* reserved=VirtualAlloc(reinterpret_cast<void*>(0x10000000),0x300000,MEM_RESERVE,PAGE_NOACCESS);
    HMODULE gui=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    Check(gui!=nullptr,"load GUI test image");
    Check(reinterpret_cast<uintptr_t>(gui)!=0x10000000,"exercise ASLR-relative patch");
    auto* code=reinterpret_cast<uint8_t*>(gui)+0x157C7B;
    const uint8_t original[]={0x05,0xF4,0x01,0,0},instant[]={0x05,0,0,0,0};
    Check(std::memcmp(code,original,5)==0,"real audited instruction");
    MEMORY_BASIC_INFORMATION before{},after{};VirtualQuery(code,&before,sizeof(before));
    rnative::tooltips::DelayPatch patch;
    Check(patch.Apply(nullptr,false),"default off needs no module");
    Check(!patch.Apply(nullptr,true),"late GUI safely waits");
    for(int i=0;i<10;++i){
        Check(patch.Apply(gui,true)&&patch.Changed()&&std::memcmp(code,instant,5)==0,"enable 500 to zero");
        Check(patch.Apply(gui,true)&&std::memcmp(code,instant,5)==0,"enable idempotent");
        Check(patch.Apply(gui,false)&&!patch.Changed()&&std::memcmp(code,original,5)==0,"disable restores 500");
    }
    VirtualQuery(code,&after,sizeof(after));Check(before.Protect==after.Protect,"original page protection retained");
    uint8_t other[]={0x05,0x7B,0,0,0};Write(code,other,5);
    Check(!patch.Apply(gui,true)&&std::memcmp(code,other,5)==0,"foreign delay rejected unchanged");Write(code,original,5);
    other[0]=0x90;Write(code,other,5);
    Check(!patch.Apply(gui,true)&&std::memcmp(code,other,5)==0,"unexpected opcode rejected unchanged");Write(code,original,5);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(gui);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<uint8_t*>(gui)+dos->e_lfanew);
    const DWORD stamp=nt->FileHeader.TimeDateStamp,unknown=0;
    Write(&nt->FileHeader.TimeDateStamp,&unknown,4);
    Check(!patch.Apply(gui,true)&&std::memcmp(code,original,5)==0,"unsupported build rejected unchanged");
    Write(&nt->FileHeader.TimeDateStamp,&stamp,4);
    DWORD old=0,ignored=0;Check(VirtualProtect(code,5,PAGE_NOACCESS,&old)!=0,"guard test setup");
    Check(!patch.Apply(gui,true),"unreadable executable address refused safely");
    Check(VirtualProtect(code,5,old,&ignored)!=0,"guard test teardown");
    Check(patch.Apply(gui,true),"enable for ownership check");other[0]=0x05;Write(code,other,5);
    Check(!patch.Apply(gui,false)&&std::memcmp(code,other,5)==0,"cleanup preserves a competing patch");
    Write(code,instant,5);Check(patch.Apply(gui,false),"cleanup restores owned operand");
    Check(std::memcmp(code,original,5)==0,"final original bytes");
    FreeLibrary(gui);if(reserved)VirtualFree(reserved,0,MEM_RELEASE);
    std::puts("PASS: real relocated GUI image, ten enable/disable cycles, protection, late load, mismatched build/opcode/delay, unreadable page and cleanup ownership; no file writes or GUI execution.");
}
