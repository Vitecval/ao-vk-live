# Shared Ground+ terrain details — Preview 13

Historical Preview 13 design. The unreleased revision now keeps regular Shared
Ground and adds only pebbles at density 1; GroundOn stays off in Ultra and
overrides the shared modes when enabled. See `tree-pebbles-validation.md`.

## Cause

Preview 12 had two independent gaps. Production settings treated the stored
GroundOn switch as the only parent of pebbles, loose rocks, cliff depth and
other terrain details. Ultra selected Shared Ground+ but kept manual GroundOn
off, so those child settings had effective zero values. The direct renderer
fixtures bypassed this production settings interaction.

Separately, the renderer's Plus-only branch captured tile data and drew a pebble
overlay without constructing GroundOn's full terrain patches and cliff/rock
geometry. Enabling the manual switch changed the path. This was not merely a
missing checkbox or an artwork-pack failure.

The ground capture callback could also be installed after the first zone event.
Capture now starts deferred and can recover from the next live ground callback.

## Implementation

- `proxy/ddraw/rvk_settings.cpp`: `SharedGroundPlus`, `V` and `ApplyOne` enable the
  effective terrain parent and reapply terrain controls on mode/style changes.
  Extra tile-edge blending, AO, terrain sun relighting and dune relighting remain effectively zero in
  Plus. Stored manual values are retained. `Clamp` migrates Classic HD value 1
  to AnarchyHD value 2; the selector contains Original and AnarchyHD only.
- `proxy/ddraw/rvk_console.cpp`: `FormatValue` and `NoEffect` show automatic
  GroundOn and the temporarily disabled lighting controls accurately.
- `proxy/ddraw/rvk_ground.cpp`: `OnNewGround`, `OnGroundFrame`, `GroundFrame` and
  `SetNativeGroundSetting` use the same capture lifecycle for manual GroundOn
  and Plus. Remove the competing data-only detail mode. Existing bounded tile
  capture remains; terrain data is not rebuilt each frame.
- `rvk/native_ground.cpp`: `SetNativeGround` builds the established patches;
  `NativeGroundReplaces` uses the same combined or separate terrain passes and
  existing pebble/cliff logic. Remove the extra Plus overlay branch.
  `DestroyNativeGround` clears pending draw state and discards old sky workers;
  `PollNativeSky` and `FillNativeLight` keep added terrain darkening off in Plus.
- `rvk/draw.cpp`: `FillFrameLights` suppresses extra tile-edge blending, native
  terrain AO and dune relighting in Plus, including direct renderer callers.
- `rvk/rvk.h`, proxy headers, `tools/gen_interface.py` and generated exports:
  share the Plus predicate and expose the optional read-only
  `RvkSettings_Effective` query without changing existing settings structures.
- `rvk/diag.cpp`: requested frame dumps include native patch/detail counts and
  effective terrain lighting values.

This enables GroundOn's existing renderer automatically; the terrain therefore
uses its replacement patches rather than Preview 12's data-only Plus path.
Unsupported original draws retain the existing fallback. There is no new terrain
or pebble pass. One-time zone capture and patch memory now match the working
manual GroundOn implementation; this is not a claim of unchanged memory cost
against Preview 12's incomplete path. Artwork and pack selection are unchanged.

## Automated validation

The Windows x86 renderer, demo and production DLL harness build successfully.
`build/groundplus-fix/results.txt` records the pixel comparisons and draw metrics.

- Production settings harness: GroundOn, Shared Ground, Shared + GroundOn, Plus
  and Ultra; immediate changes, Enhancements/style off and on, saved manual
  preferences, legacy style migration, then two fresh process starts. All pass.
- Twenty renderer cases include both combined and separate terrain draws,
  mode cycling, positive pebble/cliff controls and ordinary Shared regressions.
  No validation errors, device loss or missing screenshots occur in these runs.
- The Plus fixture draws four patches per frame, 1,120 loose rocks and 1,512
  pebbles, with native AO and terrain sun relighting zero. Plus, Plus with manual
  GroundOn, the reference with added terrain lighting and edge blending off, and the final cycled
  mode are pixel-identical. Counts match, with no second terrain pass.
- Disabling pebbles changes 194 pixels visibly; disabling the cliff controls
  changes the cliff rendering. These positive controls check actual output,
  rather than merely trusting configured settings or generated geometry counts.
- Ordinary Shared Ground continuity, asphalt roughness and deck palette cases
  are pixel-identical to the saved Preview 12 reference images.
- Combined and separate Plus/reference comparisons have maximum pixel delta 0.
- Plus is identical with a requested tile-edge blending value of either zero or
  one, while the production settings test verifies its stored manual value
  survives and resumes with ordinary GroundOn. A separate texture-off control
  verifies that the enhanced floor textures remain visible in Plus.
- A close tile-boundary view is identical between Plus and the manual
  no-blending reference. Turning blending back on changes 82 pixels visibly,
  confirming that this control exercises the feature being disabled.

Reproduce rendering with `tests/shared_ground_plus_render.ps1`. Analyze output
with `python tests/shared_ground_plus_tests.py build/groundplus-fix` after the
production harness settings/restart runs and Preview 12 reference captures are
present. The harness flags are `--terrain-settings-test` and
`--terrain-settings-restart 2`, followed by `--terrain-settings-restart 1`, in an
isolated test client directory, never the live user's preference file.

## Live validation still required

Automated scene rendering and real settings save/reload are covered. They do
not establish behavior at every live zone, actual cliff/model fit, grass and
rock placement against collision, extended zoning stability or every third-party
options window's presentation. After restarting the game, inspect the previously
reported hill and cliff, switch Shared/Plus, zone away and back, and confirm
terrain brightness and details. The in-game console reports effective automatic
states; external options clients may continue to display the stored manual
GroundOn switch until they adopt the optional effective-value query.

No release tag, GitHub publication or push is part of this fix.
