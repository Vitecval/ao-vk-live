# Preview 24 local revision 47: rock geometry and non-periodic surfaces

This local trial follows revision 46. The published Preview 24 downloads, tag and checksums are unchanged. Evidence and backups live in `build/preview24/rocks-r47`.

## Rock geometry

Enhanced Rocks now has a separate **Rock shape** strength control, enabled by default. Eligible opaque static rocks receive actual adaptive GPU subdivision and a bounded world-space deformation near the camera. Smooth source corners receive restrained Phong rounding; irregular ledges and lateral erosion break up straight low-poly faces and outlines. This changes rendered geometry, rather than relying on normal maps alone. Game collision and large-scale placement stay original.

The topology pass welds positions across normal and UV seams, identifies open/non-manifold borders and identifies disagreeing normals at hard creases. Open borders are pinned, preserving grass-cap and adjacent open-piece joins. Phong rounding fades at hard creases; shared world positions use the same deformation field and amplitude. Height changes from the erosion field are small; total rounding is separately clamped. The geometry fades from full strength at 20 m to zero at 40 m. Motion vectors account for the previous camera's fade, and geometric normals use world-sized finite differences to avoid precision noise at kilometre-scale coordinates.

Subdivision is capped at 8,000 generated triangles per draw, using odd caps of 1, 3, 5 or 9. Meshes above 8,000 source triangles, above 20,000 vertices, below one metre in maximum size, without supported topology, on GPUs without tessellation, and non-static/character/foliage/transparent geometry retain their material enhancement. Ground and native clutter use their existing geometry paths. Boundary metadata is reused from a bounded CPU cache of at most 512 entries (under 16 MiB of flag data), protecting currently active entries. It reuses the renderer's existing index fingerprint instead of hashing the entire index buffer each draw. Only border flags are copied into the existing frame ring; there are no per-rock texture allocations or persistent extra GPU meshes.

Shadows retain the original coarse casters and their existing caching/cadence. The close-range render deformation is small, but the shadow outline is consequently an approximation. This trial does not claim wholesale replacement of AO's source models or changes to walkable/collision geometry. Rock shape at zero disables this geometry path while retaining the surface variation.

## Reduced repetition

Each triplanar projection blends three stochastic photographic patches using a continuous triangular lattice. Patches vary their offset, horizontal direction and scale, so the original 3.6 m image no longer repeats as one fixed wallpaper pattern. Albedo, normal and occlusion/roughness maps share the exact same transforms and blending weights; normal directions account for mirroring. Explicit gradients retain mip stability, and contrast compensation prevents patch blends from washing out the stone. The existing close grain layer, biome tint, moss mask and distance/footprint fades remain.

The same embedded CC0 photographic maps are shared: 16 MiB including mip levels, with no new downloads or prepared texture-cache changes. Sampling more photographic patches and tessellating geometry do increase GPU work; exact fixture timings are recorded in the material and shape reports. The prototype measured roughly 0.36 ms additional geometry time for one rock and about 1 ms for fifteen rocks at 768×512. Retail cost depends on visible rock coverage and mesh complexity.

## Validation and delivery

CPU tests check welded seams, open/non-manifold edges, hard-normal seams, invalid indices and the triangle bound. Actual Vulkan tests check changed silhouettes, pinned open-face boundaries, stable camera turns, baked-lighting meshes without vertex normals, exclusions for characters and unknown textures, and topology sharing across fifteen instances. A wide-wall image is checked for loss of the former fixed horizontal tile period. Shared/native terrain separation and shadow depth-range/movement/static-cache regressions are checked independently.

Settings validation covers the new control's parent, default, Original mode and process restart. The final settings-table correction does not change the renderer library or graphics-fixture executable; `RENDERER-CORE-BEFORE-SETTINGS.json` and `RENDERER-CORE-VERIFIED.json` document that provenance when connecting the original GPU reports to the final DLL. Graphics fixtures are not a retail playtest.

The local installer verifies the candidate, installation ownership, staged copies and backups before using the existing recoverable transaction. It changes only the renderer and its receipt. Preferences and prepared material files are preserved. The isolated fixture checks install, repeat install, modified-file refusal and exact revert. `tools/Revert-Preview24-r47.cmd` restores revision 46 if the current trial still matches its ownership record. Pipeline preparation now includes a known rock and the geometry shader path before the game is launched; its driver cache is prepared locally and delivered with a verified backup.
