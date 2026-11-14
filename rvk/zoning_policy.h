#pragma once
#include <cstdint>
#include <algorithm>

namespace rvk::zoning {
// The counter advances only for successfully presented world frames.
struct Reveal {
    uint64_t baseline = 0, began = 0;
    bool active = false, settling = false, dismissed = false;
    void Start(uint64_t now) { began = now; active = true; settling = false; dismissed = false; }
    void Loaded(uint64_t worlds) { baseline = worlds; settling = true; }
    bool Ready(uint64_t worlds) const { return active && settling && worlds >= baseline && worlds - baseline >= 4; }
    bool Continue(uint64_t worlds) {
        if (!Ready(worlds)) return false;
        dismissed = true; return true;
    }
    bool Visible(uint64_t now, uint64_t worlds, bool waitForInput = false) const {
        return active && !dismissed && (Ready(worlds) ? waitForInput : now - began < 90000);
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
struct ReadyFade {
    uint64_t began = 0;
    bool started = false;
    float Step(uint64_t now, bool ready) {
        if (!started) {
            if (!ready) return 0;
            began = now; started = true;
        }
        const float t = std::clamp(float(now-began)/900.f,0.f,1.f);
        return t*t*(3.f-2.f*t);
    }
};
}
