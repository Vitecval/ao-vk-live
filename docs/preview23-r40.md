# Preview23 revision 40 - faithful loading artwork, fades, shadows and sky traffic

Local update to r39. No published downloads or retail clients were changed by this session.

## Loading images

All 14 paintings are direct retouches of the matching original AO screenshots, found in the local AO screenshot folder. The original camera angles, scene compositions, buildings, kiosks, terrain and props remain recognizable and in place; treatment is restrained material cleanup, light painted polish and balanced exposure. These replace the earlier freely reimagined drafts. Floating names/UI/cursors are removed. Async2 is removed from scenery shots and retained in the final Pumpkin-Head combat image. The two ship reference images are excluded.

The set includes Frontier Bridge, The Ring, ICC Grid, Mission Agencies/Dreadloch, two armorer-shop lighting variants, Shadowlands Garden, the outpost, two Tir fortress views, the woodland grid road, Newland City, Old Athen and the Pumpkin-Head fight. Exact input/output hashes and source mapping are in `build/preview23/atmosphere-r40/ARTWORK-PROVENANCE.json`.

Initial login loading keeps Frontier Bridge. Subsequent zoning rotates the set. Zoning screen remains default-on in every preset and independently toggleable. The independent no-focus popup now uses a smooth 180 ms fade-in and 320 ms fade-out. Fade-out begins after the four-fresh-world-frame reveal condition. Background/minimized clients remain suppressed; history resets remain independent of the artwork setting.

The image list holds metadata for up to 32 images, with just one decoded at a time on the popup thread. This removes r39's total pixel budget, which would otherwise truncate the larger rotation. Existing images are released before decoding the next. The renderer does not retain all 14 decoded images in the classic client's address space.

## Pebbles

Shared Ground+ effective pebble density is now **0.15**, down from r39's 0.5 and the earlier 1.0. The actual native-ground render fixture generates **245 / 762 / 1512** stones at those densities respectively (zero at density 0): about **68% fewer than r39**. GroundOn's independent tuning is retained. The density change invalidates existing pebble blocks through the existing setter.

## Huge shadows

**Skip HUGE shadows** is the first child under **Sun shadows**, including its Quick Controls expansion. It defaults off. When on, large solid static casters with a huge world footprint or broad projected shadow are omitted from every sun cascade. Placed, visible and remembered shadow items share the filter; pieces sharing a real placement transform are assessed together. Identity-space meshes are not merged into a city-wide group. Ordinary objects, animated characters and foliage cutouts remain eligible. The geometry stays visible and the point-light shadow list remains intact.

The conservative rule requires at least 10 m height and a 1600 m² footprint with 40 m span, or at least 18 m height / 18 m span / 120 m² footprint with a projected rectangle of at least 2500 m². Thin tall poles and normal houses remain eligible. These are a geometric approximation of massive shadows, not a semantic building-ID catalog.

## Sky traffic

The added sky traffic now uses bevelled angular hull intersections with four variants: freighter, winged escort, compact shuttle and modular hauler. Belly keels, separate engine housings, panel seams, recessed vent patterns and restrained running lights replace the smooth flattened hulls. The existing route and sky-only integration remain; foreground pixels and ground illumination stay unaffected in the sky regression fixtures. Native AO ships remain their existing meshes.

## Verification

Frozen x86 renderer SHA256: `6e22ec282358cfaa3c7033cbf43e97b0b9cf03ceb3f8c5d5e204c60f24ba6fae`.

`build/preview23/atmosphere-r40/VERIFIED.json` records the matching evidence:

- `checks/RESULTS.json`: sky/ambient regression suite passed.
- `stability-checks/RESULTS.json`: horizon, precision, retail-sheet replacement and static TAA checks passed.
- `feature-checks/RESULTS.json`: native pebble counts, huge-shadow render comparison and ship fixture passed. The shadow option excludes 1 of 3 fixture casters and changes 25,753 image pixels; normal casters remain enabled.
- `ZONING-CHECK.json`: fade timing/reversal policy, four-frame reveal, timeout, no-focus hidden popup, 14-image loading/rotation and single-image memory checks passed. Artwork exposure checks passed for every image.
- The settings checker passes preset defaults, shadow-option registration, 0.15 effective density and restart checks.
- `INSTALLER-CHECK.json`: actual installer passed under Windows PowerShell 5.1 against a project-local dummy client, verifying all 14 artwork hashes, renderer hash and preserved unrelated settings/receipt entries.

Retail login/zoning fade timing, exclusive-fullscreen behavior, ship appearance in motion and subjective pebble coverage still require an in-game check. Fixture passes do not prove retail visuals or performance.

## Install

Close the F: game and launcher, then run `tools/Install-Preview23-r40.cmd` locally. It targets only `F:\AO_SInstall\Anarchy Online`, verifies the frozen checks and all artwork hashes, backs up existing files, installs the renderer/artwork, updates receipt hashes and enables the zoning screen. The huge-shadow toggle defaults off and is available under Sun shadows. Rollback restores replaced artwork and renderer/settings/receipt if installation fails.

F: remains outside this session's writable paths, so retail installation was not attempted. All `C:\Funcom` clients are untouched.
