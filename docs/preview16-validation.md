# Preview 16 — saved artwork and native Shared Ground+

Local test build only. No public release or GitHub push is part of this update.

## Artwork

621 new resource identities have new baked material sets: 620 from the saved
artwork and one generated bordered-soil companion (1010004:99366). All 235
previously cataloged pending repaints are included. Of 32 reported uncataloged
atlases, 31 have been integrated; atlas 303 was already represented by the
approved exact repair source for 17824 and is not counted twice.

Earlier baked maps retain their hashes. Deliberate functional originals remain,
including the alpha-only cobweb 157249 (constant RGB, coverage in original alpha).
The identified turquoise floor family retains its existing ORM-only policy;
excluded generated colour, noisy normal and height maps remain blocked.

There are 38 active recorded families: 35 complete inventories and three partial.
Remaining exact IDs are recorded in building-families.json and the local report.
Incomplete owned batches are explicitly labeled; a validated resource's complete
map set can publish independently of another resource's missing artwork. Provider
selection, all required-map checks and original alpha restoration remain mandatory.

## Terrain

Shared Ground+ consumes the original terrain submissions and uses the same native
patches, cliff triangulation, tessellation, tile/material tables, frustum range,
clutter and pebble implementation as GroundOn. There is one terrain owner per
pass. Standard Shared Ground continues using classic terrain submissions.

The previous Plus path explicitly suppressed the native patches and boulders,
drew only pebbles, and returned the original draw. It therefore could not inherit
GroundOn's geometry or detailed distant per-cell materials.

Lighting is selected independently through the frame block. Plus retains the AO
baked lightmap, ambient and ordinary shadow response, while disabling native sky
occlusion, sun-turn/dune relighting, and the additional native sun-away multiplier.
That multiplier otherwise darkens a native slope even with GroundSun and AO at
zero. No exposure, gamma or global ambient compensation was added. GroundOn stays
selectable with its separate native lighting settings for comparison.

Plus disables sand ripple relief, ripple glints, added water ripple normals,
disturbance simulation, rain ripple perturbations and wet-puddle rings. Ordinary
water geometry, lake/waterfall rendering, reflection and refraction remain.

The existing nested-draw MeshSlot/buffer save-and-restore scope and metadata owner
epochs are retained. Ground capture uses immutable queued snapshots; mode changes
reuse the captured native data rather than rebuilding it every frame.

## Validation boundaries

New material sets were exercised through the asynchronous production proxy with
quality aliases, original-alpha restoration and style toggles. Missing/corrupt
required maps retain originals for that resource, without hiding an unrelated
valid replacement. Premium ICC uses explicit declared providers, including
material-only inputs.

Controlled terrain fixtures exercise native/clasic selection, cliffs, rock and
pebble populations, far views, night controls, unknown-pass fallback and mode
cycling. Standard Shared Ground is pixel-identical to a freshly built preserved
Preview 15 reference fixture. Combined/separate native passes have nearly equal
brightness, with isolated rasterized rock-edge differences. Instrumented tests
exercise 4,800 registered-terrain/grass frames, mode switches and zoning teardown.

These are automated fixtures and flat-atlas review, not in-game visual parity.
Live comparison with the user's screenshots, full models, real nighttime, distant
LOD transitions, multi-zone residency and long sessions still need gameplay.
Detailed metrics and installation verification are saved in build/preview16.
# Preview 16.1 correction: complete classic terrain material equation

The cobblestone comparison identified two differences beyond native sun shading.
Native cells discarded the authored-ground color policy, applying shared masters
and variation over protected paving. The native one-pass light draw also clamped
before multiplying albedo, suppressing the material highlight on the whole tile.

Shared Ground+ now preserves the retail combined ADD(lightmap, diffuse), then
MODULATE(base) contract through the common shader. Ambient, shadow classification,
fog and highlights follow classic Shared Ground, with native per-cell geometry
and materials. Authored-color and height attributes are independent per cell.
Without the captured lightmap, AO's original draw remains until it is available.
GroundOn retains its separate native-lighting behavior. No brightness/exposure
multiplier or global color-space change was introduced.

Reserved PBR diagnostics 12–21 capture raw albedo, vertex/material diffuse,
ambient, directional/local diffuse, applied shadows, raw lightmap, final linear
RGB, lightmap modulation, lit diffuse and caster visibility. Automated comparisons
use the same original AO paving/rocky-road textures, camera, clock, weather and
settings, including dark/light map regions, night and HDR. These are controlled
fixtures; actual gameplay screenshot parity still requires a live comparison.

