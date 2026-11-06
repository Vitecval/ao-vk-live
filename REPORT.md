# AO-VK Live Preview 23 — isolated candidate report

## Existing model-index correction, candidate revision 3

Revision 2 allowed the model-index filename but did not connect the existing
compatibility helper to the install transaction. A legacy index with identical
model identities and renderer categories but different descriptive names was
therefore rejected as an unknown graphics wrapper. Read-only inspection of the
reported F: index confirmed exactly this case: its raw SHA-256 is
`7f30274f61bf43d8387b4b0e296185106c93b41f2d3ff48eba5542ff0627655c`,
and its normalized identity/category SHA-256 matches the packaged index:
`315c08db99c3885481292d3bf50a752b5d38f80e6c6b4e63ca185619ff411553`.

Revision 3 validates the required packaged index, checks compatibility, and
preserves existing compatible annotations byte for byte. Preserved files are
excluded from the install receipt so uninstall cannot delete them. Missing
indexes are installed and owned normally. Different model identities/categories,
malformed indexes and damaged/missing required package data remain blocked before
installation writes. Asset conflicts now identify an asset rather than a graphics
wrapper. Renderer binaries and artwork are unchanged. Tests include complete
synthetic update/rollback/uninstall transactions and an isolated copy of the
reported index; the actual F: installation remains untouched by this work.

## Setup correction, candidate revision 2

The first private candidate incorrectly rejected its own required
`randy-vk/mesh-ids.bin` entry as an invalid optional material-pack path. This
preflight failure occurs before cache preparation or installation writes. The
installer now accepts that exact filename with the existing SHA-256 and ownership
checks; other binary paths and traversal remain rejected. Renderer binaries and
artwork are unchanged. Revision 1 should be replaced by the revision 2 setup ZIP.
The installer regression now covers model-index install/update/removal and unsafe
near-matching names. A separate package preflight test exercises the actual Lite
and Full manifests through the x86 installer in isolated fresh/update fixtures.
Preflight target lists now grow without repeatedly copying arrays, and asset
hashes use direct dictionary lookups. This preserves the same validation rules.
The initial full-manifest test exceeded its three-minute fixture timeout; that
timeout is preserved separately from assertion results. Large-manifest parsing
in Windows PowerShell can still take time; no setup speedup is claimed here.

## Status

Windows x86 renderer and private Lite/Full HD test setup candidates are prepared.
This is **not a public release or a claim that the reported full-client startup
delay/FSR crash has been resolved**. No game installation was changed, no AO
client was launched or terminated, and GitHub was not modified.

## Baseline and source isolation

- Renderer/Living baseline: `8225354f4b779cc8b07fc56a72ae3eb8b8da9670` (`codex/preview22-living-integration`).
- The Preview 22 worktree was clean. Candidate 9 SOURCE-AUDIT, FINAL-VALIDATION,
  manifest and installed-decoder receipts identify that same commit; no newer
  validated renderer snapshot was found at task start. Its status explicitly
  retains pending combined live validation.
- New worktree: `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\source` on `codex/preview23-stabilization`.
- Build: `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\x86`. Preview 22, previous releases, the Living worktree,
  texture workers and `F:\AO_SInstall\Anarchy Online` remain untouched.
- The frozen Candidate 9 packaging also carries validated Setup 4 metadata from
  `e68dcd33254a7017c072ff7f862f3c7857597e4d`. Its scripts match the renderer
  snapshot apart from the separately frozen model-index helper. That helper is
  preserved explicitly; no older renderer implementation was imported.
- Exact final source commit, changed-file inventory, build hashes and the eight
  review patches are recorded in `SOURCE-INVENTORY.json` and the package manifest.
  The report's enclosing source commit is the source snapshot for this candidate.

## Startup findings and changes

The affected player's repeated multi-minute launch was **not reproduced**.
Read-only existing local client records showed warm renderer initialization
around 384–863 ms; they do not include time before renderer entry or establish
Play-to-usable-window performance. Earlier Preview 22 fixture evidence recorded
about 74 seconds with a fresh AO-VK cache file and 1.4 seconds warm.

Fresh-cache profiling confirms expensive synchronous Vulkan pipeline creation:
several individual calls took roughly 16–27 seconds. These are driver calls,
distinguished from filename scanning, disk/cache I/O and CPU setup. This explains
the cold renderer fixture cost, **not the player's repeated-launch root cause**.
The mandatory pipeline set is still prepared before the first scene; expensive
work has not simply been moved into zoning. Optional FSR contexts remain lazy.

Concrete cache defects addressed: silent write failure; suppressing changed
content merely because byte size stayed equal; abandoning caching after a driver
rejected loaded bytes; fragile game-directory persistence and shared GPU/driver
filenames. Default persistence is now per-user/GPU/driver/UUID, with a checksum
envelope, same-size content detection, empty-cache retry, explicit diagnostics,
PID-specific temporary files and atomic replacement. Installer helper and live
runtime share the resolver. Explicit environment overrides retain priority.
Vulkan's pipeline/shader identity matching retains compatible entries rather
than clearing everything for each renderer revision. No binary GPU cache is
included in any package. See the [Vulkan cache specification](https://docs.vulkan.org/spec/latest/chapters/pipelines.html#pipelines-cache).

Instrumentation covers process creation/loader entry, addon loading/bootstrap,
renderer entry, instance/GPU/device, cache, frames/targets/swapchain, embedded
shader modules, graphics pipeline preparation, initial resources, one-time
material filename indexing and first successful presentation/submission. Slow
driver calls include thread and elapsed time. Image decode remains demand-driven
on the existing loader worker. Logs do not establish the first usable game
window. The supplied read-only `startup-report.ps1` includes a separate optional
manual Play-to-responsive measurement; it does not invent one from renderer time.

## Startup measurements

Exact measured scope: standalone x86 320×240 renderer initialization plus eight
HDR frames; complete process wall time is measured. These are **not AO launcher
benchmarks, Full HD artwork-loading benchmarks, or 1080p gameplay benchmarks**.
The host is an i7-9700K with RTX 2070 SUPER (GPU 10de:1e84); other desktop/game
activity was not controlled. No driver cache was deleted. CPU is process CPU
time; memory is peak process working set, not GPU VRAM or total virtual address use.

| Case | Process wall time | Renderer init | First submitted frame | CPU time | Peak working set |
| --- | ---: | ---: | ---: | ---: | ---: |
| cold-aovk-cache | 104.995 s | 104061 ms | 104223 ms | 88.219 s | 697.4 MiB |
| warm-1 | 3.025 s | 2348 ms | 2435 ms | 2.125 s | 381.1 MiB |
| warm-2 | 3.041 s | 2488 ms | 2570 ms | 2.031 s | 381.0 MiB |
| warm-3 | 3.434 s | 2724 ms | 2819 ms | 2.062 s | 381.0 MiB |

Earlier same-cache-code recovery tests passed corrupted-payload and synthetic
driver-metadata mismatch cases, rebuilding in about 99 and 74 seconds. These
receipts are retained separately in `benchmarks-final/RESULTS.json`; shader
calibration changed afterward, so the final-shader timing receipt above is
separate. A real driver update/cache purge was not performed. No warm renderer
fixture remained multi-minute. The reported full-client problem remains open.

## Startup progress

Native Win32/GDI window on a temporary independent thread, shown after about
2.5 seconds. It displays real stage names/completed graphics-pipeline counts
with an indeterminate indicator, no fabricated percentage or ETA. Fast starts
show no extra window. The paint loop is bounded and stop-aware; the controlled
test caught and corrected an initial shutdown/paint-loop problem.

Tests pass delayed visibility, independent message responsiveness during a
simulated blocking operation, fast-launch suppression, close and initialization
failure. It closes at the first successful presentation; actual AO responsiveness
after that point remains unverified. Errors are visible briefly (up to ten
seconds) and logged. The original launcher is preserved. No Play interception,
duplicate-launch warning, global single-instance restriction or client killing
was implemented.

## Fullscreen compatibility

The previous backend ignored cooperative-level flags and acknowledged display
mode requests without implementing exclusive ownership. This architecture has
no `VK_EXT_full_screen_exclusive` acquire/release path. **True exclusive fullscreen
remains unsupported.** A legacy exclusive request now logs the precise limitation
and applies a borderless fallback on the window's monitor, retaining desktop
resolution/refresh. Normal mode restores saved window styles/rectangle.

The production DirectDraw COM implementation passed 20 repeated exclusive-request
→ borderless → normal cycles in an isolated dependency fixture, including style/
rectangle restoration and unchanged desktop mode/refresh. The fixture loaded DLLs,
not an AO executable. Separate Vulkan/FSR tests exercised style changes and
swapchain resize. Actual AO startup fullscreen, Alt-Tab, minimize/restore,
multiple monitors and refresh switching are pending. Do not label this exclusive
support or a confirmed fix for the player's fullscreen report.

## AMD FSR stabilization

Audited FSR2.2, FSR3.1 upscaling and FSR3.1 frame generation separately. The AMD
backends can select physical-device capabilities that were not enabled on the
logical device. Supported float16/int16, extended storage image formats,
storage-buffer nonuniform indexing and subgroup controls are now enabled
consistently. Context/scratch allocation failures are handled, failed modes
latch instead of retrying every frame, and initialization falls back to TAA.
Partially failed dispatch/configuration stops further GPU submission because
resource/layout safety is unknown. This requires restart, rather than submitting
invalid work. The optional presenter and FSR contexts/views are destroyed before
their dependent inputs. Existing deferred resource handling is preserved.

Controlled GPU tests passed FSR2/FSR3 rendering, switching, render-scale changes,
resize, window style changes, frame-generation off/on and shutdown. The 72-frame
lifecycle fixture uses only its own window. Injected compute-pipeline allocation
failures produced one FSR2 and one FSR3 context failure (two injected calls total),
then rendered through TAA without repeated attempts across later frames.

Fallback reasons currently appear in the renderer log; a persistent in-game
fallback warning was not added. The original client crash was not reproduced. Radeon-specific drivers, real
device loss, real AO Alt-Tab/zoning and long multiclient sessions are unverified.
Khronos validation layers were not installed/confirmed on this host; absence of
VUID text is not presented as a validation-layer pass. Direct SDK dispatch failure
and actual hardware exhaustion were not injected.

## Exact ICC scope and original artwork

Scanner evidence identifies:

| Resource | Evidence |
| --- | --- |
| `1010004:295610` | Latest selected floor; original name `wh_basemetal_01_d.png` |
| `1010006:21` | Native cell (803,216), tile slot 13, near world (3214.1,866.8) |
| `1010006:22` | Native cell (803,217), tile slot 14, near world (3214.6,869.4) |

Quality aliases map to those same identities. Runtime guards require playfield
655; other resource IDs and other playfields are excluded. No wall additions
were approved. Shared uses elsewhere *within the larger Andromeda playfield*
have not been exhaustively surveyed; that remains a release/visual review gate.

Default AnarchyHD uses the existing files, with only a +0.035 roughness correction
on eligible target material response. No 85% default reduction, metallic edit,
albedo repaint, normal replacement, UV change or global lighting/roughness change
was made. All three frozen material archives match their complete SHA256 values
(3,108,293,439 bytes total). All nine selected
runtime source maps were also checked against Candidate 9's manifest. Original
AnarchyHD texture files and hashes are unchanged.

## Enhanced Original artwork and provenance

Three genuine separately saved built-in-imagegen source PNGs (1254×1254) follow
the recovered original references: dark continuous floor, pale framed tile and
dark framed companion. They are baked to 1024×1024 diffuse/normal/ORM maps; the
existing 512-pixel built-in runtime cap remains. Prompts, source/runtime hashes
and deterministic companion-map generation are preserved in
`material-packs/ao-icc-enhanced-original` and `tools/bake-enhanced-original.py`.
The existing AnarchyHD files are not overwritten.

Original reference pixels are only in `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\private-reference`, outside the
source/package trees, and are not distributed. Generation used those references;
that does not establish redistribution permission. The new pack and candidate
are explicitly **private-test-only; public redistribution is not approved**.
No rights clearance or live artistic acceptance is claimed. Source PNGs and
generation prompts remain available for independent review.

Runtime selection checks the complete three-map family, preserves personal
folder priority, and refreshes the three known identities on style/zone changes.
Live production AO loading, switching and zoning still require review; the
captured fixtures load the actual map files through the renderer material path.

## ICC glossiness validation

`RVK_ICCGlossiness` is 0.0–1.0 in 0.1 steps, default 1.0. Artwork selection is
`RVK_ICCArtwork` (0 AnarchyHD, 1 Enhanced Original), default 0. Both survive all
quality presets and process restart independently. No conflicting hotkey was
added: Ctrl+Shift+I picks textures and F9/Ctrl+Shift+F9 is already used for diagnostics.

At 1.0, the small baseline correction applies. Lower values broaden roughness
toward 0.90 and reduce only target environmental/SSR reflections, including the
independent wet/legacy reflection mask. Direct physical highlights remain and
metallic values/scene lighting stay intact. All eleven values passed monotonic
real-renderer roughness checks for all three IDs and both styles. Both native
ICC tile paths passed separately. Forced wet SSR mask means were 31/93/255 for
glossiness 0/0.2/1 on all six style/material combinations, confirming the wet path
does not bypass the control. This is fixture calibration, not live rain validation.

## Visual and renderer regressions

- Controlled Vulkan comparison captures are in `render-tests-delivery`,
  `native-icc-delivery` and `wet-icc-final`; all are synthetic geometry using real
  renderer output and actual material maps. They are not conceptual images or
  in-game captures.
- Non-ICC shared tile output matched frozen Preview 22 pixel-for-pixel at both
  gloss extremes. Earlier matched stable Parallax V9 home/office modes, Building
  Weathering levels and native terrain also matched exactly. The initial
  unfixed-clock native scene differed; fixed animation time restored exact parity,
  with failed/diagnostic evidence preserved.
- Live matched Preview 21/22/23 ICC scenes, alpha/UV fit on actual AO models,
  wet/dry gameplay, all cliffs/rocks/water/characters and live zoning are pending.
  These cannot be established by the fixture screenshots.
- Living core checks, twelve render-worker failure/wait checks, forty Python unit
  tests, settings/restart tests, installer process/path/ownership/rollback tests,
  4 GB option tests, downloader interruption/corruption/offline tests and actual
  synthetic Lite/Full GUI install/update/uninstall tests passed.
- The first broad unittest discovery also imported command-line-only scripts
  without their required arguments; those three invocation errors were preserved.
  The correct forty-test suite and argumented installer/download suites pass.
  Tests were not weakened to hide failed assertions.

## Living Rubi-Ka defaults

All ten cosmetic/variety settings default Off/Normal, including Ultra. The
obsolete Ultra people entry was removed; the existing preset exclusion already
preserves explicit player choices. Fresh Off values, explicit On values across
all four presets and restart persistence were exercised through the actual
addon settings API. Local assets, density/animation code, routes and the baseline
feature-completion status were retained. No unfinished baseline feature is
reclassified as live-validated by this report.

## Packages and remaining performance limits

Packages are under `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\candidate`:

- `ao-vk-live-0.6.0-preview.23-windows-x86-lite.zip`
- `ao-vk-live-0.6.0-preview.23-windows-x86-full-setup.zip`

The three large, pinned material ZIPs remain in the preserved Candidate 9 folder
and are reused; they are not copied into Preview 23. Full setup supports those
exact offline archives and their existing pinned downloads. The separate small
Enhanced Original selection is bundled with the private test packages. Payload,
addon, helper, source and manifest hashes are audited. No GPU cache, private
original reference, test-client DLL collection or active worker output is shipped.
Actual candidate installation into a real client has not been performed.

Startup CPU/working-set results are reported above. In-game FPS, GPU pass times,
1080p real-client memory, Lite/Full startup differences, preset/Parallax/Living/FSR
launch matrices, real driver-update behavior and multiclient gameplay performance
remain unmeasured. No benchmark numbers are invented for those categories.

## Integration, testing and rollback

Use the full source commit named in `SOURCE-INVENTORY.json`/the manifest on a new
integration branch. Its parent renderer baseline is `8225354f4b779cc8b07fc56a72ae3eb8b8da9670`. If Preview 22 has
advanced, cherry-pick/merge the reviewed changes and resolve overlaps; never copy
whole older renderer files over newer Living work. Main overlaps are device/cache,
startup hooks, FSR/HDR teardown, draw-transform/shader layout, native tile metadata,
material-loader selection, settings and packaging allowlists. The eight patches
are an ordered review stack, not independently buildable feature binaries.

Rebuild using the preserved local `C:\AOSharp\AO-VK\ao-vk-main\build\preview23\build.ps1` or the project's Windows
toolchain instructions. Re-run settings, startup UI, cache recovery, FSR lifecycle,
ICC wet/native/material, installer and parity checks after any merge. Keep the
legacy and candidate fixture cache files separate; Preview 22 cannot read the
new checksum envelope. Reuse the accepted material archive hashes exactly.

Before a real-client test, obtain explicit approval for its installation path.
Close its clients yourself, extract a new setup folder and use Install/Update.
Do not install to F: as part of this task. Measure Play-to-responsive time cold
and three warm starts; save loader/renderer startup records. Test each FSR path,
window mode/Alt-Tab/minimize/monitor/refresh change, zoning/shutdown and matched
ICC scenes. Confirm actual artwork loading, personal overrides and shared uses.
Use the inherited Uninstall/backup ownership receipt for rollback, or reinstall
the previous verified candidate. Preserve the INI and personal material folders.

Public release gates remain open for the reported startup/crash reproduction,
full-client compatibility matrix, same-playfield shared-use audit, live visual
acceptance and artwork redistribution rights. The isolated candidate is ready
for review; those gates are not represented as completed work.
