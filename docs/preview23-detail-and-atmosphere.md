# Preview 23 revision 30: terrain boundaries, distance and atmosphere test

This revision is prepared for retail visual testing, not release publication.

## Texture detail

The full inventory contains 13,401 colour maps covering 13,393 resource IDs.
Every one was checked against native dimensions; all 41,267 PNGs were verified.
All 13,401 material sets passed real x86 Vulkan decode/upload validation.

The earlier native-size audit did not catch small exports that equalled the
original game's dimensions. A second pass found 4,850 colour maps with a longest
edge of 256 pixels or less. Together with the selected 275744 material, the pass
updated 4,831 colour maps and retained 20 reviewed effect/coverage inputs exactly.

The work comprises:

- 138 full-source recoveries and two earlier individual renders restored at HD size.
- Three new individual renders: 32434, 32436 and 26859.
- 3,925 recoveries from the original, higher-resolution atlas cells, followed by
  pattern-preserving detail reconstruction.
- 763 other pattern-preserving reconstructions using existing rendered material
  microdetail. Added grain is bounded and suppressed at seams/control pixels.

Technical reconstructions preserve the existing structure; they are not thousands
of independent generative repaints. Normal, roughness, metalness and emission
masks retain their approved layouts. Original coverage remains sampled from the
game. Remaining unreplaced originals and model UV review flags are separate.

The renderer gives HD colour replacements their own filtered sampler. Legacy
MAXMIPLEVEL clamps continue to affect native textures, but no longer force an HD
replacement onto a blurry mip. Point-filtered inputs retain point filtering.
Frame captures now state resident albedo dimensions and the native mip clamp.
Loading-limit messages distinguish colour from auxiliary-map limits.

Revision 30 individually rerenders 1010004:20144, 19926 and 19924 at 2048 x 512
or 512 x 2048. Their previous 512 x 128 / 128 x 512 exports had little detail
across their narrow side. The runtime now budgets world colour by both area
(1,048,576 pixels) and longest edge (2,048), retaining a 2048 x 512 strip for the
same colour memory as the previous 1024-square allowance. Auxiliary-map limits
and low-address-space fallback remain bounded; low-memory jobs now explicitly
reduce their colour allowance too. No sampler clamp or FSR safeguards are removed.
Source images and full generation prompts are in preview23-selected-r30.json;
tools/bake-preview23-selected.py reproduces all nine selected maps exactly.

The import/export policy rejects unreviewed small colour exports and silent
family overrides of larger individual exports. Functional exceptions are hash
pinned. Imported validated maps must be retained from their verified library.

## Controls

In Ctrl+Shift+F12 → Main:

- **Instant Day/Night:** Server/cycle, Day (13.5), Night (0). These are shortcuts
  for the existing clock; manual `timeofday` commands remain authoritative.
- **Light source glow:** adjustable halos from authored emission masks/materials
  and ImmersiveShips navigation markings. This feeds depth-aware bloom without
  changing scene exposure or making ordinary white paint emit light. Existing
  general bloom and effect-glow controls remain available.
- **Ambient life:** named Off, Automatic (area and time), Fireflies, Dust,
  Spores, Spirit motes, Snow crystals, Embers and Ash choices, with adjustable
  density and brightness. Automatic uses local vegetation for daylight pollen
  and night fireflies, and warm dry ground for dust. Verified area identities
  select layered spores in the alien cavern and swamp, spirit motes in Scheol,
  snow in Penumbra, embers in Inferno and ash in Pandemonium. Gardens retain
  the gentler vegetation treatment. Unknown areas use local ground classification.
- **Area lighting (Enhanced Daylight):** restrained biome-specific sun, sky and
  ground irradiance. ICC has its own cool skylight and warm ground bounce.
  This changes surface illumination, not global image exposure. It follows
  Enhanced Daylight's day amount and excludes login, indoors and night.
- **Sky style:** Current/custom or Soft skies and starry nights. The latter
  uses our procedural atmosphere and richer stars/galaxy with gentler twinkle.
  Daytime sky colour preserves luminance; world exposure is unchanged. Existing
  sun/moon positions, weather and authoritative server/manual time remain intact.
- Expand **Far Scenery** for **Fuller distant scenery (loaded areas)** and its
  fullness/detail-saving controls. It keeps more large scenery groups within
  regions the engine already loaded. Balanced distant shading fades fine normal
  and parallax work from 160 to 320 metres; near detail stays intact. This is
  shading LOD, not mesh LOD, and does not load additional regions.

Ambient life is local and world anchored. The population is bounded to 81 emitter
anchors, with two particles per outdoor anchor or sixteen for the layered spore
field (at most 1,296 particles). It uses scene depth, fades at range, and adds
no dynamic lights or shadow casters. Paved surfaces receive no Auto population.
The login/creation playfield is excluded. Indoor areas are excluded unless the
known alien cavern or an explicit volume-style override applies. The effect is
optional, off in the base settings and enabled by Ultra. Other volume styles
use at most 972 particles in revision 30.

Revision 30 makes Automatic deliberately different by world and time: SL retains
visible daytime pollen or its biome's snow/spores/embers, with stronger night
activity. Rubi-Ka has a sparse daytime scattering and more fireflies at night near
vegetation. Default density is 1.33 (one third higher); automatic selection remains
bounded at full population at night. Manual styles retain their density control. Ordinary
fields submit at most 648 particles, themed fields 972, layered spores 1,296.
SL pollen/fireflies anchor above known terrain rather than placing half the
population below it, and skip hidden/covered or unavailable ground. Capture notes
now include the effective frequency and population, or a terrain suppression reason.

Immersive Ships now has an independent **Ship navigation lights** child control:
depth-tested red/green beacons and a double-flashing white beacon. Nearby ship
light strength controls real, short-range coloured illumination of other surfaces.
Up to 16 visible model groups are retained; only eight nearby light sources are
shaded, without additional shadow maps. Beam positions are inferred from drawn
model bounds, so individual ship placements still need retail visual review.
Motion includes both camera movement and the previous beacon positions. Old
sources expire after one missed frame and all are cleared on area changes.

## HD ground materials

Terrain Mode -> Shared Ground+ -> **HD Ground Textures** selects five new original
1024 x 1024 grass, dirt, gravel, sand and rock albedo masters. This replaces the
fine native colour grain with modern material colour rather than adding only a
detail filter. The original low-frequency palette, overlap alpha and automatic
blend ownership remain authoritative. World-space mirrored sampling removes
dependence on native tile rotation. The corrected cliff footprint and geometry
are untouched. Manufactured ICC artwork and asphalt retain their existing path.

The five maps are embedded in the renderer, including complete mip chains; no
new loose client files or downloads are required. Background decode keeps native
ground visible until all maps are ready. Upload is limited to one master per
frame, and toggling reuses the resident maps. Their mip chains add about 26.67 MiB
of GPU texture storage once enabled. Generation prompts, source hashes and runtime
hashes are recorded in rvk/assets/terrain/shared-hd-materials.json. The prior
masters and original artwork remain available for rollback.

HD Ground remains optional. Ultra enables the requested area/sky/ambience/glow
features and fuller scenery. The integrated F: test preserves the player's clock,
glow strength and other preferences while enabling the requested switches.

Ultra explicitly selects surface roughness 0.6, Far Scenery on at 3×, fuller
loaded scenery, Soft skies, light-source glow, Automatic ambient life, Area
Lighting, Improved Tiles and ICC Glossiness 0.7. The preset row now displays
Custom after modifying Ultra instead of mapping the custom index to Original.
The already accepted daylight profile, server clock behavior, cliff blending and
frame-generation safeguards remain in place.

## Terrain and distance corrections

Shared Ground+ no longer runs GroundOn's neighbouring-tile synthesis. Native
tile joints and painted transition/coverage masks remain intact; original
manufactured paving remains authored even when custom ICC artwork is disabled.
GroundOn retains its own blending option.

Cliff edge softness uses exact retail heights to feather the crest and toe on
the slope. It does not extend face projection onto flat ground, change vertices
or alter collision geometry. Neighbour heights distinguish a continuous long
slope from its ends, preventing a new band at every cell. The height lookup uses
one optional playfield texture; a 1536 x 768-cell area costs about 4.51 MiB.

The far camera previously multiplied view range by scenery range: 3 x 3 could
become 9 while fog used 3. Projection, broad-phase bounds and object culling now
share the maximum selected range. Loaded-area ring bounds remain in force.
This removes an inconsistent horizon reach; open-world streaming/pop-in and
model placement still require the owner's retail test.

## Reference-client inspection

The installed Lost Eden client includes HDRP assemblies, area-specific volume
settings, per-texture emission adjustments and named halo-light presets. Its
386 ambience configuration layers glowing spores, darker flecks and local haze.
Those design ideas informed the independently implemented local effects here.
Its camera/exposure numbers are not transferred into AO-VK's different lighting
scale. Ship-light metadata labels suggest a dedicated treatment of ship lights;
labels alone do not establish how its active rendering or distance LOD works.
No client binaries or reference artwork are bundled into this update.

## Validation limits

Controlled Vulkan cases cover HD/native mip selection, source-only halos,
ship-marking halos, terrain classification, login exclusion and existing lighting,
shadows, material sliders and settings persistence. Retail appearance across all
areas, Shadowlands biomes and multiple physical GPUs still needs testing.
File size and successful upload do not certify every texture's artistic quality.
The owner reported improved texture sharpness and liked the revision 28 ambience.
Revision 29 adds 54 controlled integration cases, existing lighting/shadow/mip
regressions, settings/restart checks and three repetitions of 13 performance
fixtures. Local reports are under build/preview23/lost-eden-r29. Synthetic timing
does not establish retail FPS; other AO clients were intentionally left running.
Publication remains on hold for the owner's combined visual test.

Revision 30 additionally checks dedicated beacons, flashing and nearby light,
cliff-mask monotonicity and softness, manufactured paving with HD Ground on/off,
SL daytime/night ambience, RK/SL frequency selection, real 2048-strip uploads,
square-map budgeting, new Ultra defaults and Custom reporting. Eleven changed
performance fixtures are measured three times; local reports and the incremental
artwork library are under build/preview23/visual-polish-r30. The earlier R28
complete-library file/upload audit remains historical evidence for unchanged maps.
