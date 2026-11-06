# Preview 23 revision 6 — Windows presentation message deadlock correction

This candidate contains the Preview 23 renderer, existing HD library, ICC artwork
selector and gloss slider. Living is a separate add-on and is not included.
No population, wardrobe, route, terrain-support or airspace preparation runs during
installation. There are no Living controls in this build.

## Install or update

1. Close the clients and launcher belonging to the selected AO installation.
2. Extract the latest test ZIP into a new folder. For the existing F: installation,
   run **Update.cmd** and select `F:\AO_SInstall\Anarchy Online`.
3. Setup verifies existing HD files once. Correct installed textures do not require
   a download, unpacking or replacement. Missing or changed artwork still uses the
   same pinned material archives and verified hashes.
4. The window shows the current operation, elapsed time and recent tasks. Work
   runs outside the UI thread, including file checks and GPU preparation.
5. Start AO normally through `Anarchy.exe`, select Primary Display Driver, then
   use Ctrl+Shift+F12 for renderer settings. No game is started by setup.

The three bulky license lines were removed from the installer window.
Settings, personal packs, the original game DLLs and existing executable patches
are preserved by the normal ownership checks. Keep `ao-vk-live.install.json`;
Update uses it to recognize the installed renderer. Use Uninstall.cmd for
installer-owned files and backups. No update was installed automatically.

## Fixed startup path

The previous fullscreen change rejected AO's valid pre-window call
`SetCooperativeLevel(NULL, DDSCL_NORMAL | DDSCL_FPUPRESERVE)`. It now succeeds.
The old binary failed the regression fixture; this binary also completes native
Randy windowed initialization through Vulkan. These are isolated fixture checks;
the owner's next full-client launch is the live verification step.

True exclusive Vulkan ownership remains unavailable. Exclusive requests use the
existing borderless fallback; desktop resolution and refresh are retained.
FSR safeguards remain included, with driver-specific and actual crash validation
still pending as described in REPORT.md.

## Frame generation

Revision 5 still froze in the owner's game. Revision 6 corrects a Windows
threading requirement: Vulkan presentation may synchronously message the window
owner, so that thread must keep processing those messages while waiting for
rendering. The previous futex-only caller waits could deadlock the game, render
worker and SDK presenter in a cycle.

Caller waits now service only synchronous sent messages. Posted keyboard/mouse
input, timers and quit requests remain with the normal game loop. Renderer idle
and SDK presenter teardown waits on the window-owning thread use a helper while
that thread services sent messages. The render worker keeps its efficient
indefinite idle wait. No global NVIDIA profile or capture-layer settings change.

The original waits fail four CPU regressions and the GPU presenter test stalls
with callback timeouts; the corrected tests complete all callbacks. These checks
also cover resize, scale changes, frame-generation toggles, posted-input/quit
preservation and shutdown. The actual game's retest remains required.

Revision 4 hung after frame generation was enabled in the owner's game. The
captured renderer, interpolation and presenter threads were blocked in graphics
driver synchronization/presentation. Revision 5 waits for queued presentation
work to finish before entering the driver's present call, with other queues
unlocked during that wait. A five-second timeout or graphics failure stops
presentation without reusing an unfinished fence. This reduces asynchronous
overlap and may affect performance; a real-game comparison is still required.

The old present-boundary regression fails and the corrected boundary passes,
including independent queue progress and injected timeout/allocation/submission
failures. Real GPU native-resolution, threaded UI, slow-presenter and FSR
switching fixtures pass. These fixtures do not establish that the owner's exact
freeze is resolved. Frame generation stays off in the F: preferences until the
owner deliberately tests it again. Other preferences and HD materials remain.

## ICC controls

Only `1010004:295610`, `1010006:21` and `1010006:22` are eligible, guarded by
playfield 655. `RVK_ICCArtwork`: AnarchyHD (default) / Enhanced Original.
`RVK_ICCGlossiness`: 0.0–1.0, step 0.1, default 1.0; separate from presets.
The original AnarchyHD files are unchanged. Enhanced Original is a private test
selection pending live visual and redistribution review. Same-playfield shared
uses elsewhere in Andromeda still need the recorded audit.

## Living archive

Living source, frozen tools, previous integrated renderer, client caches and
related backups are preserved under `F:\AO-VK-D2\LivingAddOn` with move receipts
and a pre-split source snapshot. This is development work for a future separate
add-on. The current renderer does not read or generate those caches.

Keep startup logs if the full-client test still fails: ao-vk-loader.log and
randy-vk.log in the client folder. The optional startup-report.ps1 reads existing
logs and accepts a separately measured Play-to-responsive time; it launches no
client. No cleanup deletion should proceed until the owner's explicit approval.
