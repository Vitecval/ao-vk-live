# Local login lighting and stability follow-up

The user accepted the previous temple and startup changes and requested a
smaller login-lighting adjustment plus a fix for visible vibration. This
unpublished 0.7.6 trial changes the login branch of the display shader only.

## Evidence and correction

The six November 19 captures from 16:35:42 through 16:35:49 have the same
camera position, 1914x1051 scene size and active login-studio lighting. The
static left plinth region still varies by about 2.30 eight-bit colour levels
across those captures; the static floor region varies by about 1.09 levels.
Animated characters and magic are excluded from these observations.

The login display path copied scene coverage into its output alpha. Temporal
anti-aliasing interprets that alpha as the amount of animated additive energy.
Opaque scenery therefore received the same 85% new-frame weight as sparks,
instead of accumulating detail with the normal 10% new-frame weight. This
exposed the scene's subpixel sampling offsets as vibration. The login path now
derives reactivity from the additive-energy buffer, as the world path already
does. Opaque panels can accumulate; actual additive magic remains reactive.

A small white-balance correction removes 15% of the shared green/blue excess
over red. A smooth highlight shoulder above 0.85 preserves brighter detail
without pushing the studio surrounds toward flat white. This operates before
the later interface composition and does not change saved preferences.

## Validation

`tests/login_stability_render.py` compares 16 late frames of the same stationary
studio detail with temporal anti-aliasing and sharpening enabled. Mean temporal
standard deviation fell from 2.0877 to 0.3165 colour levels, a reduction of
84.84%. This is a controlled renderer result, not an in-game motion assessment.

The colour-panel comparison changes channels by at most 12 of 255 levels.
Interface colours, the world-lighting control and the previously accepted
matte-temple output are byte-for-byte identical. The build, image comparisons,
installer apply/revert fixture and atomic GPU-cache replacement fixture passed.

The staged renderer SHA-256 is
`265bf141c3446accb5ee56716443c38aaa54a144ec4c18d5b7d89a743a1a8d62`.
`build/login-stability-fix/VERIFIED.json` records the matching helper, prepared
GPU cache and checks. The cache includes the preceding build's entries and is
specific to this user's current GPU and driver.

Apply/revert uses `tools/install-startup-lighting-trial.ps1` with
`-Trial login-stability-fix`. The closed F: client is the sole retail target;
the installer retains the renderer and receipt, preserves settings and texture
assets, and never launches the game. `build/login-stability-fix/install-cache.ps1`
atomically replaces the matching per-user GPU cache with a verified backup.

After the F: client exited, the renderer and prepared GPU cache were installed.
Installed hashes, the installation receipt and both previous-file backups were
verified. `INSTALL-RESULTS.json` and `CACHE-INSTALL.json` record the backup
locations. Settings and textures were preserved. A fresh in-game comparison
is still needed to confirm the user's preference.
