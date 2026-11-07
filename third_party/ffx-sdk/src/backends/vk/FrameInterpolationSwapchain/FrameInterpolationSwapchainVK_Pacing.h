// Timing for the serialized NVIDIA presentation path. No Vulkan/Windows state.
#pragma once
#include <cstdint>

inline int64_t frameProductionInterval(int64_t previousReady, int64_t ready,
                                      int64_t previousPairCompleted, bool serialized, int64_t cpuProduction = 0)
{
    if (previousReady <= 0 || ready <= previousReady)
        return 0;
    // The previous generated/real pair must finish before new game work can be
    // submitted. Counting that enforced wait as new production time feeds our
    // own pacing delay back into the next delay. Ignore only a completion that
    // belongs to this interval; stale/reset timestamps retain the stock timing.
    if (serialized && previousPairCompleted > previousReady && previousPairCompleted <= ready) {
        const int64_t interval = ready - previousPairCompleted;
        // CPU recording can overlap the previous presentation. Retain that
        // independent production time so CPU-limited clients keep useful spacing.
        int64_t floor = cpuProduction > 0 ? cpuProduction : 0;
        if (floor > ready - previousReady) floor = ready - previousReady;
        return interval > floor ? interval : floor;
    }
    return ready - previousReady;
}
