// Allocation-safe maintenance of the optional per-object acceleration cache.
#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace rvk::detail {

template<class Entries, class Keys, class Keep>
bool TryRehashObjectCache(Entries& entries, Keys& keys, size_t size, Keep keep, size_t& count)
{
    using Entry=typename Entries::value_type;
    static_assert(std::is_nothrow_copy_assignable_v<Entry>);
    if (!size || (size & (size-1))) return false;
    try {
        // Build both tables before touching the working cache. An allocation
        // failure leaves its entries, keys, successor links and count intact.
        Entries next(size,entries.get_allocator());
        Keys nextKeys(size,keys.get_allocator());
        size_t nextCount=0;
        for (const Entry& entry : entries) {
            if (!entry.key || !keep(entry)) continue;
            if (nextCount+1>=size) return false;
            size_t i=size_t(entry.key)&(size-1);
            while (nextKeys[i]) i=(i+1)&(size-1);
            next[i]=entry;next[i].next=~uint32_t(0);nextKeys[i]=entry.key;
            ++nextCount;
        }
        entries.swap(next);keys.swap(nextKeys);count=nextCount;
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

template<class Entries, class Keys, class Keep>
size_t PruneObjectCacheInPlace(Entries& entries, Keys& keys, Keep keep)
{
    using Entry=typename Entries::value_type;
    static_assert(std::is_nothrow_copy_assignable_v<Entry>);
    const size_t size=keys.size();
    if (!size) return 0;
    const size_t mask=size-1;
    for (size_t i=0;i<size;++i) {
        while (keys[i] && !keep(entries[i])) {
            size_t hole=i;
            entries[hole]=Entry{};keys[hole]=0;
            // Back-shift the probe chain: no tombstones, new table or scratch
            // allocation. Distances wrap with the power-of-two table size.
            for (size_t j=(hole+1)&mask;keys[j];j=(j+1)&mask) {
                const size_t home=size_t(keys[j])&mask;
                if (((j-home)&mask)<((j-hole)&mask)) continue;
                entries[hole]=entries[j];keys[hole]=keys[j];
                entries[j]=Entry{};keys[j]=0;hole=j;
            }
        }
    }
    size_t count=0;
    for (auto& entry : entries) {
        entry.next=~uint32_t(0); // pruning can move an entry; predictions are invalid
        count+=entry.key!=0;
    }
    return count;
}

} // namespace rvk::detail
