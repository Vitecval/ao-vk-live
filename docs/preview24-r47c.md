# Local r47c: optional zoning confirmation and weather shadow eligibility

This is a local trial for the F: client. Published Preview 24 assets and its tag are unchanged.

## Zoning screen

General → **Zoning screen: Click to continue...** (`RVK_ZoneClick`) is off by default. The saved preference is independent of graphics presets, including Original. Disabling zoning screens also disables its effective value.

With the option on, the screen waits for loading to finish and four fresh world presentations before showing the prompt. A fresh mouse-button press or ordinary key press then starts the existing 320 ms fade-out. Early input, held-key repeats, modifier-only input, system shortcuts and background input do not dismiss it. The ready screen can wait indefinitely; the existing stalled-loading timeout remains. Every new transition resets readiness. Enabling the option after a finished transition cannot bring the old screen back.

The independent window remains non-activating and transparent to hit testing. The existing game-window input hook receives confirmation; keyboard and mouse input are blocked from gameplay through the wait/fade using a separate input-owner bit. Other input owners retain their own blocks. Releases still pass through the existing input system. The prompt is painted below the artwork's logo and shares the window's fade.

## Storm performance changes

Automatic weather could render both the game's large additive `VisualCloudlight_t` mesh and the replacement lightning flash. The original pulse is still captured for timing and position, but its geometry is now skipped when replacement lightning is active. Original weather and the original-glow fallback remain available. The original strike/envelope and tonemap sources are unchanged from the r47 baseline; this does not alter bolt geometry, thunder or sun-shadow preset defaults.

`VisualCloudlight_t`, `VisualLightning_t` and `VisualSingleCloud_t` are excluded from casting, retaining and receiving world shadows. Older scene hooks classify these as Other, so the former Sky-only exclusion did not protect them. Matching accepts the exact class or its `<...` class chain. Other scenery and character eligibility are unchanged.

A regression intentionally submits depth-writing transient weather. The uncorrected renderer retains eight casters after the weather ends (two real surfaces plus six unwanted entries). The corrected renderer retains only the two real surfaces throughout, with sun shadows still enabled. Its before/after world images match exactly. Shadow GPU time is 0.04 ms before and 0.05 ms after in this small fixture.

On the RTX 2070 SUPER, a synthetic 1080p stress fixture with forty overlapping legacy glow layers falls from 37.57 ms total GPU time (36.01 ms scene) to 2.32 ms (0.90 ms scene). This demonstrates the duplicate-glow cost, not the geometry count or expected FPS of retail ICC. Ordinary lightning remains visible; a clear-air control confirms that switching the original-glow preference does not change the replacement flash's pixels. Rain/fog scenes are not used for exact flash-image comparisons because their independent evolving effects can vary between runs.

**StrongHarm's exact persistent 60 → 19 → 12 FPS report is not reproduced or declared resolved.** The available F: session logs are from another scene without the reported storm. An affected-machine capture while Sun Shadows remains on is needed to determine any remaining shadow-map, receiver or memory cost. Frame dumps now include sun direction/luminance, range/resolution, validity, retention mode, storm and flash state alongside the existing caster and timing details.

## Checks and installation

- Ten actual-renderer lightning/recovery controls; legacy fallback and near-bolt controls included.
- Five existing world-shadow regressions: mixed depth ranges, moving scenery and retained-depth equivalence.
- Fresh-install default, all-preset preference preservation and process-restart settings checks.
- Zoning policy, five-aspect-ratio prompt painting, input filtering, actual independent-window wait/fade/reset and shutdown checks.
- Background-rendering policy retained from r47b.
- Local installer fixture checks: install, repeat install, changed-file protection and exact renderer/receipt revert with settings preserved.

`tools/Install-Preview24-r47c.cmd` installs only the verified renderer and its ownership receipt into an idle F: test client. `tools/Revert-Preview24-r47c.cmd` restores its saved renderer and receipt. Settings, loading art, texture packs and prepared material caches are preserved. No game is launched. GPU shaders are unchanged, so the existing profile pipeline cache is preserved too. Detailed evidence is under `build/preview24/lightning-r47c/`.

This trial does not install the unfinished Enshrouded-style cliff prototype or its research textures.
