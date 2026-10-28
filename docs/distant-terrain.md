# Distant terrain and shared cliffs — local candidate

Preview 16 makes Shared Ground+ use the actual GroundOn patch/rock/LOD path with
classic AO-style lighting and added ripples disabled. Standard Shared Ground keeps
the original submissions described below. See [Preview 16 validation](preview16-validation.md).

Shared Ground and Shared Ground+ now use GroundOn's existing embedded rock
material and its smooth-normal slope mask, including the same irregular transition
thresholds. This replaces the stricter triangle-slope mask in the shared modes.
Verified terrain draws can use world-space cliff projection even when rotated
cells or runtime composite textures fail the square-tile fit. The fallback still
requires the terrain visual and the ground vertex/world-matrix checks; buildings,
props, sky and interface do not acquire it.

`RVK_TerrainFar` controls filtered distant material composition (0–1, default 1;
disabled with enhancements off). Coarse soil/rock samples retain the original
biome tint and use slope-aware exposure. World-anchored formations at tens to
hundreds of world units survive after fine aggregate disappears. Hue and slope
lighting changes are restrained; derivative filtering removes subpixel bands.
Authored city floors and asphalt do not receive this additional composition.

The existing optional horizon haze now follows an optical-depth curve with
restrained desaturation. The near clear radius remains clear and far ridges retain
structure. Existing weather and sky colours still supply the atmosphere. This
does not alter the separate custom fog controls or colour grading defaults.

Water reuses its existing scene-depth contact mask. Shallow submerged ground gets
a restrained wet band; the contact fade widens with pixel footprint, and distant
shore foam becomes a broad faint band as close foam detail fades. These changes
follow `RVK_TerrainFar` and do not invent a global sea level or wet inland terrain.
Wetness above the actual water boundary is not added.

No terrain silhouette geometry, rock populations or vegetation impostors are
added. Ridge material definition improves, but original horizon geometry remains.
There is no new per-frame mesh generation, terrain capture, grass regeneration,
background worker or borrowed resource lifetime in this change.

Validation uses rendered fixtures, not in-game visual confirmation. The tests in
`tests/distant_terrain_render.py` cover day, sunrise, sunset, forced overcast,
an elevated distant view, runtime composite cliff coverage, repeated rendering,
Shared/Plus agreement, optional haze and near/elevated shoreline views. GPU timings
are fixture measurements on the local RTX 2070 SUPER; they are not live FPS claims.
See the local `build/distant-terrain-shore-final/validation.json` for results.
