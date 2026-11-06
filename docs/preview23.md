# Preview 23 — private stabilization test candidate

This is an isolated Windows x86 development candidate. It is not a public release.
Actual launcher-to-usable-client timing, reported client crashes, true exclusive
fullscreen, live ICC fit and artwork redistribution review remain outstanding.

## New controls

In **Materials → Texture library (AnarchyHD)**:

- **ICC Surface Artwork** (`RVK_ICCArtwork`): AnarchyHD (0, default), Enhanced
  Original (1). A complete alternative diffuse/normal/ORM family must be present.
- **ICC Glossiness** (`RVK_ICCGlossiness`): 0.0–1.0, step 0.1, default 1.0.
  A small default roughness correction and the adjustable response apply only to
  the verified floor `1010004:295610` and tiles `1010006:21` / `1010006:22`, in
  ICC playfield 655. Direct specular highlights, metallic values, lighting, normal detail and existing images
  are unchanged. Shared uses outside ICC retain their existing response.

Artwork and glossiness are independent, saved across restarts and excluded from
quality presets. Personal texture folders retain priority. The new artwork is
separate from the existing AnarchyHD files. No wall/panel additions are approved.
Generated sources are 1254 pixels, baked to 1024-pixel runtime files; the existing built-in 512-pixel runtime limit
continues to protect the 32-bit client's address space.

**F9/Ctrl+Shift+F9 and Ctrl+Shift+I are existing diagnostics.** No new comparison
hotkey is added. Compare using the two controls in the options panel.

All ten Living Rubi-Ka population/variety controls remain Off/Normal on new
configurations. Quality presets preserve saved choices, including explicit On.

## Startup and cache

The default cache lives under `%LOCALAPPDATA%\AO-VK\pipelines-v1`, keyed by GPU,
driver version and Vulkan cache UUID. The installation helper and runtime use
the same resolver. Compiled shader/pipeline identities remain driver-managed;
compatible entries are retained across renderer updates. Cache files include a
checksum envelope, are written atomically, and are never shipped as universal
GPU data. Explicit `RANDYVK_PIPELINE_CACHE` overrides, including `off`, remain
available for diagnosis. A driver change can require local preparation again.

After about 2.5 seconds of renderer initialization, the live addon shows a small
independent progress window. Stages and completed graphics-pipeline counts are
real; the indicator is indeterminate because the driver supplies no remaining
work estimate. Quick starts show no additional window. It closes at the first
successful Vulkan presentation; this is not proof that login/world loading is
complete. Initialization errors are shown briefly and recorded in the log.

The original launcher and intentional multiclient use remain available. There
is no Play-button interception, global instance limit, duplicate-launch warning
or automatic termination of clients.

`ao-vk-loader.log` records process creation/loader-entry UTC, process age, addon loading and bootstrap duration. It is beside the loader DLL (the game directory or the shared AOPP loader directory). `randy-vk.log` records
renderer stages, slow driver operations, cache reuse/write failures and first
presentation. Normal startup adds a bounded set of lines rather than per-frame
debug output. For a slow launch, retain both logs and measure actual time from
pressing Play until the window responds. Do not infer this interval from the
renderer-init total alone. The optional `startup-report.ps1` reads both logs and accepts a separately measured `-PlayToResponsiveSeconds` value; it starts no client.

## Fullscreen and FSR

True exclusive Vulkan ownership is not implemented. A legacy exclusive request
now gets an explicit logged borderless fallback on the window's monitor. The
desktop refresh rate is retained; render resolution requests are logged. Normal
cooperative-level restoration restores the saved window styles and rectangle.
Live Alt-Tab, monitor/refresh changes and AO fullscreen behavior still need testing.

FSR2, FSR3 upscaling and FSR3 frame generation are distinct paths. Required
supported Vulkan features are enabled consistently with backend shader choices.
Context initialization and CPU-allocation failures latch to prevent per-frame
retries. Unsupported initialization falls back to TAA. A partially failed SDK
dispatch/configuration stops further GPU submission because safe continuation
cannot be established; the log identifies the failure and a restart is required.
Contexts/views and the optional presenter are destroyed before dependent inputs.
The affected player's crash and actual GPU device loss have not been reproduced.

## Later installation and rollback

No candidate has been installed into a game directory, and no AO client has been
launched or terminated for this task. In particular, `F:\AO_SInstall\Anarchy Online`
is untouched. Testing there requires the owner's separate approval.

1. Extract the selected Preview 23 test ZIP into a new empty folder.
2. For Full HD offline setup, place the three unchanged Preview 21 material ZIPs
   beside the setup folder. They are already available in the preserved Preview
   22 Candidate 9 directory. Setup can also download those exact pinned archives;
   the smaller Enhanced Original selection is bundled separately.
3. After approving a game test destination and closing its clients yourself,
   run `Install.cmd`, choose that destination and use Install/Update. Do not copy
   renderer files manually over a running client.
4. Keep the installer and its ownership/backup receipt. Use `Uninstall.cmd` for
   transactional restoration, or restore the prior verified candidate through
   its installer. Preserve `randy-vk.ini` and personal material folders.
5. Follow `REPORT.md` for the missing live startup, FSR, mode-switch and ICC
   capture matrix. Do not publish this candidate or the new artwork while its
   rights/live-validation gates remain open.

The inherited 4 GB option, local asset preparer, model-index safeguards,
transactional update and modified-file refusal are retained. No unrelated
material downloads or active texture-worker outputs are incorporated.

Setup revision 3 of the Preview 23 candidate preserves a pre-existing compatible
`randy-vk/mesh-ids.bin`, including older descriptive names. It verifies model
identities and the renderer's scenery categories, leaves the preserved bytes
unchanged and excludes that file from uninstall ownership. A missing index is
installed normally. Do not delete an existing index to work around the errors
from candidate revisions 1 or 2; extract the revision 3 ZIP into a fresh folder
and retry Update. A damaged index or incompatible identities still produce a
specific refusal before installation writes.
