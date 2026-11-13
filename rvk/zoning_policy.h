#pragma once
#include <cstdint>
#include <algorithm>

namespace rvk::zoning {
// The counter advances only for successfully presented world frames.
struct Reveal {
    uint64_t baseline = 0, began = 0;
    bool active = false, settling = false;
    void Start(uint64_t now) { began = now; active = true; settling = false; }
    void Loaded(uint64_t worlds) { baseline = worlds; settling = true; }
    bool Visible(uint64_t now, uint64_t worlds) const {
        return active && now - began < 90000 && (!settling || worlds - baseline < 4);
    }
};

struct Fade {
    uint64_t updated = 0;
    float progress = 0;
    bool initialized = false;
    float Step(uint64_t now, bool show) {
        const uint64_t elapsed = initialized ? now - updated : 0;
        updated = now; initialized = true;
        progress = std::clamp(progress + (show ? float(elapsed)/180.f : -float(elapsed)/320.f), 0.f, 1.f);
        return progress * progress * (3.f - 2.f * progress);
    }
};
}
