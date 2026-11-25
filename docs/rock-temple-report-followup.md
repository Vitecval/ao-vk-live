# Local follow-up to Horizon 0.7.6

This patch is a local test candidate, not a published release. The public
0.7.6 / r62 packages and the private 0.7.7 character artwork are unchanged.

## Captures reviewed

The F: test installation's November 19 captures at 11:21:35 and 11:21:37 show
Adonis ground rocks becoming invisible as the camera looks down. Shared Ground+
decorations inherited the first retail terrain chunk's second texture stage,
alpha test and, for two-pass terrain, multiplying blend/equal-depth state.
Generated decorations now use opaque, depth-writing material state; all retail
state is restored before the original ground submission continues.

The 11:22:43 and 11:24:43 captures show the Clan/Redeemed temple interior in
playfield 4625. Its architectural spots have 583–587 m ranges and attenuation
0.05 + 0.00002*d*d, producing up to 20x light before overlap. Only this captured
light family is normalized by 0.05. Both direct and collected frame lights use
the same correction; ordinary lamps, personal lights, sun and other zones do
not match it. Player intensity settings remain in effect.

Both temple frames also finish HDR at screen-space draw Q41, before the first
room. Offscreen triangle draws no longer count as the beginning of visible UI.
A matching-state synthetic screen probe now leaves the room pixel-identical
to a frame without the probe, while actual UI still ends HDR normally. Future
frame dumps include screen-space bounds and visual names to confirm the exact
retail draw. The jagged white surface in the latest capture still requires a
fresh in-game capture after applying this candidate; the synthetic tests do
not establish its complete cause or certify its retail appearance.

## Report form

While sending, the reviewed fields and preparation action are disabled. A
confirmed success displays a bold coral-red confirmation panel and changes
the send action to **Send a New Report**. That action clears title, description
and review, restores the default log option, and requires preparation again.
The accepted report's ZIP remains available until the new form is started.
Failed or timed-out requests retain their prepared ID and permit retry.

## Verification and handoff

- `build/rock-temple-fix/final-verification/RESULTS.json`: low/high camera views,
  combined/two-pass terrain, transparent first-chunk input, genuine UI pixels,
  temple reduction and an exact unchanged other-zone comparison.
- `build/report-flow-tests/FEATURE-UI-RESULTS.json`: successful send, double
  click, blocked preparation, reset, failed send and retained-ID retry checks.
- `build/report-flow-packaged-tests/UPDATE-TEST-RESULTS.json`: mocked report
  transport and existing update checks; no network request or email sent.
- `interior_lighting_check`: captured family boundaries, other zones, ordinary
  lamps, sun and invalid-range behavior.
- `build/rock-temple-fix/INSTALL-FIXTURE.json` and its revert record: isolated
  installation and restoration, including settings and protected-file hashes.

Renderer and matching preparation helper: `build/rock-temple-fix/candidate`.
The report-UI executable is in `build/rock-temple-fix/app-only`; its embedded
installer still contains the published r62 graphics, not the local renderer.
Do not use that app bundle to install the trial graphics.

`tools/install-rock-temple-trial.ps1` applies only the verified renderer to the
F: test client, backs up the existing renderer and receipt, and preserves its
Full edition, settings and texture cache. `-Revert` restores that exact backup.
The client and launcher must be closed. Other AO installations are excluded.

## Temple follow-up: November 19, 12:27–12:28 captures

The first local trial did not sufficiently reduce the column. The new logs
identify two additional causes that the original spotlight-only fixture did
not reproduce:

- Q41 is a full-viewport `VisualClearScreen_t`, not an offscreen probe. It clears
  the indoor world before rooms render. It must remain in the scene so it does
  not prematurely switch all subsequent room draws to the 8-bit output.
- The column is a separate `RTriMesh_t`, using vertex colours as emissive baked
  lighting (`EMISSIVEMATERIALSOURCE=COLOR1`). Its first recorded vertex is
  (248, 252, 248); the room mesh's first recorded vertex is (54, 58, 54).
  Scaling spotlights does not remove that baked component.

The second trial keeps `VisualClearScreen_t` in the scene. In playfield 4625,
only separately submitted static scenery with vertex-emissive lighting gets a
smooth baked-light shoulder: values below 0.24 are retained and brighter values
approach 0.28, preserving hue. Room geometry, including the reference floor,
is excluded entirely. Character, effect, additive and other-zone draws are
excluded. Both vertex-lit and pixel-lit paths apply the same treatment.

The captured-state comparison is `tests/temple_baked_render.py`; outputs are
under `build/temple-baked-fix`. It checks column detail/brightness against the
floor, unchanged floor pixels, the actual full-screen clear class, and genuine
UI presentation in both lighting modes. A fresh retail capture is still the
confirmation of the player's desired appearance.
