# Preview 23 revision 14: overlap CPU preparation, preserve GPU ordering

Revision 14 moves the NVIDIA actual-presentation completion wait from the end
of SDK queuePresent to the next GPU submission boundary. All four engine submit
sites use one guarded helper; the SDK logical acquisition is gated separately
because it also submits a semaphore signal. Resource drains and swapchain teardown
also wait for actual completion. CPU command recording can therefore overlap the
previous GPU/presentation interval. Later GPU commands still cannot be queued
while the previous actual WSI calls are outstanding. The old wait placement can
be selected with process-local `RANDYVK_DEFER_PRESENT_GUARD=0` for diagnosis.

Pending completion tracks only asynchronous interpolation entries; inline
passthrough presentations have already returned. Timeout/error handling retains
the five-second bound and sticky failure, and outdated swapchains propagate to
recreation. The presentation counter is atomic for concurrent CPU recording.
Guard wait duration is included in the CPU profile. An explicit message-aware
presentation drain allows benchmarks to include their last actual WSI returns.

The user clarified that the reported 38% GPU usage came from the in-game readout.
That value was scene timestamp duration multiplied by rendered FPS and excluded
SDK interpolation/composition. It was not total hardware utilization. During
generation the HUD now says `scene GPU`, omits this incomplete percentage and
labels the primary rate as rendered FPS.

| Matched fixture: 8 ms renderer-worker delay, same binary | Wait at queuePresent end | Wait before GPU submission |
| --- | ---: | ---: |
| Generation off, real/presented FPS | 61.79 | 61.30 |
| Generation on, real FPS | 23.28 | 32.87 |
| Generation on, presentation requests/sec | 46.57 | 65.74 |
| Generation on, 95th-percentile request interval | 29.98 ms | 22.03 ms |

This is a 41.2% throughput improvement in a fixture with injected worker delay,
not an actual-game claim or proof of physical scanout cadence. A separate
32 ms game-thread delay case was similar (58.82 versus 57.73 requests/sec), rather
than exhibiting the renderer-delay gain. GPU shaders and scene quality settings are unchanged.

Driver-timing experiments are archived in the workspace's
`checks/driver-pacing-research`. The ratified relative-timing path increased
request counts but provided only one nonzero scanout timestamp per pair; a
provisional NV metering experiment returned feedback 1, which NVIDIA's own
DXVK Remix integration treats as rejected and falls back to host pacing.
These experiments were not installed and are absent from the production path.
No driver settings, overlays, global timer resolution or other clients changed.

The final revision-14 runtime passes 21 x86 regression programs/scenarios and
nine isolated x64 core/synchronization validation scenarios with zero errors.
Both normal and readback overlap traces report zero later GPU submissions during
prior WSI. Injection verifies outdated-swapchain recovery, stopped rendering on
GPU submission failure, the bounded/sticky guard timeout, and owner-message
pumping. Physical validation remains limited to this RTX 2070 SUPER; actual AO
smoothness and broader GPU/driver testing remain required.

## Actual-game acceptance for the Preview 23 artwork release

The user accepted revision-14 generation behavior for Preview 23 on 7 November
2026. A stationary native-scale comparison measured 36.36 rendered FPS with
generation off, 21.87 rendered / 43.74 successful presentation returns per second
with generation on, and 36.47 rendered FPS after switching off again. Scene GPU
time stayed about 27 ms. This remains a substantial performance penalty, not a
successful performance correction. Generation is experimental and off by default;
source changes for artwork integration leave the FSR/GPU ordering code unchanged.
These short tests do not certify physical scanout cadence or cross-GPU stability.

## Earlier revision 12-13 results and limits

Revision 12 did not resolve actual-game smoothness. In a subsequent stationary
scene it produced 15.5–15.8 rendered FPS / about 31 presents per second, while
generation off produced 31–32 real FPS. The user confirmed that generation off
was smoother. The initial synthetic gain below is retained as history and must
not be presented as an actual-game speedup.

Revision 13 replaces the SDK's high-priority full-duration spin with a per-thread
high-resolution waitable timer and at most a final 0.25 ms spin. If the timer is
unavailable it yields with Sleep instead. No global timer resolution is changed.
A 416 ms controlled wait consumed 421.875 ms of quantized thread CPU time in the
baseline and 0 ms in the corrected sample. The timer-thread lifetime check left
process handle count unchanged after 16 thread creations/destructions.

The serialized path also waits for the whole prepared GPU pair once, before
setting the first presentation timestamp. Both images' submissions are already
queued, and no later game submission is permitted until both actual WSI calls
return, so this one fence proves readiness for both. It avoids submitting an
extra fence behind the first WSI call. Nonserialized paths retain per-present
readiness. A delayed-GPU mock verifies one fence, no submission between the two
WSI calls, and the requested 5 ms presentation spacing. All sticky fence-error,
bounded wait and independent queue-progress checks remain enabled.

A matched subsequent GPU fixture comparison measured 64.86 presents/sec for
revision 12 and 63.51 for revision 13, with native rendering 59.87 and 59.43 FPS.
This is similar throughput, not a further FPS gain. The separate FSR 3.1
upscaler at 67% scale, generation off, produced 120.49 real FPS in the same
synthetic fill-heavy scene. This demonstrates the distinction between reducing
rendering cost and generating additional frames; it is not a prediction for AO.

AMD recommends at least 60 pre-interpolation FPS for this FSR 3 generation
algorithm. The F: scene is well below that baseline. See [AMD's guidance](https://gpuopen.com/fidelityfx-super-resolution-3/)
and the [Windows timer API](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).
The next actual-game check must assess real FPS, physical smoothness and input
response, with upscaling independent of generation.

Revision 13 passes 17 x86 regression programs/scenarios, including the wait CPU
and timer-handle lifetime check, the extended pair-readiness/spacing test, and
zero traced later-frame GPU submissions during an earlier presentation. Six
isolated x64 core/synchronization validation scenarios pass with zero errors.
All original sticky fence failures and owner-message handoff checks pass.

## Revision 12 investigation and historical controlled results

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
fixed game-thread delay budgets (Sleep simulates production cadence, not actual
CPU computation). Both used the same single-triangle scene:

| Game-thread delay | Stock generated presents/sec | Corrected presents/sec |
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
