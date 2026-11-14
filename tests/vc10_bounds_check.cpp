// Isolated VC10-layout container checks with a deterministic quarantined test heap.
#include "native/vc10.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <stdexcept>
static std::unordered_map<void*,size_t> blocks;
static std::vector<void*> retired;
static size_t requests=0,lastBytes=0;
namespace rnative::vc10 {
void* Allocate(size_t bytes){++requests;lastBytes=bytes;if(bytes>(1u<<20))throw std::bad_alloc();auto n=(std::max)(bytes,size_t(64));void* p=std::malloc(n);if(!p)throw std::bad_alloc();blocks[p]=n;std::memset(p,0xcc,n);return p;}
void Free(void* p){if(!p)return;auto found=blocks.find(p);if(found==blocks.end())std::abort();std::memset(p,0xdd,found->second);blocks.erase(found);retired.push_back(p);}
}
int main(){
 using namespace rnative::vc10;
 Vector<uint32_t> values{};values.push_back(0x12345678);values.push_back(values[0]);
 const bool alias=values.size()==2&&values[0]==0x12345678&&values[1]==0x12345678;std::printf("self-reference append during growth: %s\n",alias?"PASS":"FAIL");values.release();
 Vector<uint16_t> overflow{};const auto prior=requests;bool rejected=false;
 try{overflow.reserve(SIZE_MAX/sizeof(uint16_t)+1);}catch(const std::length_error&){rejected=true;}
 const bool bounded=rejected&&requests==prior&&!overflow.first;std::printf("capacity overflow rejected before allocator: %s (last request %zu bytes)\n",bounded?"PASS":"FAIL",lastBytes);overflow.release();
 for(auto* p:retired)std::free(p);return alias&&bounded?0:1;
}
