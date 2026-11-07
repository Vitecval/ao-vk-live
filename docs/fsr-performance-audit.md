# Preview 23 revision 12: serialized frame-generation pacing

The working F: installation had frame generation enabled, upscaling disabled,
render scale 100%, and TAA enabled. In the user's stationary 1914x1051 scene on
the RTX 2070 SUPER, revision 10 rendered about 12.4–12.9 FPS with generation on
and 25.7 FPS with it off. Scene GPU time remained 38.2–38.5 ms in both cases.
The measurement excluded the earlier long inactive pause. Other running game
clients were left alone; these figures describe this machine and session.

The NVIDIA compatibility path deliberately waits until both actual driver
presentation calls return before allowing later game GPU submissions. The stock
SDK pacer measured consecutive interpolation-ready times, which include that
previous pair's wait. It then used half of that interval for the next spacing
target. In a GPU-limited serialized loop this feeds the pacer's own delay back
into its next estimate, approaching an extra full production interval.

Revision 12 records completion after the actual pair returns and excludes that
part of the interval from the production estimate. A separate game-thread CPU
measurement, excluding the previous render-worker wait, provides a lower bound:
CPU work that overlaps presentation must still contribute to frame spacing.
Only the serialized NVIDIA path uses the correction. The unsynchronized SDK
timing used on other vendors is unchanged. Invalid/stale timestamps fall back
to the original interval; long stalls still reset the moving average.

All previous guards remain: full generated/real GPU pair submission before WSI,
bounded GPU-ready fence outside queue locks, window-owner WSI calls, sent-message
pumping, acquisition/handoff limits, and waiting for actual pair completion
before later game GPU work. No global driver or overlay settings changed.

## Controlled measurements

The fixture uses the real x86 renderer and driver at 1914x1051, warms 120 frames,
then measures 240 completed rendered frames. It counts successful driver present
returns, not physical display scanout. GPU and CPU comparisons run sequentially.
The heavy scene draws 16 full-screen depth-enabled triangles with always-pass
depth testing. The comparison executable changes only the production-estimate
selection back to stock timing; all guards and the test workload are identical.

| GPU-heavy fixture | Stock pacing | Corrected pacing |
| --- | ---: | ---: |
| Generation off, rendered/presented FPS | 49.14 | 48.92 |
| Generation on, rendered FPS | 19.84 | 26.88 |
| Generation on, successful presents/sec | 39.68 | 53.76 |
| Generation on, 95th-percentile present interval | 26.57 ms | 27.03 ms |

That is a 35.5% increase in frame-generation throughput in this workload. It is
not an actual-game speedup claim. CPU-budget and freeze-regression evidence is
stored under `build/preview23/checks/fsr-performance` in the owning workspace.

The unchanged revision-10 timing fixture and corrected fixture also ran with
fixed game-thread CPU budgets. Both used the same single-triangle scene:

| CPU work per frame | Stock generated presents/sec | Corrected presents/sec |
| --- | ---: | ---: |
| 8 ms | 151.49 | 186.59 |
| 16 ms | 109.39 | 111.84 |
| 32 ms | 58.04 | 59.07 |

These are single sequential samples, not statistical confidence intervals.
Early exploratory runs with depth disabled did not exercise interpolation and
were rejected by the fixture; an oversized exploratory workload hit the test's
45-second limit and was stopped. Neither is a passing performance result.
`baseline-zero-*` is an early mislabeled corrected-timing run and is excluded.

## Limits and handoff

All 16 x86 regression programs/scenarios passed: actual generation, queued-frame
overlap tracing (zero), owner WSI, whole-pair submission, window replacement,
resize/toggles, acquisition limits, handoff timeout, slow presentation, allocation
failure, pacing boundaries, GPU-ready fencing, worker faults and message pumping.
Six x64 core/synchronization validation scenarios passed with zero validation
errors and the official layer confirmed loaded: overlap tracing, window
replacement, threaded generation, sent-message handoff, FSR/FG lifecycle and
frame-generation initialization failure. These diagnostic children disabled
implicit layers and reused the earlier shader-compatible pipeline cache.
An initial validation launch with implicit layers reproduced the overlay
framebuffer anomalies already recorded in the revision-10 audit. Empty-cache
validation startup also exceeded the 60-second fixture budget during shader
compilation; neither incomplete launch is counted as a passing validation case.
Normal x86 regression children retained the installed implicit layers.

Serialized presentation still prevents full GPU/presentation overlap. Generated
and real frames can have unequal intervals; the higher throughput does not imply
uniform scanout, lower input latency, or faster game simulation. The 600-frame
diagnostic now reports rendered and presented rates separately. Assess perceived
smoothness and responsiveness in the actual game as well as the counters.

The user reported revision 10 working after the earlier freezes. Controlled
checks cannot prove that every driver hang is eliminated. Physical validation
here is limited to this NVIDIA GPU; AMD/Intel performance certification remains
pending. Frame generation remains experimental and off by default for fresh
installations. Final textures and final-package validation remain required
before public release. Living remains excluded, and Setup 6 is retained.
