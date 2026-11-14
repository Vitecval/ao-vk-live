// Exercise the actual shipping DirectDraw objects, in an isolated DLL fixture.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>
#include <atomic>
#include <new>
namespace {
using HeapAllocate=LPVOID(WINAPI*)(HANDLE,DWORD,SIZE_T);
HeapAllocate actualHeapAllocate=nullptr;std::atomic<bool> failDllHeap{false};std::atomic<unsigned> heapFailures{0};std::atomic<SIZE_T> failedHeapSize{0};
LPVOID WINAPI FaultHeapAllocate(HANDLE heap,DWORD flags,SIZE_T bytes){if(failDllHeap.load()&&(!failedHeapSize.load()||bytes==failedHeapSize.load())&&failDllHeap.exchange(false)){++heapFailures;SetLastError(ERROR_NOT_ENOUGH_MEMORY);return nullptr;}return actualHeapAllocate(heap,flags,bytes);}
bool PatchHeap(HMODULE module,bool enable){
 auto* base=reinterpret_cast<unsigned char*>(module);const auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);const auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!directory.VirtualAddress)return false;
 for(auto* import=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+directory.VirtualAddress);import->Name;++import){if(!import->OriginalFirstThunk)continue;
  auto* names=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+import->OriginalFirstThunk);auto* addresses=reinterpret_cast<IMAGE_THUNK_DATA32*>(base+import->FirstThunk);
  for(;names->u1.AddressOfData;++names,++addresses){if(IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))continue;const auto* name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
   if(std::strcmp(reinterpret_cast<const char*>(name->Name),"HeapAlloc"))continue;DWORD old;if(!VirtualProtect(&addresses->u1.Function,sizeof(DWORD),PAGE_READWRITE,&old))return false;
   if(enable){actualHeapAllocate=reinterpret_cast<HeapAllocate>(uintptr_t(addresses->u1.Function));addresses->u1.Function=DWORD(uintptr_t(&FaultHeapAllocate));}else addresses->u1.Function=DWORD(uintptr_t(actualHeapAllocate));DWORD ignored;VirtualProtect(&addresses->u1.Function,sizeof(DWORD),old,&ignored);return true;
  }
 }return false;
}
struct ThreadProbe{IDirectDrawSurface7* surface;HANDLE ready,release;HRESULT result=E_FAIL;};
DWORD WINAPI ReadCaps(void* data){auto& p=*static_cast<ThreadProbe*>(data);DDSCAPS2 caps{};p.result=p.surface->GetCaps(&caps);SetEvent(p.ready);if(p.release)WaitForSingleObject(p.release,INFINITE);return 0;}
}
int main(int argc,char**argv){
 const bool parentOnly=argc>1&&std::strcmp(argv[1],"--parent")==0;
 SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
 HMODULE stock=LoadLibraryA("randy31.dll"),addon=LoadLibraryA("ao-vk.dll");if(!stock||!addon){std::printf("load failed %lu\n",GetLastError());return 2;}
 using Boot=BOOL(__cdecl*)(HMODULE,void**,void**);using Create=HRESULT(WINAPI*)(GUID*,void**,REFIID,IUnknown*);
 auto boot=reinterpret_cast<Boot>(GetProcAddress(addon,"RvkBootstrap"));void* create=nullptr,*enumerate=nullptr;if(!boot||!boot(stock,&create,&enumerate)){std::printf("bootstrap refused: stock %p addon %p boot %p\n",stock,addon,boot);return 2;}
 IDirectDraw7* dd=nullptr;if(FAILED(reinterpret_cast<Create>(create)(nullptr,reinterpret_cast<void**>(&dd),IID_IDirectDraw7,nullptr)))return 2;
 DDSURFACEDESC2 desc{};desc.dwSize=sizeof(desc);desc.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT|DDSD_MIPMAPCOUNT;desc.dwWidth=desc.dwHeight=16;desc.dwMipMapCount=5;
 desc.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP|DDSCAPS_COMPLEX|DDSCAPS_SYSTEMMEMORY;desc.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);desc.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;desc.ddpfPixelFormat.dwRGBBitCount=32;desc.ddpfPixelFormat.dwRBitMask=0xff0000;desc.ddpfPixelFormat.dwGBitMask=0xff00;desc.ddpfPixelFormat.dwBBitMask=0xff;desc.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000;
 IDirectDrawSurface7* root=nullptr;auto result=dd->CreateSurface(&desc,&root,nullptr);if(FAILED(result)){std::printf("create failed %08lx\n",result);dd->Release();return 2;}
 if(argc>1&&std::strcmp(argv[1],"--factory-oom")==0){
  if(!PatchHeap(addon,true))return 3;heapFailures=0;failedHeapSize=44;failDllHeap=true;void* output=reinterpret_cast<void*>(uintptr_t(1));HRESULT status=E_FAIL;bool escaped=false;
  try{status=reinterpret_cast<Create>(create)(nullptr,&output,IID_IDirectDraw7,nullptr);}catch(const std::bad_alloc&){escaped=true;}
  failDllHeap=false;PatchHeap(addon,false);const bool safe=!escaped&&status==E_OUTOFMEMORY&&!output&&heapFailures>0;
  std::printf("DirectDraw factory allocation failure is contained: %s (escaped %u, output %p, failures %u)\n",safe?"PASS":"FAIL",escaped,output,heapFailures.load());root->Release();dd->Release();return safe?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--depth-cycle")==0){
  DDSURFACEDESC2 depthDesc=desc;depthDesc.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_SYSTEMMEMORY;depthDesc.dwFlags&=~DDSD_MIPMAPCOUNT;depthDesc.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;depthDesc.ddpfPixelFormat.dwZBufferBitDepth=24;depthDesc.ddpfPixelFormat.dwZBitMask=0xffffff;
  const auto baseline=dd->AddRef();dd->Release();IDirectDrawSurface7* a=nullptr,*b=nullptr;if(FAILED(dd->CreateSurface(&depthDesc,&a,nullptr))||FAILED(dd->CreateSurface(&depthDesc,&b,nullptr)))return 3;
  const auto first=a->AddAttachedSurface(b);const auto second=b->AddAttachedSurface(a);a->Release();b->Release();const auto held=dd->AddRef();dd->Release();
  const bool safe=FAILED(first)&&FAILED(second)&&held==baseline;
  std::printf("mutual depth attachments cannot retain a cycle: %s (creator refs %lu; expected %lu)\n",safe?"PASS":"FAIL",held,baseline);
  if(SUCCEEDED(first)&&SUCCEEDED(second)){a->AddRef();a->DeleteAttachedSurface(0,b);a->Release();}
  root->Release();dd->Release();return safe?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--thread-oom")==0){
  if(!PatchHeap(addon,true)){std::puts("HeapAlloc import not found");return 3;}
  ThreadProbe first{root,CreateEventW(nullptr,TRUE,FALSE,nullptr),CreateEventW(nullptr,TRUE,FALSE,nullptr)},second{root,CreateEventW(nullptr,TRUE,FALSE,nullptr),nullptr};
  heapFailures=0;failedHeapSize=8;failDllHeap=true;HANDLE a=CreateThread(nullptr,0,ReadCaps,&first,0,nullptr);const bool firstDone=WaitForSingleObject(first.ready,5000)==WAIT_OBJECT_0;failDllHeap=false;
  HANDLE b=CreateThread(nullptr,0,ReadCaps,&second,0,nullptr);const bool secondDone=WaitForSingleObject(second.ready,500)==WAIT_OBJECT_0;
  const bool okay=firstDone&&secondDone&&SUCCEEDED(first.result)&&SUCCEEDED(second.result)&&heapFailures>0;
  std::printf("diagnostic allocation failure keeps COM lock usable: %s (first %08lx, second completed %u, injected failures %u)\n",okay?"PASS":"FAIL",first.result,secondDone,heapFailures.load());std::fflush(stdout);PatchHeap(addon,false);
  if(!okay)ExitProcess(1);SetEvent(first.release);WaitForSingleObject(a,5000);WaitForSingleObject(b,5000);CloseHandle(a);CloseHandle(b);CloseHandle(first.ready);CloseHandle(first.release);CloseHandle(second.ready);root->Release();dd->Release();return 0;
 }
 if(argc>1&&std::strcmp(argv[1],"--query-oom")==0){
  if(!PatchHeap(addon,true))return 3;heapFailures=0;failedHeapSize=12;failDllHeap=true;IDirect3D7* d3d=reinterpret_cast<IDirect3D7*>(uintptr_t(1));const auto failed=dd->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d));failDllHeap=false;
  const bool safe=failed==E_OUTOFMEMORY&&!d3d&&heapFailures>0;const auto retried=dd->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d));const bool recovered=SUCCEEDED(retried)&&d3d;if(recovered)d3d->Release();
  std::printf("QueryInterface allocation failure returns clean error and recovers: %s\n",safe&&recovered?"PASS":"FAIL");PatchHeap(addon,false);root->Release();dd->Release();return safe&&recovered?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--pixel-formats")==0){
  std::vector<DDPIXELFORMAT> formats;formats.push_back(desc.ddpfPixelFormat);
  auto pf=desc.ddpfPixelFormat;pf.dwFlags=DDPF_RGB;pf.dwRGBBitCount=16;pf.dwRBitMask=0xf800;pf.dwGBitMask=0x7e0;pf.dwBBitMask=0x1f;pf.dwRGBAlphaBitMask=0;formats.push_back(pf);
  pf=desc.ddpfPixelFormat;pf.dwFlags=DDPF_RGB;pf.dwRGBBitCount=24;pf.dwRGBAlphaBitMask=0;formats.push_back(pf);
  pf={};pf.dwSize=sizeof(pf);pf.dwFlags=DDPF_PALETTEINDEXED8|DDPF_RGB;pf.dwRGBBitCount=8;formats.push_back(pf);
  for(char digit:{'1','3','5'}){pf={};pf.dwSize=sizeof(pf);pf.dwFlags=DDPF_FOURCC;pf.dwFourCC=MAKEFOURCC('D','X','T',digit);formats.push_back(pf);}
  unsigned cases=0;bool good=true;
  for(const auto& format:formats)for(DWORD dimension:{1u,2u,3u,4u,17u}){
   auto textureDesc=desc;textureDesc.dwWidth=textureDesc.dwHeight=dimension;textureDesc.ddpfPixelFormat=format;
   IDirectDrawSurface7* source=nullptr;if(FAILED(dd->CreateSurface(&textureDesc,&source,nullptr)))return 3;
   auto plainDesc=textureDesc;plainDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;plainDesc.dwFlags&=~DDSD_MIPMAPCOUNT;IDirectDrawSurface7* copy=nullptr;
   if(FAILED(dd->CreateSurface(&plainDesc,&copy,nullptr)))return 3;
   IDirectDrawSurface7* level=source;level->AddRef();unsigned mip=0;std::vector<unsigned char> base;
   while(level){
    DDSURFACEDESC2 locked{};locked.dwSize=sizeof(locked);if(FAILED(level->Lock(nullptr,&locked,DDLOCK_WAIT,nullptr)))return 3;
    const size_t rows=(format.dwFlags&DDPF_FOURCC)?(locked.dwHeight+3)/4:locked.dwHeight;const size_t bytes=size_t(locked.lPitch)*rows;
    auto* data=static_cast<unsigned char*>(locked.lpSurface);for(size_t i=0;i<bytes;++i)data[i]=static_cast<unsigned char>((i*73+dimension+mip)%251);if(!mip)base.assign(data,data+bytes);level->Unlock(nullptr);
    DDSCAPS2 wanted{};wanted.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;IDirectDrawSurface7* next=nullptr;level->GetAttachedSurface(&wanted,&next);level->Release();level=next;++mip;
   }
   const auto copied=copy->Blt(nullptr,source,nullptr,DDBLT_WAIT,nullptr);DDSURFACEDESC2 locked{};locked.dwSize=sizeof(locked);const auto read=copy->Lock(nullptr,&locked,DDLOCK_WAIT|DDLOCK_READONLY,nullptr);
   const bool same=SUCCEEDED(copied)&&SUCCEEDED(read)&&locked.lpSurface&&!std::memcmp(locked.lpSurface,base.data(),base.size());if(SUCCEEDED(read))copy->Unlock(nullptr);good &=same;
   if(!same)std::printf("pixel roundtrip failed: flags %lx, bits %lu, fourcc %lx, dimension %lu\n",format.dwFlags,format.dwRGBBitCount,format.dwFourCC,dimension);
   source->Release();copy->Release();++cases;
  }
  PALETTEENTRY* invalid=reinterpret_cast<PALETTEENTRY*>(uintptr_t(1));IDirectDrawPalette* rejected=reinterpret_cast<IDirectDrawPalette*>(uintptr_t(1));const auto indexPalette=dd->CreatePalette(DDPCAPS_1BIT|DDPCAPS_8BITENTRIES,invalid,&rejected,nullptr);
  const bool safeIndex=FAILED(indexPalette)&&!rejected;good &=safeIndex;
  std::printf("%u format/dimension/mip copy roundtrips: %s; unsupported byte palette safely rejected: %s\n",cases,good?"PASS":"FAIL",safeIndex?"PASS":"FAIL");root->Release();dd->Release();return good?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--vb-overflow")==0){
  IDirect3D7* d3d=nullptr;if(FAILED(dd->QueryInterface(IID_IDirect3D7,reinterpret_cast<void**>(&d3d))))return 3;
  D3DVERTEXBUFFERDESC vertexDesc{};vertexDesc.dwSize=sizeof(vertexDesc);vertexDesc.dwFVF=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1;vertexDesc.dwNumVertices=0x08000000u;
  IDirect3DVertexBuffer7* buffer=nullptr;const auto status=d3d->CreateVertexBuffer(&vertexDesc,&buffer,0);const bool rejected=FAILED(status)&&!buffer;
  std::printf("vertex-buffer byte count overflow rejected: %s (%08lx)\n",rejected?"PASS":"FAIL",status);if(buffer)buffer->Release();d3d->Release();root->Release();dd->Release();return rejected?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--palette-small")==0){
  const DWORD sizes[]={2,4,16,256},caps[]={DDPCAPS_1BIT,DDPCAPS_2BIT,DDPCAPS_4BIT,DDPCAPS_8BIT};
  SYSTEM_INFO info{};GetSystemInfo(&info);auto* pages=static_cast<unsigned char*>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!pages)return 3;DWORD old;VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old);
  bool okay=true;for(unsigned k=0;k<4;++k){auto* entries=reinterpret_cast<PALETTEENTRY*>(pages+info.dwPageSize-sizes[k]*sizeof(PALETTEENTRY));for(DWORD i=0;i<sizes[k];++i)entries[i]={BYTE(i),BYTE(i+1),BYTE(i+2),0};
   IDirectDrawPalette* palette=nullptr;std::printf("Creating %lu-entry palette at guarded allocation boundary\n",sizes[k]);std::fflush(stdout);const auto status=dd->CreatePalette(caps[k],entries,&palette,nullptr);
   PALETTEENTRY read[256]{};bool correct=SUCCEEDED(status)&&palette&&SUCCEEDED(palette->GetEntries(0,0,sizes[k],read))&&!std::memcmp(entries,read,sizes[k]*sizeof(PALETTEENTRY));
   PALETTEENTRY extra{};if(correct)correct=FAILED(palette->GetEntries(0,sizes[k],1,&extra));std::printf("%lu-entry palette copy and limits: %s\n",sizes[k],correct?"PASS":"FAIL");okay &=correct;if(palette)palette->Release();
  }VirtualFree(pages,0,MEM_RELEASE);root->Release();dd->Release();return okay?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--palette-overflow")==0){
  PALETTEENTRY entries[256]{};IDirectDrawPalette* palette=nullptr;if(FAILED(dd->CreatePalette(DDPCAPS_8BIT,entries,&palette,nullptr)))return 3;
  std::puts("Checking overflowing palette entry range");std::fflush(stdout);const auto read=palette->GetEntries(0,0xffffffffu,1,entries);const auto write=palette->SetEntries(0,0xffffffffu,1,entries);const bool rejected=FAILED(read)&&FAILED(write);
  std::printf("palette entry range overflow rejected: %s\n",rejected?"PASS":"FAIL");palette->Release();root->Release();dd->Release();return rejected?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--self-blt")==0){
  DDSURFACEDESC2 locked{};locked.dwSize=sizeof(locked);if(FAILED(root->Lock(nullptr,&locked,DDLOCK_WAIT,nullptr)))return 3;
  for(unsigned i=0;i<16;++i)static_cast<uint32_t*>(locked.lpSurface)[i]=0xff000000u+i;root->Unlock(nullptr);
  RECT source{0,0,15,1},target{1,0,16,1};if(FAILED(root->Blt(&target,root,&source,DDBLT_WAIT,nullptr)))return 3;
  locked={};locked.dwSize=sizeof(locked);if(FAILED(root->Lock(nullptr,&locked,DDLOCK_WAIT|DDLOCK_READONLY,nullptr)))return 3;
  bool correct=true;for(unsigned i=1;i<16;++i)if(static_cast<uint32_t*>(locked.lpSurface)[i]!=(0xff000000u+i-1))correct=false;root->Unlock(nullptr);
  std::printf("overlapping CPU self blit preserves source: %s\n",correct?"PASS":"FAIL");root->Release();dd->Release();return correct?0:1;
 }
 if(argc>1&&std::strcmp(argv[1],"--bounds")==0){
  unsigned failures=0;
  for(RECT rect: {RECT{-1,0,1,1},RECT{0,-1,1,1},RECT{0,0,17,1},RECT{0,0,1,17},RECT{2,0,1,1},RECT{0,0,0,1}}){
   DDSURFACEDESC2 locked{};locked.dwSize=sizeof(locked);const auto status=root->Lock(&rect,&locked,DDLOCK_WAIT|DDLOCK_READONLY,nullptr);
   const bool rejected=FAILED(status);std::printf("invalid lock [%ld,%ld,%ld,%ld]: %s (%08lx)\n",rect.left,rect.top,rect.right,rect.bottom,rejected?"PASS":"FAIL",status);if(!rejected){++failures;root->Unlock(nullptr);}
  }
  for(auto size: {std::pair<DWORD,DWORD>{0,16},{16,0},{0x40000004u,16}}){
   auto invalid=desc;invalid.dwWidth=size.first;invalid.dwHeight=size.second;IDirectDrawSurface7* bad=nullptr;const auto status=dd->CreateSurface(&invalid,&bad,nullptr);
   const bool rejected=FAILED(status)&&!bad;std::printf("invalid dimensions %lu x %lu: %s (%08lx)\n",size.first,size.second,rejected?"PASS":"FAIL",status);if(bad)bad->Release();if(!rejected)++failures;
  }
  root->Release();dd->Release();return failures?1:0;
 }
 if(argc>1&&std::strcmp(argv[1],"--blt-bounds")==0){
  DDSURFACEDESC2 destDesc=desc;destDesc.dwFlags&=~DDSD_MIPMAPCOUNT;destDesc.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
  IDirectDrawSurface7* destination=nullptr;if(FAILED(dd->CreateSurface(&destDesc,&destination,nullptr)))return 3;
  unsigned failures=0;RECT target{0,0,1,1};
  for(RECT source: {RECT{-1,0,0,1},RECT{0,-1,1,0},RECT{16,0,17,1},RECT{0,16,1,17},RECT{2,0,1,1}}){
   const auto status=destination->Blt(&target,root,&source,DDBLT_WAIT,nullptr);const bool rejected=FAILED(status);std::printf("invalid blit source [%ld,%ld,%ld,%ld]: %s (%08lx)\n",source.left,source.top,source.right,source.bottom,rejected?"PASS":"FAIL",status);if(!rejected)++failures;
  }
  destination->Release();root->Release();dd->Release();return failures?1:0;
 }
 if(argc>1&&std::strcmp(argv[1],"--depth-repeat")==0){
  DDSURFACEDESC2 depthDesc=desc;depthDesc.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_SYSTEMMEMORY;depthDesc.dwFlags&=~DDSD_MIPMAPCOUNT;depthDesc.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;depthDesc.ddpfPixelFormat.dwZBufferBitDepth=24;depthDesc.ddpfPixelFormat.dwZBitMask=0xffffff;
  IDirectDrawSurface7* depth=nullptr;if(FAILED(dd->CreateSurface(&depthDesc,&depth,nullptr)))return 3;
  if(FAILED(root->AddAttachedSurface(depth)))return 3;depth->Release();
  std::puts("Repeating the same attachment with its existing owner reference");std::fflush(stdout);
  const HRESULT again=root->AddAttachedSurface(depth);const bool safe=again==DDERR_SURFACEALREADYATTACHED;
  std::printf("duplicate depth attachment: %s (%08lx)\n",safe?"PASS":"FAIL",again);std::fflush(stdout);
  if(!safe)ExitProcess(1);root->Release();
  IDirectDrawSurface7* self=nullptr;if(FAILED(dd->CreateSurface(&depthDesc,&self,nullptr)))return 3;const auto attached=self->AddAttachedSurface(self);const bool rejected=attached==DDERR_CANNOTATTACHSURFACE;
  std::printf("self depth attachment rejected: %s\n",rejected?"PASS":"FAIL");if(!rejected)ExitProcess(1);self->Release();dd->Release();return 0;
 }
 if(argc>1&&std::strcmp(argv[1],"--stress")==0){
  root->Release();
  for(unsigned iteration=0;iteration<20000;++iteration){
   IDirectDrawSurface7* chain[5]{};IUnknown* extra=nullptr;
   if(FAILED(dd->CreateSurface(&desc,&chain[0],nullptr)))return 3;
   DDSCAPS2 wanted{};wanted.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;
   for(unsigned i=1;i<5;++i)if(FAILED(chain[i-1]->GetAttachedSurface(&wanted,&chain[i])))return 3;
   if(FAILED(chain[3]->QueryInterface(IID_IUnknown,reinterpret_cast<void**>(&extra))))return 3;
   chain[2]->AddRef();chain[2]->Release();
   // Coprime stride visits every interface in a different release order.
   for(unsigned j=0;j<5;++j){const unsigned i=(iteration+j*3)%5;chain[i]->Release();}
   const ULONG last=extra->Release();const ULONG owner=dd->AddRef();dd->Release();
   if(last!=0||owner!=2){std::printf("chain imbalance at %u: child %lu owner %lu\n",iteration,last,owner);return 1;}
  }
  std::puts("20,000 mip chains / mixed release and QueryInterface orders: PASS");dd->Release();return 0;
 }
 if(argc>1&&std::strcmp(argv[1],"--oom")==0){
  root->Release();
  void* reservations[512]{};unsigned reserved=0;SYSTEM_INFO system{};GetSystemInfo(&system);uintptr_t address=0;
  while(address<uintptr_t(system.lpMaximumApplicationAddress)&&reserved<512){MEMORY_BASIC_INFORMATION region{};if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)))break;const uintptr_t next=uintptr_t(region.BaseAddress)+region.RegionSize;if(next<=address)break;
   if(region.State==MEM_FREE&&region.RegionSize>=(1u<<20)){const uintptr_t first=(uintptr_t(region.BaseAddress)+system.dwAllocationGranularity-1)/system.dwAllocationGranularity*system.dwAllocationGranularity;const size_t bytes=(next-first)/system.dwAllocationGranularity*system.dwAllocationGranularity;void* block=bytes?VirtualAlloc(reinterpret_cast<void*>(first),bytes,MEM_RESERVE,PAGE_NOACCESS):nullptr;if(block)reservations[reserved++]=block;}address=next;}
  static IDirectDrawSurface7* live[32768]{};unsigned made=0;HRESULT failed=DD_OK;IDirectDrawSurface7* rejected=nullptr;
  for(;made<32768;++made){rejected=reinterpret_cast<IDirectDrawSurface7*>(uintptr_t(1));failed=dd->CreateSurface(&desc,&rejected,nullptr);if(FAILED(failed))break;live[made]=rejected;}
  for(unsigned i=0;i<made;++i)live[i]->Release();for(unsigned i=0;i<reserved;++i)VirtualFree(reservations[i],0,MEM_RELEASE);
  const ULONG owner=dd->AddRef();dd->Release();const bool clean=failed==E_OUTOFMEMORY&&!rejected&&owner==2;
  std::printf("surface allocation exhaustion: %u chains, result %08lx, output %p, creator refs %lu: %s\n",made,failed,rejected,owner,clean?"PASS":"FAIL");dd->Release();return clean?0:1;
 }
 if(parentOnly){
  const ULONG held=dd->Release();std::printf("creator retained while surface exists: %s (remaining %lu)\n",held?"PASS":"FAIL",held);
  if(!held){root->Release();return 1;}
  void* creator=nullptr;const bool queried=SUCCEEDED(root->GetDDInterface(&creator))&&creator;
  if(creator)static_cast<IUnknown*>(creator)->Release();const auto released=root->Release();
  std::printf("creator interface survives parent release: %s; final chain refs %lu\n",queried?"PASS":"FAIL",released);return queried&&released==0?0:1;
 }
 DDSCAPS2 caps{};caps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;IDirectDrawSurface7* mip=nullptr;if(FAILED(root->GetAttachedSurface(&caps,&mip))||!mip)return 2;
 const ULONG remaining=root->Release();std::printf("root retained while mip interface exists: %s (remaining %lu)\n",remaining?"PASS":"FAIL",remaining);
 // Old implementation freed root here; avoid dereferencing its dangling pointer.
 if(!remaining){mip->Release();dd->Release();return 1;}
 DDSURFACEDESC2 locked{};locked.dwSize=sizeof(locked);const auto lock=mip->Lock(nullptr,&locked,DDLOCK_WAIT,nullptr);bool okay=SUCCEEDED(lock)&&locked.lpSurface;
 if(okay){static_cast<unsigned char*>(locked.lpSurface)[0]=0x71;okay=SUCCEEDED(mip->Unlock(nullptr));}
 std::printf("mip remains lockable after root release: %s\n",okay?"PASS":"FAIL");const ULONG last=mip->Release();std::printf("chain freed after final mip release: %s (remaining %lu)\n",last==0?"PASS":"FAIL",last);dd->Release();return okay&&last==0?0:1;
}
