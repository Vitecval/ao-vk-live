// CPU-only allocation exhaustion/overflow checks, isolated from the game process.
#include "skin.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <malloc.h>
int main(int argc,char**argv){
 SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
 const bool exhaust=argc>1&&std::strcmp(argv[1],"--oom")==0;
 rvk::skin::Bone bone{};bone.m[0]=bone.m[4]=bone.m[8]=1;bone.m[9]=7;
 rvk::skin::Palette palette;palette.Set(&bone,1);auto* original=palette.bones;
 void* reservations[512]{};unsigned reserved=0;
 if(exhaust){
  SYSTEM_INFO info{};GetSystemInfo(&info);uintptr_t address=0;
  while(address<uintptr_t(info.lpMaximumApplicationAddress)&&reserved<512){
   MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)))break;
   const uintptr_t next=uintptr_t(region.BaseAddress)+region.RegionSize;if(next<=address)break;
   if(region.State==MEM_FREE&&region.RegionSize>=(8u<<20)){
    const uintptr_t first=(uintptr_t(region.BaseAddress)+info.dwAllocationGranularity-1)/info.dwAllocationGranularity*info.dwAllocationGranularity;
    const size_t bytes=(next-first)/info.dwAllocationGranularity*info.dwAllocationGranularity;
    void* block=bytes?VirtualAlloc(reinterpret_cast<void*>(first),bytes,MEM_RESERVE,PAGE_NOACCESS):nullptr;if(block)reservations[reserved++]=block;
   }
   address=next;
  }
  void* probe=_aligned_malloc(size_t(1u<<20)*sizeof(rvk::skin::Palette::Columns),16);
  if(probe){_aligned_free(probe);for(unsigned i=0;i<reserved;++i)VirtualFree(reservations[i],0,MEM_RELEASE);std::puts("Cannot establish allocation exhaustion");return 3;}
 }
 std::printf("Triggering %s after valid pose (reserved %u address ranges)\n",exhaust?"allocation exhaustion":"bone-count overflow",reserved);std::fflush(stdout);
 palette.Set(&bone,exhaust ? (1u<<20):UINT32_MAX);
 const bool safe=palette.count==0&&palette.bones==original&&palette.capacity>=1;
 for(unsigned i=0;i<reserved;++i)VirtualFree(reservations[i],0,MEM_RELEASE);
 palette.Set(&bone,1);const bool recovered=palette.count==1&&palette.bones[0].c[3][0]==7;
 std::printf("rejected safely: %s; recovered pose: %s\n",safe?"PASS":"FAIL",recovered?"PASS":"FAIL");return safe&&recovered?0:1;
}
