# Local 0.7.6 follow-up: startup, login and temple stone

This is an unpublished test build. Public 0.7.6/r62 and the private 0.7.7
artwork are unchanged. It retains the ground-rock visibility and report-form
changes from the earlier local trials.

## Evidence

November 19's 14:17–14:25 F: captures identify the authored login hall's three
point lights, including its green/cyan lamp (diffuse 0.71, 0.91, 0.87). The
presentation log also shows login followed by an unknown engine state, which
previously switched login-specific lighting off. Preserve a known login state
through that temporary unknown state; confirmed world entry still switches
immediately to normal world rendering, and confirmed logout returns to login.

The login scene now uses AO's authored vertex-light interpolation with HD
textures, without extra PBR illumination, night-glow amplification or HDR
headroom. LoginScene changes refresh constants and frame lights as well as
tone mapping. World preferences remain saved and active in the world.

Temple stone in playfield 4625 gets an explicit matte response for the known
stone, rock and honed-stone material families and the captured temple atlas.
Both room meshes and separate props are covered; characters, effects and
additive light draws are excluded. Specular, environment sheen, emission-map
amplification and screen-space reflectivity are zero. A diffuse highlight
shoulder retains values up to 0.70 and rolls higher illumination toward 0.80.
The earlier static-prop vertex-bake correction remains active.

The slow launch recorded 84.167 seconds to first presentation, with 82.212
seconds inside graphics pipeline compilation. Other recorded launches using
the prepared cache took about 1.3–2.8 seconds to first presentation. The local
shader updates require new cache entries, so prepare the matching cache before
the next test launch instead of leaving that work to AO's startup.

The common windowed device path now requests startup progress. An earlier
headless Begin can be promoted without resetting the elapsed time or stage.
The independent progress window has a taskbar entry, stage/count text and
elapsed time, and logs its creation. It still suppresses itself on fast starts.

## Validation and handoff

- `build/startup-lighting-fix/verification/RESULTS.json`: authored studio
  reference differs by at most one 8-bit colour level; the matte stone's
  PBR-on/off comparison also differs by at most one level. UI colours agree.
- The first test initialization took 74.1 seconds with changed shaders;
  subsequent processes using the saved cache took about 2.7 seconds. These are
  controlled renderer tests, not promises about retail loading or zoning time.
- `build/startup-lighting-progress-check.log`: responsive delayed window,
  headless-to-visible promotion, taskbar visibility, fast-start suppression and
  the error/close paths passed.
- `presentation_policy_check`: login/unknown/world/logout/device-reset cases.
- The isolated install and revert preserve the selected Full edition, settings,
  original client files and texture cache.

`build/startup-lighting-fix/candidate` contains the matching renderer/helper.
`verification/pipelines.cache` was prepared on this user's current GPU and
driver, seeded from the existing per-user cache. `VERIFIED.json` records its
hash. The F: client must be closed before the renderer is replaced. Preserve a
backup of the previous per-user cache before atomically installing the prepared
copy. Do not distribute this GPU/driver-specific cache with a public package.

`tools/install-startup-lighting-trial.ps1` applies/reverts the renderer in the
F: test client only. A fresh in-game comparison remains necessary to confirm
the user's visual preference for the hall and stone temple.

The renderer apply/revert fixture and atomic cache replacement fixture both
passed. The revised build and matching GPU cache were subsequently installed
in the F: test client with the user's approval. Installed file hashes and the
renderer receipt match `VERIFIED.json`; renderer, receipt and previous GPU
cache backups were verified. `INSTALL-RESULTS.json` and `CACHE-INSTALL.json`
record the exact backup locations. Settings and prepared textures were
preserved. The game was not launched by the installer; a fresh in-game
comparison is still pending.
