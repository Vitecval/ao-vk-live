# Preview 23 revision 44 — shadow cost and broad texture preparation

This local trial is installed on F:. Packaging and publishing have not been performed. The user approved reducing sun-shadow distance from 400 m to 200 m. Resolution, softness, grass, water, artwork and all unrelated preferences are preserved.

## What the r43 run established

At one logged checkpoint the material worker had 757 prepared-cache hits and 2,048 PNG fallbacks. The missed examples 200348, 14933, 14960, 200346, 9236 and 204188 were absent from all three ICC captures used to prepare r43. That trial accelerated the captured views but did not cover the wider zone library. A single-material benchmark was insufficient evidence for complete zoning latency.

The recent r43 GPU log includes a 30.68 ms sun-shadow window in a 60.78 ms total GPU frame, alongside other views with much smaller shadow costs. Grass and water remain substantial in several views. The data are 600-frame averages with other AO clients present, not isolated before/after measurements at a fixed retail camera.

The r42 stability work had removed the earlier alternating refresh of the two widest shadow cascades. r43 still refreshed all four every frame unless its exact static-depth signature repeated. Camera motion prevents that reuse, making the distant geometry expensive again.

## Shadows

The closest layers update every frame; the two distant layers take turns. Each skipped layer retains its own depth image, matrix and coverage. The stable camera-centered coverage introduced for turning remains intact. Initial validity, cascade-count changes, significant light-direction changes, sun/moon changes and changed huge-shadow exclusions refresh all layers together. The existing static-depth optimization remains available. `RANDYVK_SHADOW_FULL_REFRESH=1` is the fixture comparison control.

The user-selected 200 m setting is saved as the sole preference change. It reduces the spatial range of retained/extended shadow scenery while preserving the selected 2048 shadow-map resolution. Nearby rendering options are otherwise unchanged.

The actual-renderer turn-away, approach, huge-exclusion, mixed-depth and moving-scenery regressions pass. In the 4,096-cube moving-camera fixture, with static-depth reuse disabled, sun-shadow GPU time was 0.99 ms for full refresh at 400 m, 0.58 ms for the new cadence at 400 m, and 0.51 ms for the new cadence at 200 m. The frame dump confirms four layers updated in the full-refresh control and three in the cadence cases. This is renderer-fixture evidence, not a retail ICC FPS claim.

## Textures

Format 4 stores losslessly compressed mip chains once per source set. Smaller normal/albedo limits, including the existing low-address-space 256-pixel mode, reuse the appropriate already-filtered levels. Alpha-preservation policy is applied on read. The key still includes the source paths, lengths/write times and algorithm version. Header dimensions, encoded sizes, total bounds and the header-plus-pixel checksum are validated. Individual decompression outputs are bounded to their expected level size.

A representative material uses 702,660 stored payload bytes for 8,388,604 decoded bytes. Cache tests verify exact mip pixels against fresh PNG processing, smaller-resolution and alpha-policy reuse, rejection of insufficient prepared quality, source invalidation and corrupted/truncated/oversized entries. The legacy v3 reader is retained and was tested against an actual previously installed cache entry, so the existing ICC preparation remains useful while the broader library arrives. Compression affects disk storage; GPU texture format and image quality do not change.

The single worker now permits up to 32 completed small sets while keeping the same 12 MiB decoded look-ahead threshold. Previously, four completed sets could make progress depend on a low frame rate despite available byte headroom. The existing per-drain upload budget is unchanged. Texture-registration calls can drain completed uploads only between frames, allowing preparation to progress during AO's load instead of relying solely on visible world-frame presentations. GPU uploads still use the game's thread and the existing bounded render/upload queues.

The offline preparer now scans the installed material providers across zones, rather than selecting only IDs seen in screenshots. It follows the runtime eligibility rules for reviewed ground, complete world materials and managed building resources, and verifies each newly prepared entry by reading it back. Two disjoint offline shards prepare files; this does not add game-time decode workers.

## Installation and cache delivery

The renderer and requested setting were installed and independently verified while F: was closed. A backup is recorded in `build/preview23/atmosphere-r44/INSTALL-RESULTS.json`. The initial renderer-only transaction preserves the existing v3 cache, receipt artwork and every unrelated INI byte. The graphics library was unchanged after its GPU regression runs; the final renderer adds the separately tested read-only v3 cache fallback.

`tools/deliver-material-cache.py` publishes completed v4 files individually through temporary files and an atomic Windows rename. It never replaces an existing different file and never changes the renderer, preferences, artwork or PNG sources. Previously delivered entries are tracked in `CACHE-DELIVERY.json`. Delivery can continue while AO runs and requires no further restart. Preparation completion, final source manifest and independent installed-file verification are recorded separately; an installed renderer does not itself establish that the whole cache has arrived.

Completed on 2026-11-13: 14,293 eligible source sets were prepared or reused and read-back checked with zero failures. All 14,345 published cache entries (16,881,268,491 bytes, including earlier prepared extras) were independently SHA-256 verified against both source and installed copies. The full-library manifest, delivery ledger and final verification are recorded in the r44 evidence folder. All cache delivery is complete; the F: renderer is installed with the requested 200 m setting. Retail zoning time and ICC FPS still require an in-game comparison. No package was built or release published.
