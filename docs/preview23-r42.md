# Preview23 revision 42 - stable shadows, restored ships and safer setup

This is a local test update. Packaging and publishing remain pending the user's approval of the in-game result. No release ZIPs or published Preview23 downloads were rebuilt.

## Shadows

The F: log recorded `RVK_ShadowMem=1`, a 400 m shadow distance, repeated changes to Skip HUGE shadows, missing statel packs, and cached casters forgotten because they were in view but not drawn. This agrees with the reported turn-away failure: the game can omit scenery through culling or LOD without removing the structure. The r40 huge-shadow filter also recomputed each building's combined bounds from the current submission, so missing pieces changed its classification.

The update makes these decisions independent of that changing subset:

- Substantial known static scenery can enter the shadow cache on first sight, after the existing owner/movement checks. It no longer needs twenty consecutive visible frames.
- Known static casters are not discarded merely because they are undrawn near the camera. Distance eviction remains bounded, and playfield changes explicitly clear retained casters and classification data.
- A terrain-only frame can use the captured world camera to render remembered casters even with zero current caster submissions.
- Huge-shadow bounds accumulate per placement in a bounded table. An exclusion stays fixed until the playfield changes. Identity-space geometry remains separate. The game's static kind and actor ownership determine eligibility; the generic freshly-uploaded mesh flag no longer temporarily treats ordinary scenery as animated.
- Cascade coverage is centered on the camera position, with existing texel snapping, rather than moving with camera direction. All cascades refresh together, avoiding mixed old/new exclusion decisions.
- Skip HUGE shadows defaults **off**, and applying any preset explicitly turns it off. The optional control remains available.

The renderer can retain only geometry the game has supplied, or that a verified statel pack provides. Normal cascade resolution and soft-shadow detail still vary with distance; these changes prevent disappearance from view-dependent caster removal and huge-building classification.

## Ships and zoning

The earlier r39 smooth hull, raised cabin and rounded engine pods are restored, replacing the r40 bevelled rectangular hulls and grid-like belly panels. Routes and sky-only integration remain unchanged.

Zoning aspect-fits the complete image into the window. The phrase is painted over the image above its AO wordmark in Segoe UI, with a thin dark outline for readability. There is no reserved text footer or filled panel. The fifty phrases, shuffle behavior and fades remain intact.

## Install, update, uninstall and reset

The public uninstaller now preserves unrecognized, modified and unverifiable files while continuing to remove recognized files. Unknown asset records are ignored during uninstall without following their paths. Loading-image paths are now accepted in the shared allowlist; their absence from that list was a concrete failure when uninstalling the r39-r41 installation records. A shared loader mapped by an unrelated AO installation is preserved rather than blocking removal of the selected installation. Running clients that actually use the selected installation still block changes.

Install/update continue to reject unrecognized files they would replace. Modified obsolete assets with no new replacement are left untouched. Interrupted setup recovery checks whether the selected installation is running before changing files. Transaction rollback, staging integrity and path restrictions remain in place.

`tools/Reset-Settings.cmd` opens reset-only setup. `tools/reset-settings.ps1 -ClientDir <folder>` deletes only that client's `randy-vk.ini`, leaves the renderer and artwork installed, and succeeds when the INI is already absent. It checks the selected client's processes and pending setup transaction. The GUI explains the reset separately from uninstall.

Future public packages receive a standalone `Reset settings.cmd`, the reset script, and a choice in `Uninstall.cmd` between uninstalling, resetting settings only, and cancelling. Both Preview23 assembly paths refresh these launchers and include the zoning images. These source changes are ready for the next package; the package builders were not run.

## Verification

The final x86 renderer hash and check records are in `build/preview23/atmosphere-r42/VERIFIED.json`.

The actual Vulkan fixture retains a visible ground shadow after its building is omitted following either one or thirty observed frames. Both results, and the camera-turn result, have zero changed pixels compared with continued submission. Approaching changes 192 boundary pixels in the 768x512 comparison instead of removing the 178,874-pixel shadow. A partial huge-building submission is stable with shadow memory both on and off. The negative control changes 15,360 pixels when the huge-building filter is applied, proving the filter is exercised.

Sky/ambient tests and the sky camera precision, TAA and horizon suite pass. All fifty phrases render over the complete image at five window shapes without changing the logo or image edges. Presets, default-off behavior and saved settings pass through process restart.

The production installer suite passes install/update/uninstall, process guards, stock/wrapper checks, rollback, material ownership and settings preservation. Fault/recovery tests cover interrupted moves, committed cleanup, malformed inputs, directory collisions, locked files, disk-full staging, external changes, junctions and Unicode paths. Additional real-script fixtures cover loading artwork, unknown wrapper/asset/receipt paths, an in-use shared loader, reset-only idempotence and recovery blocked by a running client. The actual r42 updater passes custom-preference and missing-INI cases against dummy files. Representative setup and zoning previews are saved beside the evidence.

Retail shadow appearance, turning/approaching in Async1, and performance still need an in-game check. `tools/Install-Preview23-r42.cmd` installs only the tested renderer and the requested default-off value on F:, preserving other preferences and artwork. `INSTALL-RESULTS.json` records the actual installation state and backup location.

Installed and independently verified in `F:\AO_SInstall\Anarchy Online` on 2026-11-13. Renderer and receipt hashes match. All fourteen loading images, all other receipt fields and all other INI bytes are preserved. The backup location is in `INSTALL-RESULTS.json`. No game was launched, package built or release published.
