// Test executable only. Counts live C++ allocations separately from driver/CRT
// heaps; caller addresses allow a growing site to be identified in the PDB.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <malloc.h>
#include <atomic>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
namespace {
constexpr uint32_t kSites=8192,kCookie=0xa0c0be52;
struct Site {std::atomic<void*> caller{nullptr};std::atomic<int64_t> bytes{0},count{0};};
Site sites[kSites]{};
std::atomic<int64_t> liveBytes{0},liveCount{0};
struct Header {void* raw;size_t bytes;uint32_t site,cookie;};
static_assert(sizeof(Header)==16);
uint32_t FindSite(void* caller){
 uint32_t at=(uint32_t(uintptr_t(caller))>>4)%kSites;
 for(uint32_t n=0;n<kSites;++n){void* existing=sites[at].caller.load(std::memory_order_relaxed);if(existing==caller)return at;
  if(!existing&&sites[at].caller.compare_exchange_strong(existing,caller,std::memory_order_relaxed))return at;at=(at+1)%kSites;
 }return 0;
}
void* Allocate(size_t bytes,size_t alignment,void* caller){
 if(!bytes)bytes=1;if(alignment<16)alignment=16;
 const size_t prefix=alignment>sizeof(Header)?alignment:sizeof(Header);
 if(bytes>SIZE_MAX-prefix)throw std::bad_alloc();void* raw=_aligned_malloc(bytes+prefix,alignment);if(!raw)throw std::bad_alloc();
 void* result=static_cast<unsigned char*>(raw)+prefix;auto* h=reinterpret_cast<Header*>(result)-1;h->raw=raw;h->bytes=bytes;h->site=FindSite(caller);h->cookie=kCookie;
 liveBytes.fetch_add(bytes,std::memory_order_relaxed);liveCount.fetch_add(1,std::memory_order_relaxed);sites[h->site].bytes.fetch_add(bytes,std::memory_order_relaxed);sites[h->site].count.fetch_add(1,std::memory_order_relaxed);return result;
}
void Free(void* ptr) noexcept{
 if(!ptr)return;auto* h=static_cast<Header*>(ptr)-1;if(h->cookie!=kCookie)std::abort();h->cookie=0;
 liveBytes.fetch_sub(h->bytes,std::memory_order_relaxed);liveCount.fetch_sub(1,std::memory_order_relaxed);sites[h->site].bytes.fetch_sub(h->bytes,std::memory_order_relaxed);sites[h->site].count.fetch_sub(1,std::memory_order_relaxed);_aligned_free(h->raw);
}
}
__declspec(noinline) void* operator new(size_t n){return Allocate(n,16,_ReturnAddress());}
__declspec(noinline) void* operator new[](size_t n){return Allocate(n,16,_ReturnAddress());}
__declspec(noinline) void* operator new(size_t n,std::align_val_t a){return Allocate(n,size_t(a),_ReturnAddress());}
__declspec(noinline) void* operator new[](size_t n,std::align_val_t a){return Allocate(n,size_t(a),_ReturnAddress());}
__declspec(noinline) void* operator new(size_t n,const std::nothrow_t&) noexcept{try{return Allocate(n,16,_ReturnAddress());}catch(...){return nullptr;}}
__declspec(noinline) void* operator new[](size_t n,const std::nothrow_t&) noexcept{try{return Allocate(n,16,_ReturnAddress());}catch(...){return nullptr;}}
void operator delete(void* p) noexcept{Free(p);}void operator delete[](void* p) noexcept{Free(p);}
void operator delete(void* p,size_t) noexcept{Free(p);}void operator delete[](void* p,size_t) noexcept{Free(p);}
void operator delete(void* p,std::align_val_t) noexcept{Free(p);}void operator delete[](void* p,std::align_val_t) noexcept{Free(p);}
void operator delete(void* p,size_t,std::align_val_t) noexcept{Free(p);}void operator delete[](void* p,size_t,std::align_val_t) noexcept{Free(p);}
void operator delete(void* p,const std::nothrow_t&) noexcept{Free(p);}void operator delete[](void* p,const std::nothrow_t&) noexcept{Free(p);}
extern "C" uint64_t AoAuditHeapLiveBytes(){return uint64_t(liveBytes.load(std::memory_order_relaxed));}
extern "C" uint64_t AoAuditHeapLiveCount(){return uint64_t(liveCount.load(std::memory_order_relaxed));}
extern "C" void AoAuditHeapSnapshot(const char* path){
 FILE* out=std::fopen(path,"w");if(!out)return;std::fprintf(out,"caller_rva,live_bytes,live_allocations\n");
 for(auto& site:sites){const auto bytes=site.bytes.load(std::memory_order_relaxed),count=site.count.load(std::memory_order_relaxed);if(bytes||count)std::fprintf(out,"0x%08lx,%lld,%lld\n",static_cast<unsigned long>(uintptr_t(site.caller.load())-uintptr_t(GetModuleHandleW(nullptr))),(long long)bytes,(long long)count);}
 std::fclose(out);
}

extern "C" bool AoAuditHeapSelfTest(){
 const uint64_t before=AoAuditHeapLiveBytes(),count=AoAuditHeapLiveCount();
 void* normal=::operator new(257);void* aligned=::operator new(513,std::align_val_t(64));
 const bool measured=AoAuditHeapLiveBytes()==before+770 && AoAuditHeapLiveCount()==count+2 && !(uintptr_t(aligned)%64);
 ::operator delete(normal);::operator delete(aligned,std::align_val_t(64));
 return measured&&AoAuditHeapLiveBytes()==before&&AoAuditHeapLiveCount()==count;
}
