# Horizon 0.7.4 / Preview 24 r61

Routine updates reuse an asset verification index for unchanged artwork. The
first update creates it after checking the files. Changed or missing files are
checked normally; replacements, removals and uninstall still verify ownership
from the actual bytes. Repair always checks every asset. Cached verification
expires after 30 days and does not extend itself on each quick update.

The index is tied to the installation receipt and written atomically after a
successful installation. A missing, damaged, incompatible or stale index causes
full verification and rebuilding. File size, modification time and creation time
are acceleration hints, not a substitute for Repair's corruption check.

Setup progress stays above the page while preparing, recovering an interruption,
checking existing files, installing and cleaning temporary files. It shows real
phase counts and elapsed time, identifies waits without claiming progress, and
offers the setup log directly. The previous attempt's log is kept. A blocked
graphics-preparation helper produces an actionable error naming the executable.

Minimize sends Horizon to the system tray. Click the icon to restore the same
window; its menu also opens the setup log or exits. Active installations keep
their existing exit guard and continue while minimized.

Graphics includes a searchable Keybinds subsection explaining the fixed graphics
shortcuts, in-game options controls, captures, diagnostic tools, free camera,
console and travel-screen input. This help is available before selecting AO.

Vivid Tone is optional and off by default. It adapts Barbatos's MIT-licensed
VividTone 1.2.1 into native Vulkan passes, with overall strength, gamma contrast,
exposure, shadow lift, adaptive contrast, static exposure and adaptation speed.
It runs before AO's interface, leaving text and menus unchanged. No ReShade
installation is required. Presets preserve this separate choice. Attribution
and license are included in licenses/Barbatos-VividTone.txt.

The login and character-selection scene keeps its authored lighting rather than
receiving live-world brightness compensation and atmosphere during tone mapping.
The user's world settings remain saved and resume in the live world. The native
window restore path handles WM_SIZE without waiting on the renderer lock and
does not treat the minimized 154 x 10 client rectangle as a restored game window.

The AO graphics-preparation window explains that AO opens when preparation
finishes, shows elapsed time, and stays above the active client's black window
without taking focus. The installer now ships the helper built with this
renderer's shaders, rather than the older historical helper.

All 0.7.3 terrain, rough masonry, render-scale and zoning-screen fixes remain.
Existing Full HD companions are reused. Reworked armor and creature artwork is
private staging and is not included in this release.

Validation includes real Vulkan color-chart comparisons against an independent
Vivid Tone reference, unchanged UI colors, zero-strength/off restoration,
exposure adaptation, a real cross-thread window-lock/restore regression, startup
window responsiveness, asset-index failure/recovery scenarios, installer
rollback/ownership checks and tray minimize/restore cycles. Live confirmation in
the reported game scenes remains a separate playtest after updating.
