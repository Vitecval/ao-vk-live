# Shared Terrain+ 0.7.7 restoration, private r70

The requested baseline is the verified 0.7.7 renderer source, rather than the later Enhanced Ground renderer. Ordinary Shared Terrain+ now restores that baseline's texture selection, authored-layout flags, world detail/HD synthesis, and cliff projection. Enhanced snow remains a separate path. Display, sound, lighting, signs and other object artwork remain intact.

## Cause of the visible squares

The later `ImprovedTile` early return selected individually repainted natural-ground tiles even when HD Ground was disabled. Scan 143150 selected replacement artwork for 133 of 138 tiles. The paintings' RGB transition layouts and palettes did not form the connected native family used by 0.7.7. Preserving alpha did not fix that: these relevant native transitions have fully opaque alpha and encode their boundaries in RGB.

Returning to original geometry alone did not restore the old appearance. The later shaders also changed master feature size, palette treatment, projection weights and source classification. Those non-snow Shared+ changes are restored to 0.7.7. Per-tile ground replacements remain available to other applicable paths; the installed artwork library is preserved.

## Scope

- Non-snow Shared+ uses 0.7.7's original transition sources and shared detail. HD Ground selects its original HD master treatment; switching it off keeps the original shared detail path.
- Its original ICC exceptions, authored layouts and asphalt treatment remain as in 0.7.7.
- Snow-bearing cells retain the newer continuous normals/lighting, snow material, powder and prints. Shared+ submits original AO terrain, including snow. Explicit Enhanced Ground remains independent.

## Evidence

The independent reference executable is rebuilt from the immutable 0.7.7 engine snapshot. All 790 recorded build inputs match before and after. It does not use the restored shader as its reference.

Twelve near/far, winding-path, cliff, cliff-toe, paving and gravel GPU surface-colour comparisons cover HD off and on. They match the independent reference exactly and include nonblank/active-HD controls.

Two scanned neighborhoods use 441 cells each, exact source heights, rotations and 76 original tile images. Eight comparisons cover both HD settings and both native-only and installed-repainting controls. Surface colour matches the independent reference within one channel value at a single pixel; attached non-snow paintings do not change the restored output. These are controlled native-family replays, with local coordinates, a controlled camera and white-light/opaque-alpha controls, rather than complete retail frame replays.

Snow-only comparisons against the preserved r69 renderer separately check that its enhanced snow appearance, normals, powder and material detail remain intact. Current caustic and native-resolution checks are separate regression evidence.

Live AO visual acceptance still requires the user's test. Controlled material readbacks do not prove exact live camera, shadow, object occlusion or transparency reproduction. Nothing is published by this private update.
