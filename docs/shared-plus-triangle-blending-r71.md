# Private r71: Shared Terrain+ cliff blending

Natural Shared Terrain+ now derives its material weights, fragment lighting base and shadow-facing normal from the captured height field. The original AO vertices, indices, world positions, clip depth and coverage remain unchanged. Shared+ does not switch to Enhanced Ground/GroundOn geometry.

The 0.7.7 transition sources, per-cell selection, original tile joints and `TerrainBase077` colour function remain in place for non-snow terrain, with either HD setting. This change corrects how those materials meet on slopes; it does not replace their palette or introduce individual ground paintings into the restored path.

## Why the first correction was incomplete

The scans report straight-up AO shading normals on terrain containing steep faces. The old cliff-material floor therefore followed each triangle's geometric slope. The first r71 correction replaced that floor, but still used geometric flat eligibility and geometric support. A source cell could contain one steep triangle and one flat triangle, yielding material weights of 1 and 0 at the same shared edge. Milder cells also retained gaps as large as 0.735.

Verified natural cells now use the field normal on both halves. Material support uses field slope, never triangle slope. A genuinely flat field still has zero cliff weight and retains its original colour. A flat triangle inside a sloping field is no longer exempted from the continuous blend.

Side textures use bilinear field height for their vertical coordinate, retaining the rock layers' existing X/Z variation. Their mip footprints and material tangent basis use analytic derivatives of that same height. This avoids collapsing a side texture into a stripe on a geometrically flat half. Toe/crest joins also use field height. These are material coordinates, not displaced geometry.

Lighting retains the original AO normal magnitude and baked/vertex shade. Existing normal maps perturb the continuous fragment base afterward. Shadow facing and bias consume the same unperturbed base, so they cannot retain an unrelated AO-normal boundary. Manufactured and ice surfaces, and terrain without a verified grid, retain their existing policy. Snow retains its existing UV and normal construction.

## Subtle Inferno warmth

Inferno's ambient neutralization changes from 90% to 85% at full regional strength, preserving a little more of its original warm ambient colour. Mixing remains toward the source's own luminance, so this is not a brightness increase or a global red filter. Other regional palettes, directional lighting, shadow strength, local lamps and lava RGB/emission are unchanged. Neutral ambient stays neutral.

## Evidence and limits

- The final source verifier passes against the hash-verified 0.7.7 shader and confirms that its colour function, source dispatch and original tile-joint policy remain intact.
- The CPU counterexample check passes 176,144 diagonal samples over 1,744 captured natural cells, including repeated cells across captures. All four reported residuals now have zero modeled material and side-UV/Jacobian gaps. The former flat halves have nonzero field derivatives where appropriate. Truly flat fields and normal-magnitude controls pass.
- The compiled `inferno_warmth_check` passes. For captured ambient 0xA23232 at area strength 0.6, luminance remains 0.327843; residual chroma changes from 0.043922 to 0.065882. The test also pins other palettes and checks bounded, luminance-preserving behaviour over saturated, dim and neutral inputs.

Receipts are under `build/environment-078-r71/natural-field-cpu-check/` and `build/environment-078-r71/independent-checks/`. The CPU model uses source float32 heights evaluated in float64; it does not establish GPU pixel continuity or reproduce every adaptive retail triangle. Flat-field algebra and functional-role guards are checked, but mixed natural/manufactured/ice/snow boundary pixels still require the separate GPU fixtures and retail review. The snow preservation runner covers near/far diagnostics, mapped normals, powder and active material controls separately; the natural-cell CPU audit does not test snow. GPU results are recorded by their owning worker, not claimed by these CPU/source receipts.

Nothing is published by this private update.

Final controlled GPU verification passed 58 cases across all four captured neighborhoods. Flipping the original-grid diagonals changes material weights and base/perturbed normal readbacks by at most one byte, with no pixels changing by more than that. The previous renderer produces substantial differences in the same material control. A real collected caster still changes both shadow visibility and the lit image. Two validation-layer runs report no errors. These are controlled views of captured source data, rather than exact retail camera or adaptive-mesh replays.

The near/far snow preservation checks, twelve independent 0.7.7 colour controls and eleven framebuffer/display regression cases also pass against the final renderer. Physical 4K presentation and independent interface scaling are not established by the offscreen display tests. Mixed material-role boundaries still need retail review; the legacy manufactured classification is preserved and does not include every newer carved-floor identity.
