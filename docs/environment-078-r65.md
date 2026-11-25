# Private 0.7.8 environment follow-up

The owner supplied Penumbra/Inferno screenshots and requested a bright, readable
regional art direction without a whole-image exposure lift. This work remains
private; it does not publish a release or claim retail playtesting.

## Renderer

- Inferno's captured playfield 4005 now shares the volcanic profile with 4605.
- Regional lightmaps separate diffuse sky/bounce energy from direct sunlight
  before applying sun visibility. Shadows remain, without removing open-sky light.
- Penumbra and Inferno Soft skies use brighter sky radiance, white cloud detail
  and a more open automatic cloud climate. Explicit weather selections still win.
- Natural snow joins blend even in Shared Ground+. Continuous mirrored powder
  sampling removes another repeated-edge discontinuity; paving is excluded.
- A shallow world-anchored tessellated powder shell adds centimetres of surface
  relief near the camera, with smooth distance fade and matching material passes.
- The owner's 01:12:41/01:12:48 Penumbra captures exposed Shared Terrain+'s
  coarse original submissions versus Enhanced Ground's persistent height-field
  patches. Snow regions now share those patches and cliff rounding in Shared
  Terrain+, with authored HD tile maps and classic lightmap lighting. Other
  regions retain original cutouts/overhangs. Geometry controls reach the snow
  patch path even when GroundOn is off; installed ground albedo selection is
  independent of the optional ICC tile style.
- Inferno's captured ambient 0xA23232 and four orange directional fills are
  neutralized at their original luminance, under regional lighting. Reflected
  ground/sky light is neutral grey; actual local lava/lamp colour remains warm.
  Lava emission is reduced 18%, bloom contribution another 25%, and local lava
  light red intensity reduced with a softer colour balance.
- The later zone566 captures confirmed HD masters loaded but legacy ground
  artwork still dominated the surface. Full HD colour now uses original broad
  palette/transition masks rather than retaining native fine grain. Reviewed
  private/personal paintings carry an explicit metadata flag; large legacy
  restoration files no longer bypass the shared HD treatment merely by size.
  Authored paving retains its layout. Classic ground shading normals come from
  the native height field, matching Enhanced Ground's cliff coverage without
  moving submitted vertices/cutouts. F9 now reports native tile IDs, selected
  dimensions and independent-painting metadata for residency review.
- The owner's 12:09/12:12 zone566 captures then exposed the remaining split:
  independently painted natural transition tiles bypassed the continuous HD
  material field, while old/missing maps followed another path. Natural Shared+
  HD now always uses world-space HD material detail. Only the original coarse
  transition shapes/palette (32 texels per cell, independent of source dimensions)
  are retained and blended symmetrically across neighbouring cells. No old fine
  grain is displayed and individual-map residency cannot change this output.
  Original per-cell alpha remains exact. Manufactured paving/carved floor layouts
  retain their joints and physical ice retains reviewed HD facets/frost response.
  F9 distinguishes the mask source from the final colour route.
- Steep faces retain side projection even with upward-leaning coarse normals;
  Shared Terrain+'s soft crest/toe blend and original geometry are preserved.
- Soft screen/glow cards require genuine hard-alpha boundaries before any leaf
  lighting or foliage distance LOD. The captured Dreadloch screen overlays had
  none, yet previously entered those paths. Advert artwork/depth/blending stays
  intact; see `docs/billboard-alpha-classification-r65.md` for the scoped fix and
  the remaining retail flicker check. Rocks and Scree defaults to 0.5.
- Inferno liquid uses an authored molten surface and emission rather than water
  reflection/foam. Up to eight stable surface samples feed the existing local lamp
  grid, with at most four lava shadow cubes within the existing budget. Triangle clipping prevents lamps on dry islands;
  samples are cached per liquid mesh/camera cell and cleared from lights on zoning.

## Texture assessment and production

`build/environment-078-r65/texture-audit.json` covers 13,880 indexed world/ground
identities. The first pass found 7,569 installed sub-1K/no-resolution-gain entries.
That count is a review queue, not a visual quality verdict. The refined worker
audit separates effects, equipment, narrow strips, functional original textures,
provider rules, absent replacements and native-layout microdetail restorations.

The owner authorized repainting every confirmed poor texture even above 500,
and explicitly authorized parallel workers. Individually painted sources, exact
prompts and review records are saved under `build/environment-078-r65/workers`.
The original game alpha remains authoritative. Metal/emission masks are retained
when appropriate; snow/stone stay dielectric. No image is enlarged to claim HD.
Seven authored Athens paving paintings match their original broad RGB mean on
export while retaining every painted fine texel/joint. Their raw paintings stay
unchanged. Adequate alias targets remain installed as-is. A proven legacy-provider
mistarget, 292915's radial light field replaced by an armour atlas, now retains its
original functional input; personal/private overrides are unaffected.

`tools/integrate-environment-repaints.py` integrates accepted worker records into
the private environment pack. Resolution flags alone never mark a painting done.

## Validation

- `tests/lava_lighting_check.cpp`: regional identity, bounded sampling, actual
  triangle containment, invalid/vertical/distant inputs and deterministic placement.
- `tests/environment_078_render.py`: route contrast, short-lived impressions,
  procedural sky/legacy overlay interaction, stars-off independence and deliberate
  regional night-sky art direction.
- `tests/environment_r65_render.py`: regional light on real GPU output, retained
  material contrast, unchanged unrelated region, and emitted molten surface.
- `tests/environment_followup_render.py`: Penumbra patch submission and Enhanced
  Ground silhouette agreement, distant detail, captured Inferno ambient/fill
  colour, retained cast shadows and reduced molten emission/bloom.
- `tests/terrain_topology_render.py`: unchanged original cutout/overhang coverage
  for Shared Terrain+ outside the snow regions.
- `tests/hd_ground_material_render.py`: HD/detail material output differs on
  actual GPU frames; the cliff material remains visible with coarse upward AO
  normals and matches the normal height-field reference. Geometry is untouched.
- `tests/hd_ground_seams_render.py`: mixed private/legacy/missing natural maps
  produce identical HD colour, grid-edge discontinuity is reduced, and exact
  manufactured paving joints survive HD mode.
- `tests/hd_ground_path_render.py`: narrow winding route coverage/layout retained
  while old straight grass/gravel cell boundaries blend automatically.
- `tests/billboard_alpha_render.py`: soft HDR screen cards are pixel-identical
  under leaf-lighting/LOD toggles, with zero foliage draws; real leaf cards retain
  backlighting and forced far LOD. This is distinct from a retail flicker retest.

GPU results and logs are retained under `build/environment-078-r65`. Actual retail
UV fit, camera motion, all-biome appearance and performance remain playtest items.
