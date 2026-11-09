# Preview 23 revision 29: integrated terrain and atmosphere test

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
optional and off by default. Other volume styles use at most 648 particles.

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

The new area/sky/ground/fullness features remain opt-in defaults pending retail
review. The integrated F: test configuration enables them and Automatic ambient
life while preserving the player's clock, glow strength and other preferences.

Ultra explicitly selects surface roughness 0.6 and Far Scenery on at 3×.
The already accepted daylight profile, server clock behavior, cliff blending and
frame-generation safeguards remain in place.

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
