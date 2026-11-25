# Rubi-Ka Horizon — 0.7.7

The planned Display page stores startup window preferences separately for each
selected AO installation in its `randy-vk.ini` `[Display]` section. The default,
**Use game setting**, leaves AO in control. **Windowed** offers pixel sizes that
Windows reports for the selected attached monitor; duplicate refresh-rate modes
are shown once. **Borderless** fills that monitor at its existing desktop size.
There is no desktop resolution or refresh-rate change. Save explicitly, then
fully close and restart AO and its launcher. An unavailable saved monitor or
resolution is shown as unavailable rather than silently written over; select
Use game setting to recover. Refresh monitors rereads saved choices.

These choices are connected to AO's native framebuffer and full main viewport;
they do not merely stretch the final image. Modes are discovered from the chosen
monitor, with duplicate refresh variants removed. A mode still has to fit the
renderer/GPU limits and allocate successfully. Unsupported sizes, disconnected
monitors or failed window/allocation changes fall back to the working game
configuration for that run without erasing the saved preference.

Mixed Windows display scaling needs special care with AO's legacy windows.
The native override is applied only when physical pixel coordinates can be
confirmed. A bitmap-scaled legacy window, incompatible monitor DPI or unknown
DPI state keeps the working game path instead; changing monitors can disable
the override for that run. Horizon does not change the game's process DPI
awareness or your Windows scaling. Restart on the intended monitor after
changing Display choices. Independent game-interface scaling remains unsupported.

The native contract is `Mode=Game|Windowed|Borderless`, `Width`/`Height` both zero
for game sizing or both positive, and `Monitor` a Win32 display device name
(empty means automatic). These are output-window preferences, outside graphics
presets. `RVK_RenderScale` independently controls the 3D scene's internal render
resolution. Interface scaling is separate and is not offered by this page;
AO's text and controls may look smaller at higher output resolutions. Actual
framebuffer, input, projection and temporal-rendering verification belongs to
the native renderer's high-resolution checks, not the Horizon UI fixture.

The graphics layer is AO-specific Vulkan support, not universal dgVoodoo
compatibility. Do not stack competing `ddraw` wrappers in the AO folder.
Changing Display preferences never installs/replaces wrappers or rewrites
unknown wrapper sections, sound options or named custom presets.

The isolated `--self-test-display-ui --state-root <empty test folder>` check
operates actual WPF controls and two dummy client folders. It verifies mode and
monitor choices, 1080p/1440p/4K/ultrawide persistence, disconnected-monitor
recovery, restart notices and preservation of unrelated preferences, without
launching AO or changing Windows display settings. Real read-only monitor
enumeration is checked separately from the deterministic UI inventory.

See [the native display guide](../../docs/native-high-resolution-display.md)
for setup, recovery and the distinction between isolated checks and live AO
framebuffer, text, tooltip, label and mouse-picking verification.

0.7.7 includes matching Preview 24 r63 graphics and shader preparation: consistent
fresh/returned login lighting, reduced temporal shimmer, matte temple stone,
ground-rock visibility and visible startup progress. Day/Night Cycle adds
PermaDay and PermaNight. Accepted issue reports lock their completed form and
switch to Send a New Report. The current Full HD library is reused. HD armor
and creature rework remains separate for 0.7.8 and is not included.

0.7.5 adds a persistent update notice with a link to Updates, Patch notes and
Suggestions pages, tray launch for the selected AO folder, and named custom
graphics presets. Suggestions use the existing reviewed anonymous report service
without reading logs. Automated checks never send a suggestion or launch AO.

Custom presets are stored atomically in Horizon's per-user `presets.json`, with
a previous-file backup. Their active selection is tracked separately for each
AO folder. New catalog keys remain red until explicitly changed or confirmed
with Keep current value; saving under another name preserves pending choices.
Built-in presets apply only new keys' preset/default values on catalog upgrade,
leaving existing personal values intact. Older installations' graphics settings
are preserved; custom-preset tracking begins when a preset is selected or saved.
F10 integration is deferred. This release reuses the verified r61 graphics and
existing Full HD library; the private Reworked artwork is not published.

0.7.4 adds indexed quick updates, visible recovery/setup progress, tray minimize,
searchable Graphics → Keybinds, and optional Vivid Tone (off by default).
Preview 24 r61 also corrects login tone mapping, defers resize work out of window
callbacks, and bundles a matching current shader-preparation helper.
See [r61 release details](../../docs/preview24-r61.md).

Preview 24 r60 retains AO's submitted terrain topology in Shared Ground+, keeps
Adonis wall 208052 rough and non-metal, restores all five render-scale choices,
and makes fast zoning artwork visible. Quick Controls includes character
materials; Reworked artwork is held separately for approval and missing reworks
fall back to Standard Enhanced.

New installations require a Lite/Full HD choice. Folder switching, app-only
installation, Full HD removal, tooltip contrast, the in-game shortcut and
post-install update state are clearer. A Full HD removal keeps the Lite engine;
the separate graphics uninstall retains its existing ownership checks.

`InstallDetection` separates the running version, owned installed app, actual
renderer hash/revision, installation record and selected edition. File metadata
keys limit expensive verification to changed states and explicit checks. The
interactive app uses a per-user desktop mutex and activation event, independent
of portable path/version/state folder. Only isolated diagnostics bypass the
interactive gate; updater successors wait for their parent before acquiring it.
Headless two-process tests verify ownership and activation signaling without
bringing any real app/game window forward.

Companion application and installer built around the verified Preview 24 r60 engine.
The existing 19 material archive hashes stay pinned. Legacy r59 download buckets
remain reusable; the changed bootstrap is verified and refreshed independently.

Updates are checked from GitHub's latest stable release endpoint. A dedicated
Updates page checks without installing or downloading texture packs. Verified app
downloads use the asset size and SHA-256 supplied by GitHub over HTTPS; untrusted
redirects, altered bytes and expired handoffs are refused. Applying restarts
Horizon from its owned per-user installation and updates the selected AO only
when the bundled engine/edition differs. App-only updates skip the full texture
check. Startup checks are optional, non-modal, and disabled in offscreen tests.
The permanent download is `/releases/latest/download/Rubi-Ka-Horizon-Setup.exe`;
keep that filename and mark stable publications as Latest.

Report issue reads the newest whitelisted renderer log in the selected folder,
plus Horizon diagnostics, into bounded, scrubbed text (full normal logs, with the middle of very large
logs explicitly omitted). Players review the
report before compressing and posting it anonymously to the fixed asyncstuff.com WordPress
endpoint. No SMTP password or GitHub token is bundled. A local ZIP is kept as a
fallback. `apps/wordpress/horizon-reports` supplies the disabled-by-default mail
receiver; the site owner must install, enable and verify WordPress mail delivery.
No raw logs are published or emailed by automated tests.

Build using `tools/build-horizon.ps1 -Tests`. Output:
`build/horizon-preview/minimal/Rubi-Ka Horizon Setup.exe`. The WPF app targets x64 Windows
10/11 and the OS-provided .NET Framework 4.8. No WebView, Electron or separately
downloaded .NET runtime is used. The renderer itself remains 32-bit.

For a fresh source checkout, first download the pinned r59 Full Setup archive
from the Preview 24 release into `build/preview24/release-r59/`. Its file name is
`ao-vk-live-0.6.0-preview.24-windows-x86-full-setup.zip`. The expected SHA-256 is
`0b9cd09e7111cb6e42f5534bc297708b5c731c1ff21f486ef6a611b87e49f860`.
The checked-in catalogue, compact manifests and bootstrap index are sufficient
for the app build; the package step checks the downloaded bootstrap against
`Assets/engine.json`. Generating the catalogue again requires the native exporter.

The same EXE is the portable app and installer. A hashed ZIP overlay contains the
r59 bootstrap; it is verified before extraction. This avoids loading the full
setup archive into the UI assembly. The app can copy itself into the current
user's Programs directory, create a Start-menu shortcut and register its own
uninstaller. Removing the app preserves AO, preferences and downloaded files.

Live controls read/write only `[Renderer]` in the selected `randy-vk.ini` using
Windows profile APIs. Existing r59 checks edits every 500 ms, including while
background presentation is suspended. No injection, window messages or remote
threads are used. Controls refresh external edits, debounce changes and support
guarded undo. The catalogue and exact preset write sets are exported from an
isolated production DLL by `horizon_catalog_export`; no retail client is loaded.

Install/update/repair/uninstall/reset use the existing checked PowerShell backend
in a hidden 32-bit worker. Process guards, hash verification, transaction recovery,
personal file protection and Full-cache binding stay in that backend. Lite has
no texture download metadata. Full reuses the 19 existing pinned companions and
14,345 portable cache files. Downloads/partials persist in the chosen work folder;
temporary expanded artwork duplicates are removed after the worker exits.
Full HD repair hashes all installed artwork/cache files and can take several
minutes. Progress and errors are flushed to `last-setup.log` while the worker is
running. A quick executable-path guard rejects the selected running client before
preparation; authoritative shared-loader checks remain in the 32-bit backend.

Graphics settings are not written merely by opening the app. It never selects or
changes another running AO client automatically. The user's explicitly chosen
client folder is shown in the sidebar. The app's static interface uses software
rendering to avoid competing for the game's GPU.

Diagnostics: `--self-test --state-root <isolated folder>`,
`--self-test-setup --stock-fixture-dir <isolated original DLLs> --state-root <folder>`,
and `--render-preview <png> --page Graphics --client <folder>`. Render previews
measure/draw the app's own visual tree without opening a desktop window.

The background is the existing Frontier Bridge loading artwork; its original
artwork terms remain in force. Renderer/app source licensing and artwork licensing
remain separate. Internal AO-VK ownership records and file names are preserved for
compatibility with existing installations.

Texture-release rules:
- Bump Horizon's release version for content changes; never replace a published
  texture ZIP with different bytes at an existing pinned URL. Keep old material
  releases available for clients whose manifests still reference them.
- The current r59 bootstrap and companions are frozen. A new artwork release
  needs a matching new bootstrap, embedded Full manifest, hashes and versioned
  material source. Update the downloader's explicit source/version pins when
  introducing that source; the app cannot invent a new texture catalogue.
- Rebuild affected prepared mip chains into a fresh preparation output. Do not
  trust an older cache merely because paths, file sizes or timestamps match.
  Check rebuilt sets against fresh source decoding before packaging them.
- Check added, replaced and removed assets with the same renderer version.
  The update decision compares ownership in both directions, so retiring an old
  asset still runs the transaction. Only valid generated bindings are extra.
- The app's Full catalogue must match the Full manifest actually embedded in the
  bootstrap. Packaging refuses mismatches. Downloads reuse verified unchanged
  archives; a changed archive is downloaded as a whole ZIP, not a binary delta.

The current design is the minimal revision requested during the first preview:
Graphics / Installation / About navigation, compact settings rows, no status
badge, no promotional dashboard or slogans. The earlier dashboard executable in
the parent build folder is obsolete. See `build/horizon-preview/LATEST.json`.

For a side-by-side live preview on another monitor, enable **Render while AO is
in the background** for that selected client. With it off, the renderer still
reads saved settings but resumes drawing when AO returns to the foreground.
Existing preferences, including this option, are never changed on app startup.
