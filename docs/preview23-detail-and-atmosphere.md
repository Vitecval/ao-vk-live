# Preview 23 revision 28: local detail and atmosphere test

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
- **Ambient life:** adjustable density and brightness. Auto uses vegetation for
  daylight pollen/night fireflies, warm dry ground for dust, and the known 386
  cavern profile for glowing spores with darker drifting flecks. Style overrides
  offer Fireflies, Dust and Spores for unusual areas, including Shadowlands.

Ambient life is local and world anchored. The population is bounded to 81 emitter
anchors, with two particles per outdoor anchor or sixteen for the layered spore
field (at most 1,296 particles). It uses scene depth, fades at range, and adds
no dynamic lights or shadow casters. Paved surfaces receive no Auto population.
The login/creation playfield is excluded. Indoor areas are excluded unless the
spore profile applies. The effect is optional and off by default.

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
