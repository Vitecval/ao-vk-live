# Preview23 revision 39 - loading artwork and zoning history reset

Local follow-up to r38. No published downloads or retail clients were changed in this session.

## Behavior

Quick Controls includes **Zoning screen** (`RVK_ZoneScreen`), enabled by default and whenever any of the four presets is applied. Turning it off persists across restart; explicit temporal resets still run.

Initial login loading selects Frontier Bridge. Subsequent transitions rotate through Frontier Bridge, The Ring and ICC Grid. Username/password and character selection remain unchanged. The three screenshot-based paintings remove floating names, use restrained AO branding, and retain shadow and highlight detail. Source artwork is in `artwork/loading`; the installed copies go in `randy-vk/loading`.

A verified N3 teleport-status entry starts the screen before the loading call. An independent Windows popup thread keeps the image available while the game thread stalls. It does not activate or take input, and suppresses itself for hidden, minimized or background clients. The engine-status getter also catches a loading flag set before the hook was installed. Unknown client builds or unexpected instruction bytes disable the hook safely.

At loading start/end, the render worker resets camera reprojection, TAA, cloud history, upscaler inputs, object motion and depth-of-field focus. Reveal waits for four successfully presented world frames after the final reset. A 90-second timeout prevents an indefinitely stuck overlay. Artwork is aspect-fitted rather than stretched.

Shared Ground+ now uses effective pebble density **0.5**, reduced from 1.0. GroundOn retains its own density behavior. r38 curved cloud horizon improvements are included.

## Verification

The x86 renderer, demo, settings checker, zoning policy and zoning window fixtures build successfully. Frozen renderer SHA256: `1ca8044171d5982a37a9ae9982c949db5f1a41e5a463833d0f68ccdd34378170`.

Evidence is in `build/preview23/atmosphere-r39`:

- `checks/RESULTS.json`: sky/ambient regressions passed.
- `stability-checks/RESULTS.json`: camera offsets, retail cloud-sheet replacement, static TAA and horizon regressions passed.
- `ZONING-CHECK.json`: reveal policy, hidden/background popup, clean shutdown and artwork exposure checks passed. Mean image luminance is 88.1, 93.4 and 74.7 out of 255; clipped highlights stay below 0.003%.
- Settings checker passed default-on for every preset, toggle persistence and effective Shared Ground+ pebble density.
- `INSTALLER-CHECK.json`: actual installer passed under Windows PowerShell 5.1 against project-local dummy files, preserving unrelated settings/receipt entries and checking all installed artwork hashes.
- `PAYLOAD.json`: exact renderer and artwork hashes.

The window fixture does not prove retail login/zoning timing, fullscreen behavior or responsiveness during real loads. Check initial login, several consecutive zones, Alt-Tab during loading, the toggle and reduced pebbles in game.

## Installation

With the F: game and launcher closed, run `tools/Install-Preview23-r39.cmd` locally. It targets only `F:\AO_SInstall\Anarchy Online`, verifies all validation records and payload hashes, backs up replaced files, installs the artwork and renderer, updates receipt hashes and enables the setting. Artwork replacements are rolled back if installation fails.

F: is outside this session's writable paths, so the retail installation was not attempted. All `C:\Funcom` clients remain untouched.
