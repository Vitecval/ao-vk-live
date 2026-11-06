# AO-VK Live Preview 23 — candidate revision 4

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

Packages are in `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\candidate-r4`:

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
