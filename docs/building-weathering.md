# Building Weathering

Preview 20 adds optional aging to a reviewed list of architectural materials. It
defaults to **Off**. Open the graphics settings and choose **Materials → Building
Weathering → Off / Low / Medium / High**. The equivalent saved setting is
`RVK_BuildWear=0`, `1`, `2`, or `3` in `randy-vk.ini`.

Low adds light stains and roughness variation. Medium makes localized grime,
runoff and material aging more visible. High expands stains, oxidation and coating
wear. Concrete, steel and painted panels use different responses. Maintained
modern architecture receives less aging. Patterns retain their broad detail at
ordinary viewing distances; fine chips fade as they become smaller than a pixel.

The classifier uses the original AO texture identity after HD map attachment.
It supports original and Full HD materials, classic and per-pixel lighting, and
PBR enabled or disabled. Unsupported textures keep their original appearance.
The two explicitly reviewed emissive ICC panels require their existing emission
mask; without that mask they remain unchanged, including in Lite. No optional
weathering images or geometry are required. Off skips optional material inspection
and shader texture reads/noise work. Existing production grass and terrain keep
their independent controls.

Characters, armor, terrain, foliage, labels, glass, water, sky, transparent draws,
unmasked emissive maps and ambiguous materials are excluded. A reviewed opaque
architectural draw must also have static geometry, a known playfield, depth and
lighting, and a supported texture-stage arrangement. Full HD is not required.

For a matched comparison, stand still in front of a supported wall, keep the same
camera, time and other graphics settings, and cycle Off, Low, Medium and High.
Repeat in daylight and at night. Animated light, weather, camera movement and
exposure changes can obscure the comparison. Clean or unsupported buildings may
show little or no change.

For diagnosis, `RVK_WearDebug=1` logs original material IDs, matches, eligible draws,
weathering-enabled submissions, PBR/HD flags and exclusion reasons. Level `2`
renders a mask visualization. Return it to `0` after diagnosis. A submission count
does not establish post-occlusion pixel visibility. Renderer logs periodically
report the counters; frame dumps include the per-material table.

Release comparisons use the actual Vulkan renderer with real artwork on controlled
geometry. They establish the implementation and level differences, while the final
gameplay evaluation in ICC, Tir/older settlements and industrial areas remains a
separate user check. Offline timings are not a gameplay FPS prediction.
