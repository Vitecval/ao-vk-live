#include "native/attachment_index.h"
#include <cstdio>

int main()
{
    using rnative::scene::attachments::Index;
    int characterA, characterB, heldFrame, otherFrame;
    Index index;
    std::unordered_map<const void*, bool> live{{&characterA, true}, {&characterB, true}};
    std::unordered_map<const void*, std::vector<const void*>> maps;
    maps[&characterA] = {&heldFrame};
    auto valid = [&](const void* p) { return live[p]; };
    unsigned reads = 0;
    auto read = [&](const void* p) { ++reads; return maps[p]; };
    index.Refresh(&characterA, 17, valid, read);
    if (index.frames.at(&heldFrame) != &characterA) return 1;
    const auto stable = index.generation;
    for (unsigned frame = 18; frame < 3018; ++frame) {
        index.Prune(frame, valid);
        // A weapon may be submitted before its character: positive ownership
        // refreshes the live map, once per render, without invalidating it.
        index.Refresh(&characterA, frame, valid, read);
        const auto before = reads;
        index.Refresh(&characterA, frame, valid, read);
        if (reads != before || index.generation != stable || index.frames.at(&heldFrame) != &characterA) return 2;
    }
    // The held frame's address now belongs to a whompa. Its former character
    // is alive but no longer references it. Remove ownership before its draw.
    maps[&characterA].clear();
    index.Refresh(&characterA, 3018, valid, read);
    if (index.frames.count(&heldFrame) || index.generation == stable) return 3;
    maps[&characterA] = {&otherFrame};
    index.Refresh(&characterA, 3019, valid, read);
    live[&characterA] = false;  // character address recycled into another class
    index.Prune(3020, valid);
    if (!index.owners.empty() || !index.frames.empty()) return 4;
    // No character need be drawn, and cleanup cannot depend on landing on a
    // particular render-number modulus (the old cleanup gate did).
    maps[&characterB] = {&heldFrame};
    index.Refresh(&characterB, 3021, valid, read);
    index.Prune(5022, valid);
    if (!index.owners.empty() || !index.frames.empty()) return 5;
    // Reassignment must not let the previous owner's expiry erase a new owner.
    live[&characterA] = true;
    maps[&characterA] = maps[&characterB] = {&heldFrame};
    index.Refresh(&characterA, 5023, valid, read);
    index.Refresh(&characterB, 5023, valid, read);
    index.Forget(&characterA);
    if (index.frames.at(&heldFrame) != &characterB) return 6;
    std::puts("attachment ownership: stable gear retained; recycled frames/owners and offscreen expiry invalidate cached verdicts");
}
