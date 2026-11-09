# Preview 23 - expanded Full HD artwork / Setup 7

Setup 7 retains the Setup 6 fixes for the empty-path archive search, supports Unicode installation records, and recovers interrupted file changes. Failed preparation or file checks preserve the previous installation. Setup shows current activity and saves detailed error logs. See [the installer audit](preview23-installer-audit.md).

The Preview 23 Full HD edition integrates 13,393 unique diffuse replacement IDs and 41,267 runtime maps, including 4,766 IDs beyond the Preview 21 library. The library combines HD artwork, recovered artwork and pattern-preserving technical restorations; these counts do not mean every original received an independent repaint. This includes 310 newly completed originals and 50 reviewed older artwork IDs recovered from broad retention holds. The remaining 23 image-service-rejected requests keep their originals. See [artwork coverage and retained originals](preview23-artwork.md).

The revision-28 local test adds a global colour-detail pass, Instant Day/Night,
optional light-source glow and optional ambient life. Ultra selects SharedGround+
surface roughness 0.6 and Far Scenery at 3×. New atmosphere effects remain off by
default. See [detail and atmosphere testing](preview23-detail-and-atmosphere.md).
Packaging and publication are on hold for the owner's retail visual test.

Frame generation retains the revision-14 freeze safeguards and remains experimental and off by default. Its NVIDIA compatibility path can reduce real rendering FPS substantially; the generated-frame counter is not the same as real rendering speed. FSR upscaling and render scale remain independent. See [the performance audit](fsr-performance-audit.md) for measured limits.

Revision 10 fixed FSR initialization leaks, ignored pipeline/descriptor errors and missing hardware checks. Unsupported features fall back with a logged reason. Frame generation remains experimental and off by default. The earlier [FSR audit](fsr-stability-audit.md) records the investigation; the latest user report above updates its then-pending local test status. Additional physical-GPU validation remains pending.

The current visual correction adds **Day/Night Cycle** to the graphics menu and
its own settings section. **Rely on Server Sided day/night** retains the normal
clock. **Longer Day / Shorter Night** remaps the local Rubi-Ka sky clock smoothly
without changing the length of its 27-hour cycle; other worlds keep their own
clock. **Enhanced Daylights (happier)** keeps the server cycle and adds restrained
cool sky bounce to dark outdoor surfaces, with slightly gentler directional
shadows and a clearer daylight sky. It preserves sunlit exposure, material
contrast, black levels, night and weather. It does not drive a whole-image
brightness or gamma lift, and it is excluded from intro and indoor scenes.

**Enhanced Original** now preserves original AO world patterns and palette,
using bounded fine HD detail and softer, less metallic highlights. It is the
default for new preferences; existing artwork choices remain selectable.
Character artwork keeps its separate selection. **Improved Tiles**, beside the
enhanced ground renderer under Terrain mode, controls the custom ICC tiles in
Shared Ground+. Off feeds the original tiles into the continuous shared terrain
renderer. Shared Ground+ retains AO's triangle diagonals and disables geometric
rounding, displacement and tile parallax so terrain overlays remain aligned.
Source colour is retained instead of replacing it with a blurred classification
sample. Large world colour maps may retain 1024 pixels when address space permits;
normal/material maps and the decoded cache keep their existing memory limits.

Creation characters without a world playfield retain their current draw's local
studio lights at the selected light intensity. The room keeps the normal world
light path; there is no blanket full-strength light override. Separate Vulkan fixtures cover that light path,
pattern preservation, readable daylight, ICC tile fall-through and cliff coverage.
Retail creation-screen, city/terrain visual approval and other-GPU tests remain
separate checks; these fixtures do not certify every zone or graphics card.

The following release notes retain the prior rendering changes.

This candidate contains the Preview 23 renderer, expanded HD library, ICC artwork
selector and gloss slider. Living is a separate add-on and is not included.
No population, wardrobe, route, terrain-support or airspace preparation runs during
installation. There are no Living controls in this build.

## Install or update

Full HD Setup downloads the matching verified Preview 23 material companions,
or reuses those ZIPs when placed beside Setup. Lite includes the renderer and
essential resources; choose Full HD for the complete replacement library.

1. Close the clients and launcher belonging to the selected AO installation.
2. Extract the latest ZIP into a new folder. Open **Install.cmd** for a fresh
   installation or **Update.cmd** for an existing AO-VK installation, then select
   your classic AO installation folder.
3. Setup verifies existing HD files once. Correct installed textures do not require
   a download, unpacking or replacement. Missing or changed artwork still uses the
   matching pinned Preview 23 material archives and verified hashes.
4. The window shows the current operation, elapsed time and recent tasks. Work
   runs outside the UI thread, including file checks and GPU preparation.
5. Start AO normally through `Anarchy.exe`, select Primary Display Driver, then
   use Ctrl+Shift+F12 for renderer settings. No game is started by setup.

Settings, personal packs, the original game DLLs and existing executable patches
are preserved by the normal ownership checks. Keep `ao-vk-live.install.json`;
Update uses it to recognize the installed renderer. Use Uninstall.cmd for
installer-owned files and backups. The owner-authorized F: renderer update is
recorded separately in the delivery verification receipt.

## Fixed startup path

The previous fullscreen change rejected AO's valid pre-window call
`SetCooperativeLevel(NULL, DDSCL_NORMAL | DDSCL_FPUPRESERVE)`. It now succeeds.
The old binary failed the regression fixture; this binary also completes native
Randy windowed initialization through Vulkan. The owner subsequently launched
the actual F: client successfully before testing frame generation.

True exclusive Vulkan ownership remains unavailable. Exclusive requests use the
existing borderless fallback; desktop resolution and refresh are retained.
FSR safeguards remain included, with driver-specific and actual crash validation
still pending as described in REPORT.md.

## Frame generation

Revision 8 still froze, including a controlled launch without either Twitch or
RTSS Vulkan overlays. Revision 9 adds NVIDIA compatibility pacing: the next
game frame's GPU work waits until the previous generated/real pair has actually
returned from presentation. The earlier ready fence alone did not prevent later
dependent work from being queued before an earlier presentation call.

A frame-origin regression found 104 such overlaps in revision 8 and zero in
revision 9. Twelve GPU scenarios plus the CPU wait/failure guards pass. The
completion wait has a five-second failure limit, and the first eight driver
present entries/returns are logged. Actual game confirmation is still needed.
This path can trade some throughput/overlap for compatibility. Other GPU vendors
retain asynchronous scheduling; no global driver/overlay setting is changed.
Frame generation remains off in the saved F: preferences after recovery.

Revision 7 also froze in the actual game. Revision 8 sends frame-generation
presents to a private message-only window on the game's window thread. This
avoids crossing threads during the driver's window callbacks. The game's input
window and global driver/overlay settings are preserved. If the private
dispatcher cannot be made, normal presentation continues with a logged reason.

A regression using a driver lock and synchronous window callback timed out all
108 callbacks with revision 7; revision 8 completes them on the owning thread
with zero timeouts. Single-thread rendering also keeps servicing sent messages
during SDK and GPU waits. These are controlled tests; another actual-game retest
is still needed. Frame generation remains off in the saved F: preferences.

Revision 6 also stalled in the actual game. Revision 7 prepares both generated
and real-frame GPU work before the first presentation call, preserving their
order and pacing. Previously the presenter could block inside the driver before
submitting completion signals needed by later rendering/acquisition work.

Frame generation requests enough swapchain images to acquire the pair safely;
surfaces that cannot provide them use normal presentation with a logged reason.
Real-image acquisition is bounded to 250 ms, and the render worker's interpolation
handoff has a five-second failure limit instead of an infinite wait.

The old SDK loop fails the paired-submission regression; the changed loop passes,
along with image-acquisition failures, limited surfaces, window callbacks,
resize/scale/toggles, initialization failure and slow presentation. These are
controlled tests. Actual game confirmation is still pending while the owner is
away; frame generation remains off in the saved F: preferences. The extra image
and up-front work may affect memory, pacing or latency and require a live check.

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
