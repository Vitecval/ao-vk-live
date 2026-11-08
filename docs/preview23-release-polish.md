# Preview23 Ultra, texture fidelity and continuous terrain

Ultra selects ICC Glossiness 0.5, AnarchyHD world artwork with HD replacement
colour enabled, low Building Weathering, Enhanced Daylights, and Global Shiny
0.5. Clouds default off in every quality preset. Other quality presets retain
the independent world-artwork choices. Enhanced Daylights follows server time
by default. An explicit console/INI hour or speed overrides the automatic
clock while retaining its lighting. `timeofday 14` selects hour 14;
`timeofday -1` with `timespeed 1` restores the server/cycle clock. Manual
settings persist like other console settings. Applying Ultra resets the clock
to its automatic defaults.

Global Shiny scales material highlights/environment/scene reflections and
raises the roughness floor as it is reduced. It reuses an existing frame-block
spare component without changing descriptor or uniform layouts. Original mode
keeps the original specular response.

The resolution audit reads the full-quality texture records directly from the
owner's database and compares every installed diffuse PNG header. Its initial
result is 13,401 files / 13,393 unique IDs, 563 undersized diffuse replacements,
and seven exact aspect mismatches (one is a small rounding mismatch). No database sources remain unresolved. Two legacy
character records use a flag in place of JPEG marker bytes; only those marker
bytes are reconstructed in memory for decoding. The database is never edited.

297003 has a 256-square replacement for a 1024-square original. An older family
export created 256-pixel source crops regardless of native atlas size. The
renderer now retains the already resident original colour when an identified
world/character/icon atlas replacement is smaller in either dimension; matching
material maps can still contribute. This is a fidelity fallback, not a claim
that the 563 artwork repairs are complete. New renders/export results must be
checked against the original dimensions and aspect before promotion.

`tools/check-material-resolution.py` checks every staged diffuse PNG against
the dimension-only native record ledger. Empty inputs, unknown source IDs,
undersized exports, and incorrect atlas aspect fail the check. The world-art
baker also checks each newly baked catalog entry and source resolution before
resizing; legacy entries retained by `--only-new` remain visible to the full
pack check and are not silently certified. The ledger includes no game pixels.

When Shared Ground/SharedGround+ or GroundOn owns terrain, independent per-tile
replacement paintings no longer override it. Only the verified ICC tiles with
Improved Tiles enabled opt into their custom artwork. Original terrain inputs
remain available to the continuous renderer as biome/road masks. Edge blending
is automatic for native terrain, including opposite borders of a repeated
nonseamless tile; explicit ICC paving keeps its designed joints. Both native
terrain modes use retail triangle diagonals to meet collision and overlays.

Immersive Ships enhances existing identified spacecraft markings and clears
some haze on the named distant sky ships. It adds no traffic actors, geometry,
point lights or shadow cubes. Large scenery spheres (radius at least 16 world
units) may reach 15% farther while enabled; small-object distance is unchanged.
The projection and broad-phase sphere cover that bounded range. Additional
visible buildings can still cost time: this is not a zero-cost distance claim
or a measured guarantee for every city/GPU. It remains switchable.

Controls cover sharp 1024 original versus undersized 256 albedo, the Global
Shiny response, the ship-marking response, settings persistence/Ultra defaults,
and flat/cliff terrain with conflicting authored maps present or absent.
Retail terrain boundaries and ship-distance performance still require scene
inspection. Frame-generation and presentation logic are unchanged.

The full smaller-than-native audit now has 563 corrected exports: 561 world
and two character textures. All 563 pass the size/aspect gate and native
x86/Vulkan map-loading checks with the revision 22 renderer. This is a
technical restoration to native resolution, not 563 independent generative
repaints. Twenty-four sets use individual or recovered rendered detail;
536 use shared rendered material microdetail. Full-quality originals provide
the exact native patterns, image structure, colors and alpha. Grain is bounded
to seven byte values, fades at borders/high-contrast artwork edges, and does
not replace native image detail. Approved normal/height and ORM masks are
retained instead of assigning generic metalness to every material.

Two glass materials (217923 and 296246) receive no added grain. The new
297003 image-service request remains blocked; its unchanged full-quality
1024-square original is retained in place of the old 256-square replacement.
No fresh painting is claimed for these three. There are zero remaining
undersized exports in the audited set. The seven exact aspect mismatches
outside this smaller-than-native set remain flagged for separate review;
image aspect alone does not establish incorrect normalized UV placement.
This pass does not complete the separate original-only artwork backlog or
certify all retail uses of the textures.

Both world and character source bakers now reject new undersized exports and
undersized source art. Corrected catalog entries are marked as verified imports
and retained by `--only-new`; they cannot be accidentally rebuilt from their
old small crops. `tools/bake-native-resolution-repairs.py --audit-dir <audit>
--seed-packs <verified-seed-packs>` reproduces the technical export using
Pillow/NumPy and the preserved local audit inputs/generated material masters.
Runtime auxiliary-map, staging and address-space budgets remain bounded.
The largest original atlases can retain their resident original color when
budgeted replacement mips would otherwise reduce detail.

The incremental packaging library for these corrections is
`build/preview23/resolution-audit/runtime`; it contains complete updated pack
manifests and only changed maps. Use the revision 17 full/lite folders as the
seed for unchanged assets. The generated images, original-size audit, prompts,
export hashes and native test receipt remain in the resolution-audit folder.

## Cliff toe correction (revision 22)

The owner confirmed the sand patchwork blends after revision 21, but the cliff
texture still stretched onto the flat floor before the slope. A native render
fixture reproduces this: central-difference vertex normals lean across the
neighboring flat cell, and the old material selector used only that smooth
normal. Projecting a vertical texture onto a horizontal plane collapses its
vertical derivative into streaks.

Cliff projection now requires geometric slope support. Flat floor receives
zero face projection, while genuinely steep faces retain full side mapping
even when their corner normals have been averaged with horizontal neighbors.
The controlled footprint test changes 9,723 falsely projected floor pixels
to zero, in both SharedGround+ and GroundOn. The steep-face diagnostic remains
fully projected. Terrain geometry, collision, lighting, and draw ownership are
unchanged by this correction. The owner confirmed the cliff sides are fixed.

## Outdoor light balance and loaded scenery (revision 25)

The owner confirmed revision 24's `timeofday` priority correction. The clock
remains independent of Enhanced Daylights: server time by default, explicit
manual hours when requested. The newer comparison uses 13.5 in both clients.

The ICC capture (`rvk-frame-153015.txt`) supplies global ambient 0x7e and
neutral .55 sunlight. The earlier daylight sky-irradiance floor was .16 on a
vertical face, so it contributed nothing at that ambient level. A bounded
side-facing sky lobe and small warm ground-bounce term now illuminate the
material, including its rough environment reflection. The bounce follows the
zone's ambient energy, surface orientation and storm attenuation; it does not
change exposure, gamma, grading, sky brightness or texture colors. Upward-facing
ground receives no additional lobe. Existing material occlusion remains, and
near-black materials retain their dark response. This is an analytic outdoor
approximation, not a claim of Unity-style geometry-aware global illumination.

In Enhanced daylight, screen-space ambient occlusion now removes the ambient
share instead of also darkening the known direct-sun share. The latter was
already recorded for contact shadows. Actual contact/cascade shadows remain
separate and active. The same composition is used before depth-of-field blur.
The profile's existing outdoor/daytime gating still excludes login studios,
interiors, night, and other day-cycle modes. No frame-generation code changed.

Immersive Ships previously extended the native camera's reach for large
scenery, but N3's earlier statel rings could still hide it. Its existing 15%
extension now also reaches the outer building/tree rings, capped inside the
zones N3 already keeps loaded. Inner rock/shrub detail rings, actors, occlusion
culling, and the amount of loaded world remain unchanged. Turning it off restores
the former ring policy. An increase in distant geometry may cost frame time;
the controlled checks do not establish live retail performance or an exact
Lost Eden scene match. Visibility changes take effect on the game's next ring
refresh when the camera moves onto another tile.

## Optional Far Scenery (revision 26)

Far Scenery is an independent toggle in Main and General, with a child distance
slider from 1 to 3 (default 2.5). It defaults off and presets preserve the user's
selection. Buildings and trees use up to three times the native statel-ring
distances, with rocks/shrubs limited to twice their existing detail range. All
rings remain capped inside N3's already-loaded zones; the loaded playfield and
server actor range are not expanded. The ring change appears on the next camera
tile refresh. It draws the existing meshes: no simplified-model or batching
system is claimed, and the additional objects can cost CPU/GPU time and memory.

The effective ordinary camera/fog range rises to 3 while enabled so it does not
hide the extended small props. The large-scenery camera reach also follows the
selected multiplier. The saved ordinary view preference is preserved and
restored when disabled. Immersive Ships keeps its independent 15% extension.
Switching all enhancements off disables Far Scenery without losing preferences.
Terrain full-quality radius, tessellation, shadow distances, density, artwork,
daylight and frame-generation behavior are unchanged. The owner's F: test setup
selects Far Scenery on at 3; release defaults remain opt-in.
