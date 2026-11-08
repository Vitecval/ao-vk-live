# Enhanced Daylights: SharedGround+ shadows

Revision 20 restores the ground-shadow behavior requested for Enhanced
Daylights without enabling GroundOn or changing SharedGround+ geometry.
The override is active only in SharedGround+ with outdoor daylight; the server
lighting choice and the below-horizon night state retain their previous path.

The opaque combined AO ground draw is `(lightmap + lighting) * base texture`.
Directional sun lights are deliberately excluded because sunlight is already
baked into the lightmap. This draw previously received F_SHADOW, which only
shadowed the nonexistent directional light in its per-pixel lighting equation.
Its baked sunlight stayed unshadowed, even though the caster map contained the
objects. The separate multiplying light pass already used F_SHADOWTEX and worked.

The sunny SharedGround+ combined draw now uses F_SHADOWTEX. Cast visibility
modulates its baked lighting before local lights are added, using the same
equation as the working separate pass. The sunlight-facing terrain/rock shading
previously suppressed for classic SharedGround+ also becomes available in sunny
mode, blended by the daylight amount so it fades with the sun elevation.
Retail triangle diagonals, cliff geometry, original tile patterns, and the
Improved Tiles selection are preserved. No displacement or tessellation is
restored, and no global display grading is added.

A real Vulkan reproduction toggles shadows around the same occluder and ground
inputs. Revision 19 produced zero darkened ground pixels in the combined pass;
the separate pass produced 6,657. With the correction both passes produce 6,657
pixels at the same roughly 42% shadow/lit ratio. The server-cycle combined
control retains its previous image. Additional checks exercise soft shadows,
night controls, intro/indoor/material-colour fixtures and representative native
texture uploads. The user must still approve the actual retail scene; synthetic
checks do not certify a visual match to Lost Eden or additional GPU families.

The local check uses rvk_demo --shared-terrain-test --terrain-lighting-fixture
--terrain-lighting-occluder --terrain-pebbles --terrain-blend 2 --day-cycle 2,
with and without --shadows. Add --terrain-separate for the separate-pass control,
--sun-soft 1 for soft filtering, and --day-cycle 0 for the server-path control.
