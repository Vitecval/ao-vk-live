# Preview 22 — local Living Rubi-Ka test candidate

Candidate 6 removes timer fading from continuous aircraft. They enter only while
outside view or beyond draw distance, retain their traffic slots, and continue
complete closed flight plans while visible. A visit retires after a full lap
only once hidden again. High and Ultra retain their simultaneous limits and
shorter rests; their former short-pass rate figures do not describe these longer
visits. LRSCENE9 carries complete curved Phasefront and large-craft plans. Every
cubic segment is checked with a continuous full-craft envelope against native
scenery and terrain. Low passes have complete checked climb/return paths where
possible; an unverified approach is omitted. Original distant transport loops
also keep flying through schedule deadlines. Existing original resources and
Preview21 artwork remain unchanged.

Candidate 5 adds Phasefront High and Ultra. High rests 12–24 seconds between
complete passes per craft with 10% skipped visits; Ultra rests 2–6 seconds and
does not skip visits. Dense schedules favor already checked low lanes, retaining
the original cruise speeds and clearance envelopes. High permits three local
Phasefronts, Ultra four; combined air caps are four and five respectively,
including at most one large spacecraft. Low and Medium retain their schedules.
Existing city caches are compatible; no original resource preparation is needed.

Candidate 4 keeps pedestrians at walking pace until their destination, replacing
the long braking/creeping finish with a brief 140 ms stationary idle transition.
Occasional Phasefront skims target approximately 10 feet above terrain in newly
checked clear corridors. Full vehicle/rider height, bank, bob and steering are
included in a continuous swept-box/triangle test. Larger ground triangles are
tested precisely rather than treated as imaginary solid AABB walls. Other
flights and the large spacecraft keep their previous corridors and traffic caps.

Candidate 3 adds human pedestrian activity: branching validated paths, varied
destinations and pauses, occasional native shrugs, nods and head-scratching,
and native sit-down / ground-rest / stand-up sequences at verified flat spots.
The complete activity envelope and floor-contact samples participate in local
preparation. Unsupported bodies remain standing rather than using a bad pose.
Phasefronts choose freshly varied passes, directions, speeds, timing and small
steering corrections within prevalidated low/mid-height corridors. Choices are
stable within a pass and differ between visits; no per-frame random teleporting.

This unpublished candidate reuses the recovered V5/V5.1 engine inside the exact
Preview 21 renderer. Setup prepares original cosmetic resources from the selected
AO installation and keeps the resulting cache inside that same AO directory.
No original asset pack, game DLL, extracted model, animation or terrain export
accompanies the download. Normal rendering, Parallax V9 and accepted Preview 21
artwork are preserved. Living defaults Off, with an explicit isolated Newland
test command and settings independent of graphics presets.

Candidate 2 adds 23 original appearances, compatible skin/clothing materials,
different body/head shapes, coats and occasional original armor. Faction pools
and deterministic assignment preserve city identity. Enlarged bodies are
filtered against complete original scenery along the whole pedestrian route.

Wraith X2-45, Spectre and Banshee use their actual item-linked models, original
rider poses and correctly attached heads. All 21 supported cities have static
swept-corridor checks for Phasefronts and the rare, larger original Mech drop
ship. New aircraft have separate settings, shared traffic caps, native material
masks, navigation lights, distant LODs and smooth entry/exit fading.

Run Prepare Living City Test.cmd to opt into all-city static-route review, then
open Ctrl+Shift+F12 > Living Rubi-Ka. All new features remain optional. Actual
combined live gameplay, long sessions and dynamic obstacle validation remain
pending. Read REPORT.txt and PLAYTEST-22.txt before testing. Installation is
performed only when requested; no GitHub push or release publication is included.

Full HD Setup uses the exact pinned Preview 21 material companions. New worker
artwork requires a separate accepted material freeze. The ordinary installer
retains transactional rollback; optional local asset preparation follows it as
a separate atomic cache-generation step and reports failures independently.

## Preserved Preview 21 release notes

The following describes the original Preview 21 scope, before this local-cache
integration candidate.

# AO-VK Live Preview 21 — Window Interiors & Expanded HD Textures

Free to use with any compatible launcher. Redistribution of the HD artwork is not permitted without authorization.
See LICENSE-TEXTURES.md; third-party licenses and permissions previously granted remain in force.

**Updated artwork-license packaging:** use a fresh Full HD Setup ZIP with the
matching material archives from this release. Each companion now includes the
artwork license, so its pinned archive checksum has changed. The texture pixels
and verified 8,627 texture IDs are unchanged.

**Setup 2 — 4 GB memory support:** Lite and Full HD now offer **Allow up to 4 GB
memory (recommended)** on 64-bit Windows. Setup backs up recognized game/launcher
executables before changes, preserves existing AOSharp/AOPP patches, and restores
its own changes on uninstall when the files still match. This helps address
32-bit memory pressure. No game executable is distributed. Uncheck the option
to keep executable contents unchanged. See docs/large-address-aware.md.

**Setup fix — 2 November 2026:** If setup reports that `Value of '-1' is not
valid for 'Value', download and extract a fresh Lite or Full HD Setup ZIP from
this release, then run Install.cmd again. The corrected setup handles Install,
Update and Uninstall. Your existing graphics settings are preserved. Previously
downloaded material ZIPs are unchanged and can be reused.

An experimental graphics upgrade for the **classic Anarchy Online live client**.
Choose one download below; both editions include the same renderer and window interiors.

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

Your graphics settings and personal packs are preserved. Original game DLLs stay
unchanged; optional 4 GB executable changes are backed up and recorded for uninstall.
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
