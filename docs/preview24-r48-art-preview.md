# R48 art preview: fixed rock clusters around cliffs

Break up a regular C-shaped cliff with larger angular masses along its foot, face, corners and rim. The original cliff is the backing surface. Clusters are fixed to it, with irregular sizes, heights, offsets and orientations. Their placement is not tied to the camera.

The current comparison uses a representative C-profile test cliff in AO-VK's actual renderer. It is not an AO playfield capture. Before and after use the same camera, lighting, material and original backing. The original backing, grass-cap and ground vertices and indices are checked for exact equality. The dressed view adds 40 rigid rock pieces; the full test scene is 8,656 triangles. The geometry fingerprint is identical for near, distant and moving-camera views, and no RockShape tessellation draws are submitted.

The models have broad fractured faces, closed corners and small explicit bevels. Geometry is generated once; there is no animated displacement, parallax, distance scaling, morphing or view-dependent remeshing. Photographic albedo and normals are used at 2K with a 1K matte occlusion/roughness map and full mip chains. Texture phase, mirroring and scale vary per rock, while all material channels use the same UVs. The material is [Rock Face 03](https://polyhaven.com/a/rock_face_03), with provenance in `rvk/assets/terrain/rock-face-03-source.json`.

Review images and verification are under `build/preview24/rocks-r48/review/`. Visual case 27, toggle 24 is the bare C-profile, 8 adds dressing, 9 shows it at distance, and 12 moves the camera. `AOVK_ROCK_PREVIEW_ASSETS` identifies the local terrain-material folder. The preview uses `--fixed-clock 16` (milliseconds), 320 frames and a 1400 × 875 image.

Before retail integration, dressing needs to be fitted to actual AO cliff surfaces and protected walkable/landing areas. Keep macro silhouettes at distance; use visibility and draw budgets rather than shrinking or warping the rocks as the camera moves. The current test does not change AO collision or install any rock models into the F: client. No art or scan downloads have been added to the product or Lite installer. Published Preview 24 is unchanged.

## Additional scan study

A separate source-geometry study is visual case 28. It imports [Rock Face 01](https://polyhaven.com/a/rock_face_01), [Rock Face 02](https://polyhaven.com/a/rock_face_02) and [Boulder 01](https://polyhaven.com/a/boulder_01), under [Poly Haven's CC0 license](https://polyhaven.com/license). Source downloads are verified against the official size/MD5 metadata and also recorded with SHA-256. `tools/prepare-rock-preview.py` preserves the scan positions, normals and UVs, recentres their origins, and splits only at the 16-bit index limit. It does not simplify the scans. These heavier assemblies are research previews, separate from the lightweight angular dressing comparison.

The only installed change during this preview work is the r47d zoning-text repaint/fade fix, documented separately in `preview24-r47d.md`.
