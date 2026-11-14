# AO-VK Live Preview 24

## Revision 54 update

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
