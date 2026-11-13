# Preview 23 revision 43 — prepared textures and ICC shadows

Local trial only. The zoning fade, worker count, texture quality settings and other preferences are unchanged. Packaging and publishing have not been requested or performed.

## Prepared textures

`ao-vk-prepare-materials` processes the installed side-loaded PNG material sets before AO launches. The cache stores the exact filtered mip chains, including the normal/roughness coupling, height and alpha policy. The client reads these on the existing single worker and uses the existing upload and memory budgets. Missing, stale, damaged or unsupported entries fall back to PNG preparation. The client does not create new cache files while playing.

The cache key includes the normalized source paths, source lengths/write times, decode limits, alpha policy and format/algorithm version. Reads validate the entry size, dimensions, level count and payload checksum before publication. The cache is disposable; PNG source artwork remains authoritative. A change to the decode/filtering algorithm requires a version bump in `material_cache.h`.

For this first trial, 1,080 material variants were prepared for the IDs referenced across the three ICC captures at 14:54:37, 14:55:45 and 14:55:48 on November 13. They cover the installed providers and the normal address-space quality alternatives, using about 2.48 GiB on disk. This is targeted ICC coverage, not a claim that every zone is fully prepared. Other textures continue through the existing PNG path. The preparer also supports the entire installed library when run without `--capture`; multiple `--capture` arguments form a union.

A representative material averaged 69.4 ms for PNG preparation and 10.4 ms for a prepared read over five warm filesystem samples. Pixel chains were compared exactly. This is a material preparation comparison, not a measured reduction in a complete retail zoning transition. Tests also cover source/quality/alpha invalidation and corrupt, oversized and truncated entries.

## ICC performance evidence

The three saved frame dumps report 16,790–17,201 shadow items and 24,317–25,520 item/cascade submissions. The last dump contains 16,550 static items and 166 animated items. The caster arena is 616 MiB. The live log reaches the 16,384-entry retained-caster limit and refuses additional entries.

Recorded 600-frame averages include 42.47 ms of sun shadows in a 74.38 ms GPU frame. Later samples reach about 83–84 ms of sun shadows in a 129–130 ms frame; grass and water each cost about 10 ms there. The ordinary scene is another significant cost. Smoke, ambient life and ship navigation lights are much smaller in these samples. These averages are not timings for the exact captured frame, and five other C: AO clients were running. They establish the dominant reported cost, not an isolated retail benchmark or a precise allocation of multi-client contention.

The screenshots show a character shadow present in one orientation and absent in another. They do not independently establish which internal path caused the change. The changes below address reproduced renderer failure paths; the same ICC camera-turn check remains necessary after restarting with r43.

## Shadow changes

- Large opaque scenery could bypass promotion on its first observed frame at each new heading key. A rotating object then accumulated old retained poses until the cache filled. Movement is now observed by visual, geometry and approximate placement before that pose-dependent key. Movement retires the preceding retained pose, and continuing movement stays on the live draw path. The observation table is bounded and is cleared on playfield changes.
- World geometry sharing the same view and lens can cast shadows when its near/far clipping differs. The receiving path accepts this variation only against a captured current world view; unrelated view cameras remain separate.
- Retained opaque scenery gets a reusable depth layer. A layer is reused only when its exact light matrix, eligible caster set, placements and arena locations match. A new layer is built only after that signature repeats, so continuous movement uses the ordinary pass without an extra depth copy. Its depth is copied into the ordinary cascade before live geometry and foliage are drawn. Matrix, geometry, exclusion, playfield and resolution changes invalidate the relevant data. Current actors and foliage continue to update every frame; no cascade staggering or quality reduction was introduced.
- The extra depth allocation is limited to 64 MiB at shadow resolutions up to 2048. Higher resolutions and allocation failure use the full draw path. `RANDYVK_STATIC_SHADOW_CACHE=0` is a comparison control. Memory diagnostics include this allocation.
- Current-frame casters outside the widest light-space square are culled using the same bound as retained casters; they cannot affect a texel in that map.

The rotating-scene fixture ends with one live caster and zero retained old poses. World-depth variants produce identical final pixels. Cached and uncached dense-scenery fixtures also compare pixel-identically. The first controlled 1,025-caster comparison measured sun-shadow GPU time at 0.41 ms without depth reuse and 0.32 ms with reuse. This demonstrates the path, not an ICC speedup claim. The retained-scene turn-away, approach and huge-filter regressions also pass.

## Trial installation

`tools/Install-Preview23-r43.cmd` installs the tested local renderer and the prepared ICC entries into the F: test installation. It requires that installation and its launcher to be closed, leaves the C: clients alone, verifies the prepared payload, backs up the renderer/receipt/INI, and changes the renderer and receipt transactionally. Every INI byte and all artwork are preserved. Cache publication is additive and precedes the renderer update; an interruption can leave valid disposable entries while retaining the previous renderer. Existing differing cache entries are preserved and reported.

Builds, manifests, saved capture evidence and validation results are under `build/preview23/atmosphere-r43`. The `INSTALL-RESULTS.json` file, when present at that level, records an actual installation; the separately named fixture record does not establish one. Computer control was stopped following the user's Escape input; subsequent work used source files, saved captures and standalone fixtures.

Installed and independently hash-verified in `F:\AO_SInstall\Anarchy Online` on 2026-11-13. All 1,080 prepared variants match the manifest; all fourteen loading images, every INI byte and every unrelated receipt field are preserved. The backup location is in `INSTALL-RESULTS.json`. No game was launched and retail zoning/turning performance still requires an in-game check.
