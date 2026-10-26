# Modern artwork and material styles

## Continuous terrain and brighter daylight

In Cyberpunk mode, **Shared terrain materials** (`RVK_TerraBlend`, default 1)
uses five embedded masters: grass, dirt, gravel, sand and rock. Original AO
terrain textures supply palette and soft material weights; road shapes and biome
transitions stay in their original coordinates, while new detail samples world
coordinates and does not rotate or reset with the tiles. These are shared
surface materials, not new geometry or hundreds of new paintings. Colour-based
classification is approximate on unusual biome palettes. The original ground
must still be present; the renderer does not reconstruct hidden mask layers.

The original, combined live and native replacement ground paths support this.
Partial built-in natural/asphalt per-ID repaints are bypassed in both modes, so
zero restores coherent original ground instead of mismatched repainted tiles.
Reviewed ICC metal decks and plaza paving keep AO's original RGB palette and markings; authored normal/ORM maps retain relief and gloss. This avoids repaint colour drift with shared detail on or off. Reviewed asphalt and transition families
retain original masks with a satin dielectric sheen (roughness 0.32) on road pixels; their
dirt/gravel edges stay rough. Personal overrides retain priority.
Five mipmapped 512x512 images cost about 6.7 MiB once, and load
only in a playfield with sufficient process address space. Login art is excluded.
Existing Global terrain detail adds filtered normal relief alongside them.
Natural terrain keeps the existing rough, non-metal material response.

**Colour grading → Brighter daylight look** (`RVK_DayLook`, default 0) is
off by default, including Ultra. Try 0.6–0.8 to brighten shadows/midtones and
reduce warmth, golden-hour and night tint. HDR is required. This changes the
presentation without setting the game clock or weather. Highlight glare
reduction still follows it, and the interface is composed separately.

## Matched building families

Preview.10 restores the earlier premium ICC panel artwork and the earlier Newland/neutral window, wall and stripe artwork. Their original maps are preserved. Another 203 missing companion surfaces complete the selected building sets: ICC city, neutral midtech/Newland, Last Ditch, neutral backyard/tax office, neutral city structures and neutral ruins, alongside the existing clan, sandwall and Omni HQ/tower groups.

The inventory contains 381 available models across nine logical families and 679 approved resources. This includes reused artwork and material-only inputs, not 679 new paintings. The loader selects an explicit pack for each resource and checks its declared maps; an ORM-only material can retain AO colour. Families that share model parts switch together after their live maps are ready. Missing/corrupt/deferred inputs retain originals for that connected group. Personal overrides keep priority. Original functional signs, masks and cutouts remain.

Shared terrain stays at 1 on Ultra; daylight grading stays off. Decks/plaza paving preserve original colours with authored relief and gloss. Asphalt retains restrained non-metal sheen and original road/soil transitions.


`material-packs/building-families.json` records models, owners, shared dependencies, explicit providers and required maps. Generated companion edits are recorded in `restored-family-prompts.json`, alongside earlier family prompt sets. Other districts and live model fit remain pending.

## Global terrain and building passes

**Materials → Global terrain detail** (`RVK_TerrainHD`, 0..1) adds fine colour and
normal detail in world coordinates to actual terrain draws, including unnamed
tiles and the live combined lightmap path. It keeps the original biome colours,
transition boundaries and terrain mesh. Details fade before becoming subpixel,
and smooth triplanar blending covers steep faces. Missing authored materials get
a rough non-metal fallback. No per-tile images or CPU caches are generated.

**Global building materials** (`RVK_BuildingHD`, 0..1) provides 2,756 filename-inferred
profiles for architectural textures plus a rough non-metal fallback on other opaque
static/room geometry with normals. Metal, stone, painted metal, brick and timber
have distinct responses; dark pane regions in window-tagged textures get smooth
non-metal glazing and a restrained tint. It preserves original alpha. Explicit ORM
maps win. UI, identified character/equipment textures, additive/transparent effects
and terrain are excluded from the building fallback.

These are broad material/detail passes, not thousands of new paintings. Both
default off, need no per-texture image allocations, temporarily enable required
HDR/material lighting minima and honor master Enhancements. They remain independent
of the authored texture-style selector. Zero disables a global pass. The existing
reflection probe/screen-space approximation cannot show arbitrary unseen scenery;
no glass transmission, room geometry or ray tracing is added.

## Newland and shared world artwork

The optional `ao-cyber-world` pack adds **20 actual repaints**: 15 architecture
textures, including the picked twin-window facade and its related cladding/pipes,
plus 5 picked gravel/soil/rock/grass-transition tiles. Choose **World texture style
→ Cyberpunk**. Shared IDs change wherever AO uses them. Window panes, framing and
walls have authored distinct material regions. The original ground mesh remains;
leave experimental replacement ground disabled. Normal/ORM maps use reduced
resolution to limit memory; one full-quality set of the 63 maps is about 91 MB on
the GPU, before extra AO quality copies. The loader's existing memory safeguards
still apply. Source artwork, exact prompts and the bake are in the source package.
Final live model fit and neighbouring terrain transitions remain preview checks.

## Character artwork preview

The optional `ao-cyber-characters` pack provides **six actual repaints**: scout
torso, arms, legs, boots and helmet, plus the picked back/shoulder armour. Metal
plates, carbon weave and amber inlays have separate colour, normal, ORM and
emissive maps. This is new artwork on the original models, not a geometry change.
The pack follows **Character & equipment materials**, with strength above zero
and enhancements on. It refreshes loaded textures without restarting. Strength
blends inferred materials below; for authored art, zero disables the pack and
positive values enable its authored maps without crossfading the painting.

Original alpha is sampled from the game's texture at runtime because some armour
stores glow masks in it. Replacement RGB does not overwrite coverage/glow data;
original pixels are not distributed. Distinct secondary texture stages that
cannot share the original-alpha binding keep their original artwork. Atlas
layouts were reviewed; model fit still needs live confirmation.

## Character and equipment foundation

**Materials → Character & equipment materials** (`RVK_AutoMat`, off by default)
adds inferred material defaults to 3,661 identified character/equipment texture
IDs. Skin and fabric stay non-metallic; leather, polymers and selected metal
equipment receive distinct roughness/metallic responses. Of these, 1,718 unresolved
character surfaces use a conservative non-metal baseline. It is a first material
pass, not newly painted artwork or a complete character remaster.

**Material response strength** (`RVK_MatStrength`) blends the response; zero
disables it. Authored material maps take priority, including the existing ICC
pack. Interface panels, additive effects and transparent passes are excluded.
The catalog is embedded metadata with no additional material images or preloads.
Only required HDR/per-pixel/PBR lighting minima are applied; saved settings resume
when disabled, and screen-space reflections are not forced. Master Enhancements
also temporarily disables it.

Dynamic character skin/armour composites without an identified source texture
remain unchanged. Broader custom artwork and composite masks remain in progress.
See `material-packs/ao-character-materials/README.md` for catalog
provenance and the reproducible generator.

## ICC artwork packs

Uploaded database textures retain cold CPU copies in losslessly compressed form
when worthwhile. Later CPU reads/copies restore exact original pixels. This
reduces pressure without shrinking GPU textures. Palettes, unidentified dynamic
surfaces and locked/not-yet-uploaded textures remain raw. DirectDraw/Direct3D
HRESULT methods translate allocation failure into E_OUTOFMEMORY. A log counter
reports the current memory saved; `RANDYVK_SHADOW_CACHE=0` is a diagnostic bypass.

The classic live client has a 2 GB process address-space limit. Decoded material
maps use a 24 MB CPU cache, released when address space is low, with one decode
in flight while prior uploads drain. GPU map resolution is unchanged. If a new
optional material cannot fit, its existing maps remain; changing the style later
retries it. Replacement ground disabled also skips its whole-playfield texture
preload. Grass over the game's original terrain uses captured visible draws.

Open **Ctrl+Shift+F12 → Materials → World texture style**:

| Selection | Saved value | Result |
|---|---|---|
| Original | `RVK_ModernTex=0` | Original game materials or your personal overrides; default |
| Classic HD | `RVK_ModernTex=1` | The earlier restrained six-surface architectural preview |
| Cyberpunk | `RVK_ModernTex=2` | Brushed titanium/graphite, selected light inlays, updated city ground and broader physical material coverage |

The Cyberpunk ICC component has **127 material assignments**, including the shared pipe, bin, lamp,
statue and bark textures identified in live captures. **18 surfaces receive new
albedo and normal maps**, from twelve source images. Other assignments retain the
original artwork and change material response. This is not 127 redrawn textures
or a complete remaster of the playfield. Tree canopy art and geometry are unchanged.

Cyberpunk temporarily enables HDR, per-pixel/PBR/albedo/normal lighting and
screen-space reflections. Its effective minima are reflections 0.8, reflection
gloss 0.6 and PBR highlights/environment/screen reflections/probe 1. Stored
preferences remain unchanged and resume with Original or Classic HD. The higher
reflection quality costs more GPU time. Rain controls wetness; weather is not forced.

Style changes refresh loaded surfaces in the background; allow a few seconds to
load. No zoning or restart is required to switch. Master Enhancements off temporarily
disables the style and retains the selection. Personal materials and live-workbench
overrides keep priority. Shared resource IDs also change in other zones while selected.

Replacement ground remains optional/off by default. Preview.6 repairs its known
live combined-pass omission and preserves unsupported original draws; live review
remains necessary. The original combined ground
draw now reads its normal/PBR maps from the actual stage-1 base texture and adds
highlights once. Normal maps and reflections work without replacing the ground mesh.

**Effects → Highlight glare reduction** (`RVK_SoftHi`, default 0.75) softens bright
world surfaces after grading, including pale stone/plaster lit by direct sunlight.
It preserves dark tones, sky, interface and additive lamps. Zero restores the
previous highlight response. Known natural rock/lava surfaces are also forced
rough and non-metallic, while white temple stone uses a honed response. Neither
change requires dimming the whole scene or removing real metal/glass reflections.

Install/Update includes both ICC packs, character art and world art under `randy-vk/packs`. Hash checks and the
installation record support reversible updates and uninstall; other packs, original
game files and settings remain intact. Close this AO installation to replace files.

Maintainers: each `material-packs/ao-*-icc` folder includes a README, manifest,
source artwork and generation prompts. `tools/bake-modern-icc.py` and
`tools/bake-cyber-icc.py` reproduce the maps with Pillow and NumPy, without the game
database. The cyber catalog separates material assignments from new-art assignments.

The retail-renderer harness exercises both styles, original restoration, background
loading cancellation, master Enhancements, ground maps and texture-quality variants.
Headless GPU checks exercise live's combined ground path. Live appearance and UV
alignment still need comparison on the actual scene; screen-space reflections cannot
show unseen objects and the fallback probe is an approximation.
