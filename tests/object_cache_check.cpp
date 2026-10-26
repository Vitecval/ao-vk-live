#include "object_cache.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

namespace {
int failAfter=-1;
size_t allocations=0;
template<class T> struct FailAllocator {
    using value_type=T;
    FailAllocator()=default;
    template<class U> FailAllocator(const FailAllocator<U>&) {}
    T* allocate(size_t n) {
        ++allocations;
        if (failAfter==0) throw std::bad_alloc();
        if (failAfter>0) --failAfter;
        return std::allocator<T>{}.allocate(n);
    }
    void deallocate(T* p,size_t n) {std::allocator<T>{}.deallocate(p,n);}
    template<class U> bool operator==(const FailAllocator<U>&) const {return true;}
};
struct Entry {uint64_t key=0,frame=0;uint32_t next=~0u,payload=0;};
using Entries=std::vector<Entry,FailAllocator<Entry>>;
using Keys=std::vector<uint64_t,FailAllocator<uint64_t>>;
void insert(Entries& entries,Keys& keys,uint64_t key,uint64_t frame) {
    size_t i=size_t(key)&(keys.size()-1);
    while (keys[i]) i=(i+1)&(keys.size()-1);
    keys[i]=key;entries[i]={key,frame,0,uint32_t(key^0xabc123)};
}
bool find(const Entries& entries,const Keys& keys,uint64_t key) {
    for(size_t i=size_t(key)&(keys.size()-1);keys[i];i=(i+1)&(keys.size()-1))
        if(keys[i]==key) return entries[i].key==key && entries[i].payload==uint32_t(key^0xabc123);
    return false;
}
}
int main() {
    std::mt19937 random(12345);
    for(unsigned scenario=0;scenario<200;++scenario) {
        Entries entries(1024);Keys keys(1024);
        std::vector<uint64_t> identities;
        for(unsigned i=0;i<400;++i) {
            // Include dense wraparound collisions as well as distributed keys.
            uint64_t key=scenario%2 ? uint64_t(i+1)*1024+1020 : uint64_t(random())+1;
            insert(entries,keys,key,i%5);identities.push_back(key);
        }
        const auto before=allocations;failAfter=0;
        size_t count=rvk::detail::PruneObjectCacheInPlace(entries,keys,[](const Entry& e){return e.frame>=3;});
        assert(count==160 && allocations==before);
        failAfter=-1;
        for(size_t i=0;i<identities.size();++i) assert(find(entries,keys,identities[i])==(i%5>=3));
        for(size_t i=0;i<entries.size();++i) assert(entries[i].key==keys[i] && entries[i].next==~0u);
        const auto* originalEntries=entries.data();const auto* originalKeys=keys.data();
        for(int fail=0;fail<2;++fail) {
            failAfter=fail;
            assert(!rvk::detail::TryRehashObjectCache(entries,keys,2048,[](const Entry&){return true;},count));
            failAfter=-1;
            assert(entries.data()==originalEntries && keys.data()==originalKeys && count==160);
            for(size_t i=0;i<identities.size();++i) assert(find(entries,keys,identities[i])==(i%5>=3));
        }
        assert(rvk::detail::TryRehashObjectCache(entries,keys,2048,[](const Entry&){return true;},count));
        assert(count==160 && entries.size()==2048 && keys.size()==2048);
        for(size_t i=0;i<identities.size();++i) assert(find(entries,keys,identities[i])==(i%5>=3));
        assert(rvk::detail::PruneObjectCacheInPlace(entries,keys,[](const Entry&){return false;})==0);
    }
    std::puts("object cache: 200 collision/wraparound prune cases, no-allocation cleanup, both allocation-failure points and retry passed");
}
