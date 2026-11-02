# Shared Ground+ flat terrain investigation

The October 28 captures `rvk-frame-222852`, `222854`, `222902` and
`224554` in the active installation on `<AO installation>`
show native replacement patches with D3D lighting disabled (`lit 0`).
The stage layout is still ADD(lightmap, diffuse), then MODULATE(base,
current). The captures report 288 native patches, classic baked lighting,
and original terrain consumed. This is a material submission failure,
not absent terrain geometry or an artwork download failure.

`IsCombinedTerrainPass` required D3D lighting to be enabled. The same draw
therefore fell through `NativeGroundReplaces` as an unlit base pass.
Its inherited stage-0 coordinates addressed the lightmap, not the base
texture, so `Draw` did not populate the native tile-table record. Native
patches reached the shader with the 1x1 stand-in and the inherited stage-1
texture instead of selecting each cell's terrain material. This explains
the nearly constant brown/green terrain, despite visible geometry and shadows.

The fix recognizes the combined pass from its exact stage and coordinate
contract regardless of the D3D lighting switch. Native one-pass records
select the lighting/material path independently of that switch, including
clutter records. A separate base pass must use untransformed coordinate set
0; unknown unlit stage layouts retain the original terrain submissions.
The lighting switch itself is preserved. Frame dumps now include native
record pass, tile-table slot, one-pass/combined state and lighting state.

Validation artifacts are in `build/shared-ground-texture`:

- `before/result.bmp` reproduces the flat surface using the old renderer
  with the newly added `--native-unlit-combined` fixture switch.
- `after/plus-unlit/result.bmp` restores terrain texture and cliff detail.
- `after/validation.json` covers 19 rendered cases, including unlit Plus,
  unlit GroundOn, ordinary unlit Shared, unknown unlit stages, existing
  lit/separate submissions, mode cycling, night, far terrain and detail
  controls. Native unlit records must contain cell textures and pass 2.
- Ordinary Shared matches the saved Preview 17 reference with maximum
  pixel delta 0. Native AO/sun/ripple suppression checks still pass.
- Rendering logs contain no Vulkan validation errors or device loss.
- The x86 `rvk_demo` and `ao_vk_live` targets build successfully.

Reproduce with `tests/shared_native_terrain_render.py --output
build/shared-ground-texture/after --reference
build/preview17/terrain/shared/result.bmp`. The bundled Python runtime
provides NumPy and Pillow for the checks.

The fixed binary is `build/windows-live/ao-vk.dll`, with `ao-vk.map` beside
it. The running game retains its previously loaded DLL; this investigation
does not claim an installed or live-game-verified correction. No release,
push, artwork regeneration or preference change is included.
