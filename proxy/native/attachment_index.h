#pragma once
#include <cstdint>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rnative::scene::attachments {
// Frames and character visuals are game-owned addresses, and can be recycled.
// Refresh a positive lookup from the live owner's map before trusting it.
class Index {
public:
    struct Entry { uint32_t stamp = 0; std::vector<const void*> frames; };
    std::unordered_map<const void*, Entry> owners;
    std::unordered_map<const void*, const void*> frames;
    uint32_t generation = 0;

    void Forget(const void* owner) {
        const auto entry = owners.find(owner);
        if (entry == owners.end()) return;
        for (const void* frame : entry->second.frames) {
            auto at = frames.find(frame);
            if (at != frames.end() && at->second == owner) frames.erase(at);
        }
        owners.erase(entry);
        ++generation;
    }

    template<class Valid, class Read>
    void Refresh(const void* owner, uint32_t now, Valid valid, Read read) {
        if (!valid(owner)) { Forget(owner); return; }
        const auto old = owners.find(owner);
        if (old != owners.end() && old->second.stamp == now) return;
        auto current = read(owner);
        auto& entry = owners[owner];
        entry.stamp = now;
        if (entry.frames == current) return;
        for (const void* frame : entry.frames) {
            auto at = frames.find(frame);
            if (at != frames.end() && at->second == owner) frames.erase(at);
        }
        for (const void* frame : current) frames[frame] = owner;
        entry.frames = std::move(current);
        ++generation;
    }

    template<class Valid>
    void Prune(uint32_t now, Valid valid) {
        for (auto at = owners.begin(); at != owners.end();) {
            const auto next = std::next(at);
            if (now - at->second.stamp > 2000 || !valid(at->first)) Forget(at->first);
            at = next;
        }
    }
};
}
