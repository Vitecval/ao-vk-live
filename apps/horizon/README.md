# Rubi-Ka Horizon — 0.7.1

`InstallDetection` separates the running version, owned installed app, actual
renderer hash/revision, installation record and selected edition. File metadata
keys limit expensive verification to changed states and explicit checks. The
interactive app uses a per-user desktop mutex and activation event, independent
of portable path/version/state folder. Only isolated diagnostics bypass the
interactive gate; updater successors wait for their parent before acquiring it.
Headless two-process tests verify ownership and activation signaling without
bringing any real app/game window forward.

Companion application and installer built around the verified Preview 24 r59 engine.
The application keeps the existing r59 renderer and material archive hashes.

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

The current design is the minimal revision requested during the first preview:
Graphics / Installation / About navigation, compact settings rows, no status
badge, no promotional dashboard or slogans. The earlier dashboard executable in
the parent build folder is obsolete. See `build/horizon-preview/LATEST.json`.

For a side-by-side live preview on another monitor, enable **Render while AO is
in the background** for that selected client. With it off, the renderer still
reads saved settings but resumes drawing when AO returns to the foreground.
Existing preferences, including this option, are never changed on app startup.
