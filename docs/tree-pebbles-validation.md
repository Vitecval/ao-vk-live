# Trees, shared pebbles and matte metal floor — local candidate

This revision is a local test candidate. No GitHub release has been published.
Live visual comparisons are still required; rendered fixtures are not an in-game
verification claim.

## Behaviour

- Ultra selects AnarchyHD, Shared Ground+ and pebbles at density 1, with GroundOn
  off and colour grading off.
- Shared Ground+ uses AO's regular terrain draws and the existing shared texture
  enhancement and cliff materials. Its only added geometry is nearby pebbles.
  It adds no terrain tessellation, sand ripples, dune relighting or tile-edge blending.
- GroundOn overrides the shared modes when enabled. Its saved terrain tuning is
  retained. Its separate full-terrain lighting remains available for comparison;
  the pixel-equivalence claim below applies to the revised Shared Ground+.
- Plants → Fuller 3D tree foliage (`RVK_TreeVolume`) fills the space about leafy
  cards, retaining the original branch layout and leaf colour. Placement is
  deterministic; existing distance thinning, depth pre-pass and per-set leaf
  limits remain. Shrubs and palms are unchanged.
- The picked floor retains AO's original colour, tread and borders with a rough
  metal response (roughness 0.82, metallic 0.70). Soil/asphalt portions of its
  transitions remain rough dielectric. The material-only family is in Full HD
  and Lite; generated albedo, height and noisy normal maps are blocked at runtime.

## Floor identification and publication

The installation's `last-pick.txt` records `1010006:17` (the user's
Ctrl+Shift+I selection). The captured `rvk-frame-065817.txt`, draw 58, binds
`rdb:17 type:1010006` as stage 1 of an `AnarchyGround_t` combined draw. The
original database image is the bordered tread plate. Local image comparisons
and visual inspection confirmed its plate artwork in 22 companion transition
tiles. Resource identities and material masks are recorded in
`material-packs/ao-cyber-icc/deck-family.json`; ground quality tables 1010021 and
1010022 use the same IDs.

The old `1010006_17_d.png` and `_n.png` came from the ICC deck source artwork.
They are retained locally for provenance but excluded from the new publication
manifest. The baker now owns this whole family and emits ORM maps only. Runtime
filtering blocks old albedo/normal/height/emission files even in a preview or
personal folder. GroundOn's tile table now honours original-colour preservation
as the regular ground draw already did.

## Ownership defect and worker errors

The original crash was an access violation erasing a grass-cache node. Its next
pointer had become a frame number. AddressSanitizer subsequently reproduced an
earlier write through a freed `MeshInfo` in `DrawMeshInfo`: a registered terrain
slot stayed fresh while native ground consumed its draws, but the metadata
owner expired the record. Returning to the original draw path reused the stale
pointer. This is a demonstrated route to the later cache corruption.

Slots now carry the metadata owner's epoch and invalidate their cached borrow
after any metadata erasure. Nested native draws save and restore the parent
slot and static-buffer bindings, so child patches cannot use its cached record.
All grass-container access remains on the render worker; queued geometry and
playfield commands retain immutable shared snapshots.

Worker faults publish their original code/address and wake blocked callers.
Escaped C++ errors and unexpected worker exit are also detected. Callers fail
fast rather than retrying partially executed commands or waiting on a failed
worker. The original exception is not swallowed.

## Validation evidence

- `build/async2-grass-fix/asan-registered-before.txt`: reproducible heap
  use-after-free before the ownership correction.
- `asan-registered-after.txt`: 3,600 registered-terrain frames pass after it.
- `asan-preset-zone-final.txt`: 4,800 instrumented frames pass, including repeated
  enhanced/Original switches, HDR scene recreation, grass regeneration, five
  zoning/cleanup cycles, camera movement and turning, with concurrent rendering.
- `worker-failure-tests.txt`: 12 real command-ring cases pass: C++ exception,
  access violation blocked in a simulated legacy handler, and unexpected worker
  exit, each during sync, frame-limit, full-queue and shutdown waits.
- `build/shared-pebbles/validation.json`: combined and separate AO passes match
  regular Shared Ground exactly with pebbles off. Pebbles affect 194 visible
  pixels in the fixture. Ripple controls have zero effect on Shared Ground+;
  GroundOn produces the same result regardless of the selected shared mode.
- `build/shared-cliffs-final/validation.json`: Shared/Plus cliffs match with
  pebbles disabled, both with cliffs enabled and disabled, in combined and
  separate draw paths. Cliff controls now apply identically to both shared modes.
- Production proxy settings and two restart tests pass for Ultra defaults,
  manual GroundOn override, saved terrain preferences and immediate switching.
- `build/tree-volume/validation.json`: four original leaf masks, deterministic
  repeated bakes/toggles and a 64-tree forest pass. At 960×540 on the RTX 2070
  SUPER, the measured fixture GPU frame was 1.77 ms with volume and 1.72 ms
  without it. These are fixture measurements, not live-client frame rates.
- `build/deck-family/validation.json`: roughness 0.82, flat shading normals,
  quality-alias and shared-on/off pixel agreement, and visible transition masks.
  Production loader groups cover all 23 IDs and four lower-quality aliases,
  without applying albedo or noisy normal maps.

For a live check, compare the picked floor and its diagonal joins, inspect trees
from several angles and distances, and repeat Ultra → Original → Ultra while
moving and turning. Shared Ground+ should keep regular ground brightness with
small pebbles; enabling GroundOn should switch to its separate terrain renderer.
