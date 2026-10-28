# Shared Ground+ terrain visibility

The native terrain replacement spans multiple retail AO terrain chunks. A
chunk's base texture can be completely transparent: Shadowlands far terrain
uses such coverage around the gaps between islands. Choosing the first chunk
changes with camera angle.

The native combined pass previously retained that first texture for its early
alpha test, including the depth prepass. Its terrain color came from the native
cell table, but its coverage still came from the unrelated retail texture.
Consequently an invisible first chunk could discard the entire native terrain
while grass and other objects remained visible. This was reproduced with a
controlled transparent-first-chunk fixture.

Native terrain now tests each cell's own alpha in both color and cutout depth
passes. Original Shared Ground, nonterrain shading, classic terrain lighting,
water and ripple settings are otherwise unchanged by this correction.

AO ground texture **774**, across the three ground quality resource types, is
the reviewed Shadowlands **Brink** void marker. Its source JPEG visibly contains
that label. Native geometry previously triangulated these cells into a visible
floor. The replacement now excludes their triangles and excludes their rock and
pebble placements using an immutable cell-visibility snapshot. This does not
change game collision or teleport logic; the marker's gameplay role has not
been established. Ordinary black terrain textures remain eligible.

The cell mask is built once when a playfield is captured. It does not rebuild
terrain per frame. Native patches retain their indices/ownership and normal
frustum/LOD selection; empty patches submit no terrain triangles.

## Validation

Run the controlled regression with a built Windows x86 `rvk_demo`:

```powershell
python tests/terrain_visibility_render.py --output build/terrain-visibility
```

The test compares transparent and opaque first chunks at two camera headings,
with and without a strip of void cells. It checks rendered equality and actual
terrain index counts. Existing terrain-detail, classic-lighting, mode-switching
and worker-lifetime checks cover the remaining rendering path.

Frame dumps report terrain mode, geometry, lighting and ripple selection,
native terrain index count, hidden-cell count, alpha-test state and both stages'
alpha operations. Save a frame and screenshot with **Ctrl+Shift+F9**.

The defect was identified using paired live Nascence captures. Controlled tests
verify the correction; live visual verification of the corrected build at those
same locations and extended gameplay testing remain required.
