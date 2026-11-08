# Preview23 Ultra, texture fidelity and continuous terrain

Ultra selects ICC Glossiness 0.5, AnarchyHD world artwork with HD replacement
colour enabled, low Building Weathering, Enhanced Daylights, and Global Shiny
0.5. Clouds default off in every quality preset. Other quality presets retain
the independent world-artwork choices. Enhanced Daylights follows server time,
including when stale debug hour/speed values were saved earlier.

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
