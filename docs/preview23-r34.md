# Preview23 revision 34 — local testing update

This update is intended for the F: test installation. The published Preview23 revision 33 downloads are unchanged.

## Changes to try

- HD materials now reload after leaving and returning to an area. This repairs a shared cache fault that could leave an installed HD texture drawing at the game's original resolution.
- Ultra enables HD Ground Textures and selects Ambient Life brightness 1. The new brightness 1 equals the previous 1.5. Existing saved brightness values migrate once, preserving their appearance.
- Automatic ambient life appears more often during daylight, with sparse dust on paved city ground. Shadowlands woodland daylight uses pollen and seeds; night shifts towards insects. Other regional profiles retain spores, spirit motes, snow, ash and embers.
- Individual particles vary between rounded motes, slender seeds, flakes and occasional soft wisps, with different drift, curl, flutter and insect paths. Density still follows habitat patches rather than covering every surface uniformly. Login and character creation remain excluded.
- These changes affect particle scattering and movement, not world exposure. No global whitening filter was added.

## Why the crates and 6073 looked blurry

The latest F: captures show crate texture 1010004:33201 and corrugated wall 1010004:6073. Both already have valid HD diffuse maps: 256×512 and 512×512 respectively, compared with native 64×128 and 128×128. New artwork would not fix the observed failure.

After GPU-map eviction, the shared material table could still find the same surface as an owner. It incorrectly treated that pending owner as an uploaded source and skipped loading its HD maps indefinitely. Sharing now accepts only ready GPU material owners, so an evicted surface can reload. This applies to all material IDs.

A second problem increased unnecessary eviction: pressure on a small 214 MB device-local aperture was counted even while the main 8 GB graphics heap had several gigabytes free. Image-cache pressure now follows the largest local heap, including a single-heap integrated GPU. Genuine pressure still retires cold materials; active-material protection remains.

The production DirectDraw proxy regression reproduces both failures using actual native surfaces and the full material loader. Before the fix, each returning surface stayed at native detail after one upload and one eviction. After the fix, both upload again and their returned images match the original HD images exactly (mean pixel difference 0). Tests deliberately impose a 1 MB cache cap; no system-wide memory exhaustion is used.

## Validation and limits

The x86 renderer, cache warmer, demo and relevant checks build successfully. Settings checks cover Ultra defaults; three saved-brightness values survive migration and a second process restart without migrating twice. Render-budget checks cover the captured discrete GPU heap layout, real pressure and a single local heap.

Twelve headless Vulkan ambience profiles cover Rubi-Ka and Shadowlands day/night, woodland, snow, ash, embers, spirit effects, cavern, off and login suppression. Final paved-city and Shadowlands daylight on/off images confirm local particle changes without changing scene exposure. The city fixture includes native terrain support, since paved ground can have no retained grass tile. Placement remains bounded to the existing emitter array and one particle batch. No new textures, lights or shadow casters are needed. No Vulkan validation errors or device loss were observed in these checks.

Retail movement, daytime visibility and visual density still need the user's F: playtest. These checks do not establish a retail FPS improvement or prove compatibility with every GPU. Frame-generation synchronization and presentation were not changed.

The library still covers 14,317 of 14,753 indexed surface IDs (97.04%); 436 remain original-only. Some are functional masks or effects, but visible unresolved art also exists, including glasses 204888 and 204892. This update does not claim a 100% artwork overhaul.

## Performance audit and next useful improvements

The captured F: samples identify grass/terrain setup, interface drawing and water as more substantial costs than ambience. In the two available GPU windows, grass costs 6.40/2.17 ms, interface 4.43/7.54 ms and water 2.79/0.30 ms; ambient life rounds to 0.01/0.00 ms. Terrain/grass draw setup reaches 2.51 ms on the CPU. These are scene-specific snapshots, not universal timings.

1. Cache grass visibility and placement work across unchanged views; reduce repeated terrain setup before lowering visual density.
2. Batch consecutive compatible interface draws while preserving order and clipping. The large interface cost deserves measurement before changing distant scenery.
3. Profile water passes by screen coverage and skip unused work. Keep transparency and shore seams intact.
4. Coordinate far-object and terrain visibility/LOD to avoid distant objects appearing unsupported. Raising object range alone cannot provide missing terrain or unloaded world data.

The local Lost Eden reference contains readable per-texture glow/material adjustments, 15 named light presets, and regional spore/lighting settings. Useful next experiments are selective emissive-material tuning and decorative colored light pools with shadows disabled, reserving shadow work for important lights. Unity binaries and assets were not transplanted. The new ambient shapes and paths are original procedural code inspired by the reference's regional variety.

Detailed captures, recovery images, migration results and build logs are retained under `build/preview23/atmosphere-r34`.
