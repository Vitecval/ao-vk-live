# AO-VK Live Preview 23 — candidate revision 13

## Revision 13: efficient CPU pacing and one readiness fence per serialized pair

Revision 12 improved a controlled benchmark but the actual F: game remained stuttery: about 15.5 rendered / 31 presented FPS, versus 31-32 real FPS and smoother motion with generation off. Revision 13 removes the pacer's long high-priority CPU spin and redundant post-WSI fence submission. All paired GPU work is still completed before WSI and later game submissions still wait for actual pair return. The matched synthetic GPU test shows similar throughput rather than another speedup; CPU wait cost and delayed-GPU presentation spacing improve. FSR upscaling at 67% scale is tested separately from generation. Actual-game validation remains required. See [the audit](docs/fsr-performance-audit.md).

## Revision 12: frame-generation pacing

The serialized NVIDIA pacer now excludes the previous presentation wait from its production estimate while retaining independent game CPU work. The GPU-ready, owner-thread, whole-pair and future-submission guards remain enabled. A controlled GPU-heavy test improved successful presents from 39.68 to 53.76 per second; actual-game performance and smoothness still need verification. See [the performance audit](docs/fsr-performance-audit.md). Setup 6 and material pins are retained.

## Revision 11 / Setup 6: installer fault and recovery audit

The installer now handles shallow/root extraction folders, empty Browse, Unicode paths, malformed/oversized metadata, path collisions and concurrent setup. File moves have an interruption recovery record and snapshot checks; failed cleanup is reported without undoing a successful install. Unresponsive GPU preparation is bounded, and cached HD files can be repaired without repeating valid work.

The working revision-10 renderer is unchanged. The user reports that FSR now works with fewer frames; no performance change or broad GPU certification is claimed. See [the installer audit](docs/preview23-installer-audit.md) for corrected cases, regression evidence and the final-texture release handoff.

Earlier revision details are retained below as history.

## Revision 10: FSR failure handling and hardware requirements

A deeper audit reproduced ignored pipeline and descriptor-allocation errors, partial initialization leaks in both FSR versions, and incomplete hardware checks. These paths now unwind or fall back safely. The baseline real Vulkan pipeline-allocation failure left 419 objects allocated; the corrected case reports no leaks. The expanded CPU audit passes 289 failure points and 31 capability profiles.

See [the full FSR stability audit](docs/fsr-stability-audit.md) for evidence and limits. The previous full-game kernel presentation hang is **not proven fixed**. Revision 9 was not live-tested before this work. Frame generation remains experimental, default off, and pending actual-game and multi-GPU validation.

The sections below preserve the earlier revision history.

## Revision 9: keep later game GPU work behind actual presentation completion

Revision 8 failed in the real client. PID 57900, launch file time
134384696396085918, loaded SHA-256 82dac3d5... and wrote randy-vk-5.log.
The capture confirms WSI ran on the window owner as intended. That thread
blocked in NtGdiDdDDIPresent, the SDK presenter waited for its synchronous
owner-thread callback, interpolation waited on the GPU, and the rendering worker
was idle after the five-second handoff failure. Moving WSI to the owner did
not resolve the underlying driver stall.

A controlled comparison launched the unchanged revision-8 renderer with only
Twitch and RTSS Vulkan layers disabled in the F: launcher's child environment.
No global/user environment, registry or driver profile was changed. The new
client, PID 18348, launch file time 134384701457875573, loaded neither overlay
DLL and still froze after enabling frame generation. Its log is randy-vk-6.log.
Both are therefore unnecessary for reproducing this failure; neither is blamed.
The normal isolated GPU fixture also loads both overlays and passes. Both F:
launches were captured and closed, with FrameGen restored to 0 and all other
settings bytes preserved. The temporary diagnostic launch settings expire with
those processes. Evidence and receipts: checks/framegen-freeze-r8.

The earlier ready-fence/pair changes only cover the pair being presented. The
engine could still queue a later game frame before that pair entered WSI. The
later work can depend on future composition/replacement signals that the CPU
presenter has not submitted. A driver presentation path that drains other queues
can then wait for work whose progress depends on that same blocked presenter.
This is a dependency class supported by the new trace, not a claim to have proven
NVIDIA's private internal implementation or the real-game root cause.

For NVIDIA devices (vendor 0x10de), revision 9 waits for a CPU completion marker
published only after the entire generated/real pair's actual presentation calls
return. Both SDK composition modes publish it. The rendering worker owns no
SDK critical section or queue mutex while waiting. Completion is monotonic and
the wait is limited to five seconds, with failure returned through the existing
stop/log path. The next game frame's GPU work cannot be submitted past an
unfinished pair on a successful path. Passthrough presents already return
synchronously. Other GPU vendors retain the existing asynchronous scheduling.
The first eight driver-present entries/returns are now logged for a future
capture. A driver call itself remains uncancellable by this mechanism.

The regression propagates game-frame origins through binary/timeline semaphore
submissions. Revision 8 records 104 WSI calls with later game work already queued.
Revision 9 records zero. A simpler active-call-only probe recorded zero in both
versions and was insufficient: the problematic later work was queued before the
earlier WSI call started. The traced before/after fixture is the relevant control.

All twelve GPU scenarios pass on the local RTX 2070 SUPER: frame-origin overlap,
window replacement, direct/native threaded rendering, paired submission, window
callbacks, resize/scale/toggles, image-acquisition timeouts, limited surfaces,
slow presentation, initialization failure and delayed interpolation handoff.
Slow presentation still produces 348 driver presents for 180 rendered frames.
The delayed-handoff scenario exercises the new completion timeout and exits.
Four sent-message wait cases, twelve worker faults and ready-guard failure cases
also pass. Actual game confirmation of revision 9 is still pending. This
compatibility scheduling can reduce render/presentation overlap and throughput;
no FPS improvement or general driver-level fix is claimed.

## Revision 8: present on the game window's owning thread

The actual revision-7 client froze after enabling frame generation. PID 72860,
launch file time 134384679739441033, loaded renderer SHA-256 7e7554a9...
and wrote randy-vk-4.log. The five-second handoff guard fired and logged
presentation stopped, but did not recover the game. Its window thread was then
inside NVIDIA's window handling and a driver lock wait, the rendering worker
was inside VMA image allocation/binding and the driver, interpolation waited on
GPU synchronization, and the presenter remained in NtGdiDdDDIPresent.
This implicates a driver/window-thread interaction; exact private NVIDIA lock
ownership and the internal cause are not proven by these snapshots.

Only that captured F: launch was terminated, with path and creation-time guards.
Windows reported HasExited=true while retaining a teardown thread and an
unsignaled process handle; recovery accepts the exit status rather than retrying
the same wait forever. Only FrameGen=1 was changed to 0 in the settings.
Evidence and recovery receipt: checks/framegen-freeze-r7. The old wait-chain
utility had hardcoded revision-6 thread IDs, so its output is not evidence for
this freeze. The new independent stacks and exact revision-7 map are retained.

Revision 8 routes actual FFX WSI presents to a private message-only window on
the game HWND's owning thread. It does not subclass the game's window, dispatch
posted input, or change driver/overlay settings. The SDK callback releases the
sender-side queue mutex before the synchronous handoff; the receiving thread
acquires that mutex around the real vkQueuePresentKHR call. The request and
VkPresentInfo remain alive until the call returns. Dispatcher creation failure
uses the existing normal-presentation fallback with a logged reason. Dispatcher
windows/classes are released after SDK presenter teardown, including window
replacement and initialization failure.

The normal threaded renderer continues recording on its existing worker. For
the direct Device path, potentially blocking preparation/configuration/present
calls use short-lived helpers while the owner pumps sent messages; owner-side
frame-fence waits poll in one-millisecond intervals and pump sent messages.
This avoids introducing a new deadlock in the single-thread mode. That mode
can pay extra thread-creation overhead; no performance improvement is claimed.
The existing GPU-ready fence, complete-pair scheduling and bounded SDK handoff
remain. A Vulkan driver call itself still has no cancellable timeout guarantee.

The regression models a recursive driver lock held during a synchronous window
callback. Revision 7 presents all 108 frames on a different thread and times
out all 108 callbacks; revision 8 presents them on the owner, with zero wrong-
thread calls and zero callback timeouts. The model demonstrates this corrected
interaction, not a reproduction of NVIDIA's private implementation. Additional
GPU scenarios cover native threaded and direct rendering, FSR switches,
resize/scale/toggles, acquisition failures, limited surfaces, paired ordering,
slow presentation, initialization failure and delayed interpolation handoff.
All ten GPU scenarios pass, plus a window-replacement scenario that tears down
and recreates the dispatcher with frame generation active. Slow presentation
still produces 338 driver presents for 180 rendered frames. Four sent-message
wait cases, twelve worker-fault cases and readiness-guard failure cases pass.
Actual game confirmation is still required after installation. Earlier reports
of fixture success must not be treated as a successful real-game retest.

Relevant contracts: [Vulkan Win32 WSI](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateWin32SurfaceKHR.html)
and [Microsoft SendMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagew).

## Revision 7: paired GPU work before a blocking WSI call

The owner enabled frame generation in revision 6 and the actual client stalled
again. PID 52308 loaded the expected DLL (SHA-256 beginning 31c8eb23). The new
randy-vk-3.log recorded initialization and a 20-second hang. External thread
captures found the presenter in NtGdiDdDDIPresent, interpolation waiting on GPU
synchronization, and the renderer waiting on the SDK interpolation event. Windows
wait-chain inspection reported no CPU ownership cycle; the game thread remained
running in its message-aware producer wait. The owner authorized closing F: at
any time and went AFK. Only F: was stopped, and FrameGen=0 restored without other
settings changes. See checks/framegen-freeze-r6/RECOVERY.json and captured evidence.

The SDK previously prepared/presented the generated image before preparing the
real image. Completion/replacement signals for that real image could therefore
remain unsubmitted behind a blocking first present while another queue already
waited for them. Revision 7 prepares the entire pair first, in both SDK presenter
modes, then presents generated/real images in the original order and pacing.
The existing ready-fence guard now covers all queued pair work before WSI.
This is a scheduling correction/workaround for the observed class of stall; the
driver's exact internal blocking reason is not claimed proven.

The swapchain requests at least surface.minImageCount+1 images for two-image
forward progress, within the SDK/surface limits; otherwise normal presentation
is retained with a logged reason. Image acquisition is bounded to 250 ms with
the existing skipped-frame semaphore completion path. The renderer's SDK handoff
wait is bounded to five seconds; failure stops submission and logs a restart
instead of stranding the worker indefinitely. Window-message safeguards remain.

The control build using the original SDK loop withheld real-frame GPU submission
on 54 of 108 presenter calls. The changed loop records zero such calls in repeated
tests. Nine GPU scenarios pass: paired submission twice, synchronous window
callbacks/resize/scale/toggles, twelve injected real-image acquisition timeouts,
a surface that cannot support paired images (normal fallback), native threaded
rendering, FSR mode switching, slow presentation and FSR initialization failure.
Slow presentation still produced 344 presentations for 180 rendered frames.
An additional 6.1-second interpolation-thread delay exercises the five-second
handoff failure, which stops/logs and exits the controlled fixture. Four sent-
message wait cases, twelve worker-fault cases and readiness failure tests pass.

No global driver profile, overlay setting, texture or stock game file changes
are part of this correction. The owner is away, so the real-game retest is pending.
Do not treat fixture success as confirmation that the actual freeze is resolved.
Batching may add latency, and the extra swapchain image uses more GPU memory;
frame pacing/performance/visual quality need live validation. Earlier sections
below describe historical revisions and do not override this current limitation.

## Revision 6: Windows message-aware renderer waits

The owner retested revision 5 and it froze again. PID 63696 loaded the expected
revision-5 DLL. Its log was randy-vk-2.log (the previous terminated process still
retained the first log), and it confirmed the GPU-ready guard was active. The
new captured presenter was still in NtGdiDdDDIPresent; the renderer waited for
the SDK interpolation event and the game waited in ThreadedDevice::EndFrame.
The owner authorized closing F: as needed; only this frozen F: process was closed
and FrameGen=0 restored, with all other settings bytes preserved.

Revision 6 addresses a concrete Win32 WSI contract violation: the caller could
wait indefinitely for rendering while never servicing synchronous window
messages required by a presenter on another thread. Caller-side futex waits now
service only sent messages and use one-millisecond sleep slices; worker idle
waits are unchanged. Shutdown handle waits use MsgWaitForMultipleObjectsEx.
Idle and SDK teardown operations invoked on the HWND owner run on a helper while
that owner services sent messages. Posted input, timers and WM_QUIT are neither
removed nor dispatched by these waits. The previous GPU-ready guard remains,
but it did not solve the owner's freeze alone.

Evidence: all four original CPU waits (sync/frame/queue/shutdown) failed to
acknowledge a synchronous cross-thread message, taking 359-406 ms to hit the
test sender's timeout. Corrected waits pass, including posted-input and quit
preservation. The GPU regression invokes synchronous HWND callbacks from the
actual FSR presenter: original waits repeatedly time out and hit a 20-second
watchdog; corrected waits complete all callbacks, including final teardown.
See checks/framegen-freeze-r5 for captured logs, baseline objects and receipts.
Twelve worker-failure cases and the GPU-ready boundary error tests still pass.
Native UI, FSR/scale/resize/toggle and slow-presenter checks are recorded there.
The owner's actual game still requires live confirmation; isolated regression
coverage is not presented as a successful game retest.

Win32 requirements: [Vulkan Win32 WSI](https://docs.vulkan.org/refpages/latest/refpages/source/vkCreateWin32SurfaceKHR.html)
and [Microsoft PeekMessage](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew).
Twitch's implicit Vulkan layer was found, but it has not been proven responsible.
Driver profiles were read only; no system, NVIDIA or overlay settings changed.

## Revision 5: frame-generation correction and limits

Revision 4 was installed and its startup fix allowed the owner to play. Enabling
frame generation then produced a real hang: the game thread waited for the
render worker, which was blocked in the NVIDIA driver while the SDK presenter
was in NtGdiDdDDIPresent and its interpolation thread waited on GPU synchronization.
The full captured evidence is in build/preview23/checks/framegen-freeze-r4.
The owner approved closing that single frozen process and saving FrameGen=0;
all other settings bytes were preserved. The exact first bad runtime dependency
and possible Twitch Studio capture interaction remain unproven.

The corrected SDK present boundary submits a fence after existing work on the
present queue and waits for its completion before entering vkQueuePresentKHR.
The CPU wait holds no queue mutex, allowing other queues to progress and avoiding
an unready image being waited on inside a driver presentation lock. The acquired
image's binary semaphore remains attached to the real present. Readiness waits
are limited to five seconds; fence allocation/reset/submission/wait failures are
latched, preventing reuse of a potentially pending fence. Frame-generation
presentation errors stop submission and log the need for a restart. The fence
is destroyed after SDK presenter teardown and queue-idle checks.

The production boundary fails its new test before this guard and passes after it.
Tests verify progress on another queue while waiting and timeout/allocation/
submission/reset error handling. Native 1914x1051 direct and threaded UI, slow
presentation, FSR mode/scale/size switching and injected FSR initialization failure
all pass on the local RTX 2070 SUPER. Source-built real GPU fixtures continue to
request and present generated frames; frame generation was not replaced by an
always-off fallback. The extra present-queue completion point may reduce overlap
or add latency. Actual game stability, frame pacing and performance require the
owner's live retest. Do not claim the captured game freeze conclusively resolved
from isolated tests alone.

## Current result

The fresh Windows x86 candidate contains the renderer and HD material update.
**Living is not included:** no actor implementation or cache loader is linked,
no Living controls are exposed, no helper/recipes/commands are shipped, and setup
never prepares or rebuilds Living. This supersedes the original request to bundle
Living; the owner explicitly separated it during this session.

The three license lines were removed from the installer window and console.
Setup runs work outside its UI thread, shows the current task, elapsed time and
recent task history, and reports native helper activity during GPU preparation.
Existing correct HD textures are checked once and reused; their archives are not
downloaded or unpacked merely to update the renderer. The source commit and ZIP
hashes are recorded in SOURCE-INVENTORY.json and PACKAGE-AUDIT.json.

## Startup failure reproduced and corrected

The owner manually launched `F:\AO_SInstall\Anarchy Online\Anarchy.exe`.
A read-only process monitor captured the game process exiting with status 0 in
less than one second, before a window or Vulkan initialization. The loader itself
loaded the addon successfully; its bootstrap was about 500 ms in that attempt.
No current shader-compilation stall was observed in these attempts.

A regression introduced by the Preview 23 fullscreen change rejected a NULL
window handle unconditionally. AO's own native windowed Initialize calls
`SetCooperativeLevel(NULL, DDSCL_NORMAL | DDSCL_FPUPRESERVE)` (flags 0x1008).
This is permitted by the [Microsoft DirectDraw contract](https://learn.microsoft.com/sk-sk/windows/win32/api/ddraw/nf-ddraw-idirectdraw7-setcooperativelevel).
The rejected call prevented initialization from reaching Vulkan.

- Revision 3 production DLL: this exact call returned `0x80070057`; the new
  regression fixture exited at the expected failure check.
- Corrected DLL: the call returns `0x00000000`. NULL with exclusive/fullscreen,
  or conflicting normal/fullscreen flags, remains rejected.
- Twenty production COM exclusive-request/borderless/normal cycles passed,
  including returning to normal with NULL. Window style/rectangle restoration
  and unchanged desktop resolution/refresh were verified.
- Native Randy windowed Initialize completed through the production Vulkan
  renderer in the isolated dependency fixture, without an AO executable.
- The final no-Living fixture's warm Vulkan initialization was about 2.9 seconds.
  This is not Play-to-responsive game timing.

The owner must still test the revised build in the actual client. This report
**does not claim a completed live launch or resolution of every earlier reported
multi-minute startup delay**. No AO client was launched or stopped by the agent.

## Baseline, source and integration

The renderer baseline is the newest validated Preview 22 snapshot found at task
start: `8225354f4b779cc8b07fc56a72ae3eb8b8da9670`, verified against Candidate 9
receipts/manifests; no newer validated renderer snapshot was found. Frozen Setup
4 helper metadata originated from `e68dcd33254a7017c072ff7f862f3c7857597e4d`.
The isolated branch is `codex/preview23-stabilization`, worktree
`C:\AOSharp\AO-VK\ao-vk-main\build\preview23\source`.
Preview 22, existing public releases and texture workers were not edited.

The integrated pre-split source is archived at commit `fb29cc1`, including its
startup fix and Living preparation work. The current branch removes the four
Living implementation units, data types, tools/tests and settings from the main
build. A minimal inert compatibility bridge remains for existing frontend calls;
it performs no population loading, allocation or geometry suppression. The
library object inventory contains the compatibility bridge, not the old Living
implementation. Final DLL scans contain neither LRLOCAL1 nor LRSCENE9 readers.

Use the final source commit/archive in SOURCE-INVENTORY.json as a complete
snapshot. When integrating into a newer branch, review the device/cache and
startup hooks, DirectDraw cooperative-level handling, FSR teardown, native tile
metadata/draw-transform layout, material selection, settings and installer files.
Apply the complete stack; old review patches from revision 1 do not represent
this final no-Living build. Do not overwrite a newer renderer with older whole
files. The separate add-on archive preserves the previous Living implementation.

## Earlier cache measurements and diagnostics

Earlier Preview 23 offline x86 320x240 initialization plus eight HDR frames:

| Run | Process seconds | Renderer initialization |
| --- | ---: | ---: |
| Cold application cache | 104.995 | 104.061 s |
| Warm 1 | 3.025 | 2.348 s |
| Warm 2 | 3.041 | 2.488 s |
| Warm 3 | 3.434 | 2.724 s |

These are earlier fixture measurements, not new full-client revision 4 results.
Cold peak process memory was about 697 MiB, warm about 381 MiB. Individual cold
Vulkan driver calls took 16–27 seconds. CPU was i7-9700K, GPU RTX 2070 SUPER;
other desktop/game activity was uncontrolled. No actual driver cache purge or
1080p/multiclient FPS/GPU-pass benchmark was performed. The player's original
multi-minute launch was not measured as a successful Play-to-responsive interval.

The cache defaults to `%LOCALAPPDATA%\AO-VK\pipelines-v1` with GPU/driver/UUID
identity. An explicit RANDYVK_PIPELINE_CACHE override remains supported. Header,
size, driver and checksum validation, recovery from driver rejection, content
change detection, atomic saves and write-error logging are retained. Corrupt and
synthetic driver-metadata mismatch recovery passed in earlier fixtures. Caches
are generated locally; no universal GPU cache is distributed.

Loader logs identify process start/entry time, process age and bootstrap timing.
Renderer logs identify preparation stages, slow driver operations and first
presentation. First presentation is not proof of responsiveness. The delayed
native startup window remains at 2.5 seconds for expensive renderer preparation;
quick preparation suppresses it. No percentages or completion times are invented.
`startup-report.ps1` only reads logs and accepts separately measured launch time.

## Living separation and F: cleanup

The previous setup always rebuilt local assets, wardrobes and per-city terrain,
geometry and flight-route validation. The observed run wrote about 1.7 GB before
the installer killed its still-active helper at the fixed ten-minute limit.
That is why it failed. The preparation work and its progress/timeout correction
are preserved in the pre-split archive for the future add-on; they are absent
from this renderer installer.

At the owner's explicit request, about 6.8 GiB of Living source/tools, client
caches, legacy Living versions, interrupted preparation and selected old backups
were moved and verified under `F:\AO-VK-D2\LivingAddOn`. README and move receipts
record the contents and original locations. The original full INI was saved;
only the ten population settings were removed from the active INI. Original game
executables/DLLs, the renderer install receipt, current renderer, HD textures,
personal packs and model index were retained. Old backup content remains in the
archive. No permanent cleanup deletion or fresh update installation was performed.
The current renderer does not read the archived data. The archive is development
material for a future add-on, not a completed standalone add-on installer.

The older Preview 23 candidate folders and duplicate source ZIPs await explicit
cleanup approval. The exact paths and byte counts are listed in CLEANUP-PROPOSAL.json.

## Fullscreen and FSR

True Vulkan exclusive ownership is not implemented. Exclusive DirectDraw requests
use a logged borderless fallback on the selected monitor, retain desktop refresh
and restore prior window state on normal/restore requests. The pre-window normal
startup regression is corrected. Actual AO Alt-Tab/minimize/monitor/refresh tests
remain pending.

FSR 2/3 supported feature negotiation, allocation guards, per-device initialization
failure latches, TAA fallback and context/presenter teardown ordering remain.
Partial failed dispatch stops unsafe GPU submission and requires restart; reasons
are logged. Earlier lifecycle fixtures covered mode/scale/size changes, frame
generation toggles and injected initialization failure with no per-frame retries.
The user's actual FSR crash, AMD-specific drivers, real GPU memory exhaustion,
device loss and validation-layer behavior remain unverified. No universal crash
fix or leak-free allocation-failure claim is made.

## ICC material/artwork results

Exact selected targets are floor `1010004:295610` and native tiles
`1010006:21` / `1010006:22`, guarded by playfield 655. The default response adds
only a small roughness correction on eligible authored surfaces. Original
AnarchyHD files were not overwritten; original library archive/map hashes match
the frozen baseline. V9, terrain and ordinary character/scene rendering remain.

Enhanced Original is a separate generated, reference-derived three-family pack,
with saved generation prompts, original generated source art and nine diffuse,
normal/ORM maps. Original game reference images are not in the player ZIPs.
The pack remains private pending live UV/appearance and redistribution review;
no public rights claim is made. No wall target was approved.

`RVK_ICCArtwork` defaults to AnarchyHD. `RVK_ICCGlossiness` is 0.0–1.0 in 0.1
steps, default 1.0, independent of presets. All eleven values, four presets and
restart persistence passed again with Living controls absent (359 settings,
17 sections). Earlier real Vulkan fixtures covered the three IDs, two styles,
all gloss values, wet reflection masks, native tiles and non-ICC comparison.
Same-playfield shared uses outside ICC in Andromeda have not been fully surveyed;
that remains an explicit acceptance gate. Historical matched V9/weathering/native
parity checks passed; live scene captures and 1080p game performance are pending.

## Validation, packages and next test

The final check receipts cover: pre-window regression before/after, native windowed
initialization, twenty fullscreen fallback cycles, no-Living settings/persistence,
responsive WinForms install/update/uninstall with actual worker queues and UI
heartbeat, installer ownership/process/rollback/path guards, archive-free updates
of verified installed HD assets, and material download/pin/reuse/corruption tests.
No player package includes the Living helper, metadata, commands or generated
client assets. Package payload and available asset hashes are audited; deferred
Full HD assets retain the exact pinned Preview 21 archive descriptors.

Packages are in `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\candidate-r9`:

- `ao-vk-live-0.6.0-preview.23-windows-x86-lite.zip`
- `ao-vk-live-0.6.0-preview.23-windows-x86-full-setup.zip`

Extract the latest package into a new folder, close F: clients/launcher yourself,
run Update.cmd and select `F:\AO_SInstall\Anarchy Online`. Keep the install
receipt so the existing renderer is recognized. Then launch Anarchy.exe manually
and record Play-to-window and Play-to-responsive timing plus loader/renderer logs.
Test ICC selection and gloss, window changes and each optional FSR path. Roll
back through the inherited ownership receipt/uninstaller or the prior verified
renderer package; preserved backups are in the archive. Do not copy a prior
integrated Living DLL over the new renderer as an add-on installation method.
No GitHub publication or automatic game launch/installation was performed.
