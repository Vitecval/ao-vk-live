# Rubi-Ka Horizon 0.7.0

- First stable application release. The permanent installer link uses GitHub's
  Latest stable release and the fixed name `Rubi-Ka-Horizon-Setup.exe`.
- Added automatic startup checks (optional) and a dedicated **Check for updates**
  button. Checking is separate from downloading and applying an update.
- Updates check the published size/SHA-256, refuse untrusted redirects and changed
  installers, and hand over after the old app exits. The previous owned app is
  backed up; settings and cached textures remain. Matching graphics skip a full
  texture check during app-only updates.
- Added **Report issue**: latest selected-client log and Horizon diagnostics,
  common personal details removed, player review, compressed anonymous upload
  for private email to dev@asyncstuff.com, with a saved local ZIP fallback.
- The WordPress receiver requires installation/enabling by the site owner.
  It bounds requests/decompression, limits spam, deduplicates accepted reports
  and creates no public report files. Live inbox delivery depends on site mail.
- The renderer and all 19 texture archives remain the verified r59/Preview 24
  assets. This application release does not claim a new retail FPS improvement.

## Application foundation

- One Windows EXE for installation and live graphics settings. Minimal interface
  with existing AO landscape artwork, search, presets and guarded Undo.
- Lite retains the original world textures and downloads no texture packs.
  Full HD reuses the 19 pinned Preview 24 companions and 14,345 prepared cache
  files. The bundled renderer is unchanged r59.
- Install, update, repair, reset settings and remove graphics from the app.
  Install the app to the current user's Start menu or use the portable EXE.
- Fixed the white disabled settings list while setup is busy. Setup status and
  a continuously flushed log now explain lengthy Full HD verification.
- Removed quadratic Full HD receipt-list updates, replaced repeated scans of
  changed file paths with hashed lookups, and removed duplicate reads of each
  verified staged file. All integrity and transaction recovery checks remain.
- A selected-client check blocks setup before expensive preparation if that
  client is running; shared loader checks remain in the 32-bit installer.
- Preview limitations: a new online 28.2 GB Full installation and retail live
  setting changes have not been revalidated end-to-end on other hardware.

## AO-VK Live Preview 24 engine

## Revision 59 update

- Fixed unnecessary window background clears after Vulkan initialization.
- Allocate reusable static sun-shadow depth lazily, with memory headroom and
  bounded recovery after partial allocation failures. Moving views avoid an
  unused 64 MiB allocation.
- Skip wholly off-map ends of large retained terrain/rock shadow batches while
  preserving original triangles and draw count.
- Stop seven captured ICC moving-scenery parts accumulating historical shadow
  poses. Keep current moving shadows and stationary texture-sharing structures.
- Validation: 34 shadow-retention cases, five pixel-identical 200 m shadow image
  comparisons, 120 repaint requests, allocation recovery, zoning and installer
  rollback checks. Actual retail ICC FPS improvement remains to be measured.
- Lite has no texture downloads. Full reuses the existing 19 texture/cache
  archives and 14,345 prepared cache files. Existing settings are preserved.

## Revision 55 update

- Fixed moving ships leaving retained copies of old shadow poses. Sky-only ships
  do not cast ground shadows; nearby hulls keep their current moving shadows.
- Very broad ship/environment lights keep their original illumination without
  rendering an entire area into low-detail local shadow cubes. Nearby local
  lights retain their shadows. Grass remains at the existing range and density.
- Captures from Borealis showed three 1,041 m lights and thousands of point-shadow
  caster draws. Targeted tests reproduced 1,200 retained ship poses before the fix
  and zero after, with Immersive Ships both on and off. A normal structure still
  retains one shadow pose; local light rendering remains pixel-identical.

## Earlier Preview 24 updates

- Expanded the loading-tip deck from 50 to **545 unique tips** using the supplied
  500-tip list. Complete-deck shuffling avoids repeats, including across decks.

- Fixed Original preset zoning screens remaining over a live game until their
  timeout. Readiness now follows actual world presentations with enhancements off.
- Added optional **Click to continue...**, off by default. Once zoning is ready,
  only the prompt fades in; the loading picture stays in place.
- Background rendering defaults off for logged-in characters. Login and character
  selection keep rendering; the restriction ends immediately on logout.
- Included the lightning/sun-shadow fixes, indexed scenery and conservative
  occlusion fixes for disappearing scenery when looking upward.
- Enhanced Rocks uses the original meshes with detail-only material improvements;
  experimental geometry changes and rock clusters are disabled.
- Fixed GPU feature/limit compatibility, partial-startup resource leaks, failed
  texture uploads, noncoherent memory handling and DirectDraw lifetime, bounds and
  low-memory failures. Grass range remains 80 m.
- Hardened installer cache sharing, Unicode paths, interrupted updates and cache
  warmup. Both editions retain Reset settings and preserve existing preferences.

Validation includes 54 stability regressions, 16 instrumented memory runs, four
pixel-identical rendering comparisons and a two-hour mixed rendering run covering
4,629 zone cycles and 1,147,992 frames. Revision 53 additionally passed repeated
Original zoning, prompt/input/fade, settings and 16 DirectDraw checks. These are
isolated tests, not a guarantee of retail FPS or certification on every GPU.

Preview 24 brings prepared texture data, faster zoning and more stable sunlight
shadows to the classic Anarchy Online live client.

Choose **Lite** for the renderer, loading screens, ambient life and other visual
features using the game's existing textures. Lite contains no replacement texture
packs or prepared material cache and downloads no texture archives.

Choose **Full HD Setup** for everything in Lite plus all replacement textures and
the prepared material cache. The setup downloads the required, verified companion
archives automatically. You can also place all companions next to the setup ZIP
for an offline installation. Every archive must belong to Preview 24; mixed
release archives are rejected. Keep the setup folder to reuse downloads later.

The portable cache contains losslessly compressed, prefiltered material mip
chains. Setup connects these to your own installation, avoiding PNG processing
while zoning. Smaller texture-quality choices reuse the prepared mip levels.
Changed, incomplete or corrupt cache data falls back to the original texture
loading path. The cache reduces texture preparation; it cannot guarantee an
identical zoning time in every area or on every computer.

Sun shadows retain stable coverage through camera turns and mixed depth ranges.
Nearby cascades refresh every frame; the two widest alternate updates. Static
opaque shadow depth is reused when its inputs match. The default shadow distance
is now **200 metres**. Existing preferences are preserved on update; change the
distance in the in-game options or reset settings to use the new default.

## Install and update

1. Extract the Lite ZIP or Full HD Setup ZIP into its own folder.
2. Close the target game client and its launcher.
3. Run **Install.cmd**, or **Update.cmd** for an existing AO-VK installation.
4. Select the folder containing AnarchyOnline.exe. Start AO normally after setup
   finishes and select Primary Display Driver. Open options with Ctrl+Shift+F12.

Setup checks client compatibility, file ownership, archive hashes, available disk
space for Full downloads and whether affected files are in use. Files are staged
and verified before a recovery journal commits them. If setup is interrupted,
run it again to recover before continuing. Unknown wrappers and personal files
are preserved; a conflict can require resolving the named file before retrying.

**Update.cmd** can switch editions. Full to Lite removes unmodified textures and
cache files owned by the previous install. Personal or modified files are left
alone. Lite to Full fetches only the required Full companions. Both preserve your
settings.

## Revert settings and uninstall

Run **Reset settings.cmd**, or choose **Reset settings** in Setup, to restore
AO-VK's defaults. Close the target client first. This resets AO-VK preferences;
the renderer and installed artwork stay installed. Defaults are created on the
next start. You can also choose this from **Uninstall.cmd**.

Choose **Uninstall** to remove recognized, unmodified files owned by AO-VK.
Settings, personal packs and unrecognized or modified files are preserved.
Setup reverses only the executable memory changes it owns and can verify.

This is an experimental preview. Recovery and failure handling are tested, but
hardware, antivirus, disk failure and third-party modifications can still cause
errors. Setup reports these rather than claiming an installation succeeded.

Replacement-artwork licensing and provenance remain in LICENSE-TEXTURES.md and
the Full pack metadata. Original client DLLs, private Living exports, credentials,
personal settings and GPU-specific binary caches are not distributed. The
prepared material cache is portable artwork data, separate from the GPU cache.
