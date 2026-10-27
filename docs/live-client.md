# Experimental classic live addon

This build targets the audited **18.8.62_EP1 classic live client**. It does not target New Engine, TestLive, or arbitrary earlier/later builds. It is an experimental port, not a claim of completed gameplay testing.

## Installation

Close the game and launcher. Extract the whole Windows release ZIP and run **Install.cmd**. Choose the AO installation folder or click Browse, then click Install. **Update.cmd** and **Uninstall.cmd** open the same setup with that operation selected.

For command-line use, run `install-live.ps1 -ClientDir 'C:\Funcom\Anarchy Online'` from the packaged build. It verifies the six audited game DLLs and the addon, refuses to replace an existing graphics wrapper, and installs:

- `ddraw.dll`: a small loader that forwards normal DirectDraw calls to Windows.
- `ao-vk.dll`: AO-VK, loaded on the classic renderer's first graphics call.
- `ao-vk-live.install.json`: the installed version, owned file hashes and any extra loader location, used for update and uninstall.

To update an earlier live test package, close the game and launcher and run `Update.cmd`, or add `-Upgrade` to the installer command. It accepts only the known AO-VK builds recorded in the package manifest, verifies and stages the update, and rolls back if copying fails. Other graphics wrappers are refused.

Clients from another AO installation can remain open if their loaded modules do not use the selected folder or a loader being changed. The installer checks renderer/module paths, including AppData executable copies, and blocks when it cannot identify a running client safely.

Updates leave byte-identical DLLs in place. An unrelated client may therefore keep an unchanged shared loader open; replacing or uninstalling a shared loader still requires closing its users.

**Keep the official `randy31.dll` in place.** No `randy31_orig.dll`, patched executable, checksum edits, or manual server arguments are needed. Start with the normal official launcher or your normal quick launcher. The official launcher and patcher use the system renderer. The addon activates only in the actual `AnarchyOnline.exe` game process with the supported classic renderer.

Use this loader by itself; a pre-existing dgVoodoo, D7VK, or other `ddraw.dll` cannot occupy the same filename. The installer refuses to overwrite one. Do not copy the optional PRK AOReloaded `version.dll` as part of this package.

### AppData executable copies

An installed AOPP `MSWSOCK.dll` can automatically relaunch a copy of `AnarchyOnline.exe` from `%APPDATA%\AOPP`, even when you start the normal `Anarchy.exe` in the game folder. Windows looks for the graphics loader beside that copied executable. The installer detects that layout and also installs the small `ddraw.dll` there. `ao-vk.dll`, settings and assets stay beside the actual retail `randy31.dll`. A custom executable-copy folder can be selected with `-LoaderDir`. Uninstall checks and removes those copies of the loader as well. Other installed DLLs and launch settings are preserved.

The loader log is beside each loader DLL; in this layout, check `%APPDATA%\AOPP\ao-vk-loader.log`. Restart a running game after installing, since its existing system DirectDraw stays loaded until exit.

The first graphics startup can take longer while the driver compiles shaders. Later starts use `randy-vk.cache`.

## Graphics and in-game options

**Ctrl+Shift+F12 → General → Pause rendering when AO is not focused** suspends
graphics for background clients, retaining their last picture until you switch
back. The game stays connected and continues its simulation; its CPU work is
not eliminated. Rendering resumes automatically. Minimized windows already
suspend rendering. This new option defaults off and stays unchanged by presets.

**Ultra** now selects AnarchyHD world artwork, Shared Ground+, global terrain/building
detail at 1, full character/equipment materials and moderate distant horizon haze.
Shared Ground+ automatically enables the working GroundOn terrain details while
retaining enhanced floor textures, with extra tile-edge blending and added terrain
AO/sun/dune relighting off. The saved manual GroundOn switch remains unchanged.

Select **Primary Display Driver** in the launcher. This is the device name advertised by the Vulkan wrapper; it does not select CPU rendering. `randy-vk.log` identifies the actual Vulkan GPU. The game still uses the CPU for simulation and preparing frames.

Press **Ctrl+Shift+F12** for AO-VK's standalone options panel. Use Tab / Shift+Tab to change category, Up / Down to select an option, Left / Right to change it, Backspace to restore its default, and Esc to close. Shift makes numeric changes larger. Changes apply immediately and save automatically. On the preset row, Left / Right chooses and Enter applies. The console's `options` command opens the same panel; the key left of 1 opens the console. This panel is separate from the game's F10 preferences and requires no AOReloaded DLL.

The native terrain replacement remains disabled by default on live. Preview.6 repairs the known invisible-ground path: live's combined lightmap draw is now replaced with a complete native draw, and unknown passes retain AO's original draw. Synthetic GPU tests cover both cases; actual playfields still need live review before enabling it by default. Other Vulkan improvements work with the original ground mesh.

Grass is enabled by default and works over the original terrain. The live ground uses an opaque, lit draw with the lightmap at texture stage 0 and the ground texture at stage 1; AO-VK now captures both for grass generation. This does not require the experimental terrain replacement. Classic+, Modern and Ultra keep grass enabled. The performance HUD defaults to off; both grass and the HUD are available in Quick controls.

## Undo and diagnostics

Closing the render window now drains queued work, stops the render worker and releases Vulkan presentation, including frame-generation threads, before DLL teardown. Some game graphics objects can outlive the renderer's shutdown; presentation no longer depends on their final release. The log records `shutdown: render worker stopped` and `shutdown: presentation released`. This fixes a missed-wake race in worker shutdown and a missing window-close cleanup path; it does not establish the cause of every stuck AO process or address another addon's shutdown.

Minimized or zero-area windows suspend world rendering. Restoration refreshes the swapchain; if creation was deferred because the window had no usable area, later drawable frames retry it. This also handles clients that start minimized. The log records `presentation: minimized or zero-area window` and `presentation: window restored`. A full graphics driver reset can invalidate the Vulkan device; this build stops recording frames after that failure, but does not recreate the whole renderer. Restart the affected client to recover from device loss.

Run **Uninstall.cmd**, select the same AO folder and click Uninstall; or run `install-live.ps1 -ClientDir 'C:\Funcom\Anarchy Online' -Uninstall`. It removes only owned DLLs and the install record, including any recorded extra loader copy. Modified or unknown wrappers are refused. Game files, preferences, other addons and AO-VK settings/cache/logs are preserved. Uninstall works after an official update changes the original game DLLs; remove the addon until the new build has been audited.

- `ao-vk-loader.log`: whether the addon loaded, or Windows/unsupported-build fallback.
- `randy-vk.log`: renderer, GPU, native replacements, hook status, crashes and hangs.
- `randy-vk.ini`: settings; the original AO-VK hotkeys and console remain available. Ctrl+Shift+E toggles enhancements.

The loader falls back to Windows DirectDraw if it cannot load the addon or the renderer build is unsupported. Once the addon is active, a fault in a native renderer replacement cannot be recovered by changing wrappers in the same process; remove the two addon files to return to the stock renderer.

## Compatibility changes

Classic live Randy has all 771 PRK exports at the same ordinals/RVAs, and identical executable code and initialized data. The ten file-byte differences are PE/export timestamps and the PE checksum. DisplaySystem and FXS also have matching executable code. N3's ground and ring entry points match. Gamecode and GUI require distinct addresses:

| Hook | PRK RVA | Classic live RVA |
| --- | --- | --- |
| GUI exception prologue | `0x1738A4` | `0x173954` |
| Stars vtable / Update | `0x16CD6C` / `0xF7F3C` | `0x16DDAC` / `0xF8491` |
| Smoke vtable / Update | `0x16CAF4` / `0xEFF68` | `0x16DB34` / `0xF04BD` |

The inspected smoke and stars instructions match after code/global-address relocation. The live loader routes the four game modules' renderer imports through AO-VK's existing thunks, including texture/mesh identification; data exports still forward to the actual stock renderer. Modules loaded later are routed at presentation. Native code uses the registered original module rather than assuming a renamed DLL.

## Validation performed

- Windows x86 build with clang-cl and LLD. LLD is required to preserve the decorated C++ export aliases.
- SHA-256 preflight of Randy, DisplaySystem, Gamecode, GUI, N3 and FXS.
- All 771 retail renderer exports resolve; shared data addresses agree with the original.
- 465 renderer imports verified across DisplaySystem, Gamecode, GUI and FXS.
- Existing native comparison suite against retail Randy: **all passed, zero failures**.
- 18 real-module profile checks: PRK and classic live recognised, New Engine rejected.
- DirectDraw shim: all 22 ordinals resolve; system locks/enumeration work; unrelated processes do not load AO-VK.
- Renderer harness: Vulkan frame rendered and visually inspected on RTX 2070 Super, using the unmodified retail renderer and no backup renderer.
- Harness with the retail modules loaded: GUI, N3 rings/ground, DisplaySystem ground, particles, weather, smoke and ground-lightmap hooks installed; four frames rendered successfully.
- Relocated-executable test: harness and loader in a different directory from the game DLLs/addon; four Vulkan frames rendered with the retail modules loaded.
- Standalone options: rendered and visually inspected at 640x480; keyboard changes saved, category navigation and console entry exercised. Classic+, Modern and Ultra each tested to retain the live terrain workaround.
- Normal-exit harness: exit code 0 and presentation released with frame generation on/off, threaded/calling-thread rendering, native/original device cleanup, and window destruction both before and after Randy cleanup. Use `--normal-exit` or `--window-close-first` to exercise this path; the default harness still uses forced termination.
- Minimize/restore harness: six configurations, four restore cycles each, including frame generation on/off, direct/threaded rendering, original/native device cleanup and minimized startup; all exited normally, resumed swapchain creation and produced rendered images without logged device loss. Use `--frames 32 --minimize-test --normal-exit`, optionally `--start-minimized`.

The user logged into classic live and confirmed that disabling only the replacement terrain restores the ground in ICC with enhancements enabled. Zoning, all effects, long sessions, and interactions with other injected addons still need gameplay testing. A local `MSWSOCK.dll` in the existing retail installation crashed the isolated tests before the stock renderer loaded; the clean harness uses Windows system libraries rather than that additional DLL. It has not been altered in the retail installation.

## Build and tests

See [build-windows.md](build-windows.md) for the portable Windows build script and dependencies. The initial binaries used Visual Studio 2026, clang 19.1 and LLD 15 with the shader tools listed there.

For offline native tests, copy only the required game libraries into an isolated folder and let Windows supply system DLLs. Put the addon, loader and tests there. Run `live_check --modules`, `loader_check`, `client_build_check PRK_DIR LIVE_DIR NEW_ENGINE_DIR`, and `native_check ABSOLUTE_PATH_TO_RETAIL_RANDY`. `randy_harness --frames 4 --shot frame.bmp` tests drawing; `RANDYVK_TEST_MODULES=1` also loads the retail modules to exercise installation of their hooks. Do not redistribute the game's DLLs with the addon. The installer fixture suite uses only dummy bytes and needs no game files.
