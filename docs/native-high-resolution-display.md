# Native high-resolution display controls

The upcoming release adds Horizon's **Display** page for each selected AO
installation. It sets the native game window and framebuffer size, with the full
main viewport following that size. It does not obtain a larger output by simply
stretching a smaller final image.

1. Select the intended AO folder in Horizon.
2. Open **Display** and choose **Windowed**, **Borderless**, or the default
   **Use game setting**.
3. For a custom window size, choose an attached monitor, then a size from its
   reported list. Automatic monitor selection keeps game sizing.
4. Select **Save display settings**, then fully close and restart AO and its
   launcher. Display choices are read at startup, rather than applied live.

**Windowed** uses the chosen pixel dimensions for the window's client area;
the window frame is additional. Sizes larger than the work area may not fit on
screen. **Borderless** fills the selected monitor at its current desktop size.
**Use game setting** returns window and resolution selection to AO. The mode
list comes from the relevant monitor, removes duplicate refresh variants and
respects native format/dimension limits. It offers 1080p, 1440p, 4K or ultrawide
only when the chosen monitor reports those sizes. Final allocation also depends
on GPU limits and available resources.

These settings never change the Windows desktop resolution, refresh rate or
scaling. There is no exclusive-refresh override. Each AO folder keeps its own
choices in `randy-vk.ini`, independently of named graphics presets, sound options
and other installations:

```ini
[Display]
Mode=Game
Width=0
Height=0
Monitor=
```

`Mode` accepts `Game`, `Windowed` or `Borderless`. Both dimensions are zero for
default sizing, or both are positive integers. A nonzero Borderless pair must
match the selected monitor's current size; Horizon saves zero for that mode.
`Monitor` is empty for automatic selection or a Windows device name such as
`\\.\DISPLAY2`. Horizon shows unavailable saved monitors/sizes without silently
replacing them. **Refresh monitors** rereads the saved choices and current
inventory; it discards unsaved edits on the page.

## Render resolution and readable interface

Display resolution is the native output pixel size. **Graphics → Render
resolution** (`RVK_RenderScale`) controls the 3D scene's internal resolution
separately. Increasing it does not select another monitor mode. Start at 100%
when checking native output; higher internal resolution costs more GPU work.

Independent AO interface scaling is not implemented. Text and buttons can look
smaller at higher native output sizes. Native camera aspect/projection,
world-label projection and mouse-picking paths use the effective main viewport;
that shared sizing does not constitute a scalable interface or a guarantee that
every legacy panel remains readable. A live client check of text, tooltips,
world labels and picking is still required for the intended monitor and size.

## Monitor changes, DPI and recovery

AO's existing window DPI awareness is preserved. The override requires a
confirmed physical-pixel window: a per-monitor-aware window or a legacy window
whose coordinates and monitor scaling agree. A bitmap-scaled legacy window,
incompatible mixed-DPI monitor or unknown DPI state falls back to the working
game path. A later monitor/DPI change can disable the override for that run;
the saved preference remains available for the next startup. This release does
not turn a legacy AO window into an independently scaled high-DPI interface.

Invalid settings, a disconnected selected monitor, unsupported dimensions or a
failed window/framebuffer change retain or restore the working game
configuration for that run. The renderer records the rejection reason rather
than deleting the saved choice. Choose **Use game setting**, save and restart
to explicitly return to the default. A window move or resize is deferred outside
window callbacks before renderer work; minimization does not allocate a zero-size
framebuffer. Legacy Game fullscreen retains its original flip-chain behavior;
the new transactional resize path applies to explicit Windowed/Borderless.

## Scope and verification

This is AO-specific Vulkan support, not a universal dgVoodoo replacement or a
claim of compatibility with other DirectDraw games. Do not stack competing
`ddraw` wrappers in the AO folder. Display preferences do not install or replace
wrappers, and unrelated/unknown INI sections remain intact.

Horizon's isolated WPF checks exercise the real controls and two dummy client
folders, including per-folder persistence, monitor changes, 4K/ultrawide
selections from a controlled inventory, restart notices and preservation of
unrelated preferences. Read-only real-monitor discovery is separate; the
development machine's two monitors currently report up to 1920×1080. Higher
resolution fixture success is not a claim of physical 4K/ultrawide monitor
verification.

Native policy, hidden-window, framebuffer rollback and viewport/camera fixtures
are separate from live AO checks. Actual game framebuffer dimensions,
minimize/restore, Alt-Tab, mixed-DPI moves, tooltips, labels, picking and enabled
TAA/upscaling/frame generation need their own recorded validation. See
[native implementation details](display-mode-discovery.md) for the shared
discovery/startup contract and test boundaries.

Current controlled GPU checks verify native scene/output readbacks through 4K
and ultrawide, exact interface markers, projection, TAA and FSR 2/3 execution.
Fifteen injected host/GPU allocation failures retain actual old pixels and
resource ownership before successful recovery. These checks do not load AO.

Frame generation remains experimental. The window lifecycle fixture completes
its resize/fallback/re-enable sequence, but x86 synchronization validation reports
`SYNC-HAZARD-WRITE-AFTER-PRESENT` in the SDK presentation path. The same hazard
reproduces in the preserved r68 build with implicit overlays disabled. This
update does not claim to fix it or provide clean frame-generation validation.
Headless frame-generation requests explicitly fall back to normal rendering.
