# AO-VK Live

An experimental graphics upgrade for the **classic Anarchy Online live client**.
Choose one download below; both editions include the same renderer and window interiors.

[Download Preview 21](https://github.com/Vitecval/ao-vk-live/releases/tag/v0.6.0-preview.21)

Free to use with any compatible launcher. Redistribution of the HD artwork is not permitted without authorization.
See [the HD artwork license](LICENSE-TEXTURES.md); third-party licenses and permissions previously granted remain in force.

## Choose your download

**Lite:** Download `ao-vk-live-0.6.0-preview.21-windows-x86-lite.zip`, extract it,
then run **Install.cmd**. Lite includes the renderer and essential resources,
without the optional Full HD replacement library.

**Full HD:** Download `ao-vk-live-0.6.0-preview.21-windows-x86-full-setup.zip`,
extract it, then run **Install.cmd**. Setup downloads and verifies all required
Preview 21 material packs automatically. You do not need to download or extract
the other material ZIPs yourself. Progress is shown, valid downloads are reused,
and interrupted downloads can resume. No extra download manager or software is needed.

1. Close AO and its launcher. Open Install.cmd and choose your AO folder.
2. Choose Install, or Update for an existing AO-VK installation.
3. When setup finishes, launch AO normally and select **Primary Display Driver**.
4. Press **Ctrl+Shift+F12** in game to open the graphics options.

The recommended **Allow up to 4 GB memory** option backs up recognized game and
launcher executables before changes and preserves existing AOSharp/AOPP patches.
Uncheck it to retain executable contents. See [4 GB memory support](docs/large-address-aware.md).

Your graphics settings and personal packs are preserved; original game DLLs stay unchanged.
Setup verifies every pack before a transactional update and rolls back if the
update fails. First-time graphics-card preparation can take one or two minutes.
Allow roughly 10 GiB free for downloads, textures and safe installation staging.

The three `full-materials-*.zip` assets below are for the installer and optional
offline use. For offline installation, put all three matching ZIPs beside
Install.cmd (or keep them in its parent download folder), then run setup.
`SHA256SUMS.txt` contains checksums for all five ZIPs.

## What's new

- **Parallax Window Interiors V9:** homes and offices gain room depth, beds,
  desks, curtains and belongings, with warmer and cooler lighting, dark/dim/lit
  rooms, adjoining-room views and optional slow lighting changes.
- **Expanded HD artwork:** **8,627 unique original texture IDs with HD albedo
  replacements**, including 1,309 newly accepted IDs. The library contains 26,831 runtime
  maps; normal, ORM and emission maps are not counted as extra artwork IDs.
- **Building Weathering** and the existing GroundOn and Shared Ground+
  improvements remain, including original ground colours and classic lighting.
- **GPU pipeline-cache/startup work:** a compatible cache is prepared locally
  during installation and reused on later launches. No hardware-specific cache
  is distributed, and cold preparation can still be lengthy.
- **Updated presets and controls:** Classic+ and Modern select Medium windows;
  Ultra selects Maximum (V9 Strong). Existing saved Strong settings are retained.
  Important options come first; Enter expands related settings.
- **Independent HD selection:** choose texture styles and HD options in Classic+,
  Modern or Ultra without having the quality preset reset those choices.

## Before you play

This is a **prerelease**, not a stable release or a complete game-wide visual
overhaul. Some surfaces retain original artwork; retail mesh/UV appearance,
long sessions, repeated zoning, Alt-Tab and multiple live clients still need
player feedback. Offline renderer measurements are not AO launcher timings.

Supported: the audited **Windows classic live 18.8.62_EP1, 32-bit** client and a
compatible Vulkan GPU/driver. New Engine, TestLive and Linux/Wine are not validated.
Setup refuses unsupported game builds and unknown `ddraw.dll` wrappers. Do not
replace randy31.dll. If Windows denies access, rerun the CMD as administrator.

Living Rubi-Ka decorative people, clothing, drones, traffic, wildlife and machinery
are **not included**: their current resource packs are non-distributable. Old
private caches cannot activate those systems in this build.

Validation includes 28 regression checks, ten setup/download/recovery checks,
exact V9 visual comparisons, and real material archive verification in 32-bit
PowerShell. Download retry/resume was tested using a local HTTP fixture and
offline installation paths; **a fresh end-to-end online game installation has
not been tested**. Public asset reachability and hashes are checked separately.

If you encounter a problem, keep your logs/crash dump and tell us the location,
settings, steps and GPU/driver. Back up your working setup before experimenting.

[Project page](https://github.com/Vitecval/ao-vk-live) ·
[Report a problem on GitHub Issues](https://github.com/Vitecval/ao-vk-live/issues)

## Source and credits

Renderer code and shaders match the validated Preview 21 integration. Owner-only
experiment reports, private caches and generated optional artwork are excluded
from this source tree. Prebuilt maps are in the release assets; their catalogues
and provenance hashes remain in source. See docs/preview21-source-origin.md.

Based on [wannabuh/AO-VK](https://github.com/wannabuh/ao-vk), under the preserved
[MIT license](LICENSE) for the renderer software. Separately licensed HD artwork
is covered by [LICENSE-TEXTURES.md](LICENSE-TEXTURES.md), with existing MIT/CC0
artwork grants and third-party rights preserved. Third-party notices are in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and licenses/. This is an unofficial
community project, not affiliated with Funcom. Original game DLLs are not bundled.
