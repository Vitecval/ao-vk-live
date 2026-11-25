# Native display discovery and startup policy

The old DirectDraw mode path offered twelve fixed dimensions, clipped them to the primary desktop and reported60Hz. Its appended desktop entry bypassed the caller's filters. The native AO descriptor then stored mode dimensions in16-bit fields without validation, retained duplicate refresh records and stopped at128entries. Those limits could hide4K/ultrawide choices or corrupt oversized input.

`proxy/display_mode_policy.h` now owns dimension/format safety, exact filter matching, refresh-aware uniqueness, deterministic ordering, fallback and legacy-array priority. `proxy/display_mode_win32.h` reads the selected monitor's actual modes through `EnumDisplaySettingsExW` with flags0, adds its current mode through the same filters, and resolves monitor preference → AO window monitor → primary. It never changes the desktop. The virtual AO surface formats remain16/32-bit; they do not request a desktop bit-depth switch. Default enumeration deduplicates dimensions/format across refreshes, while `DDEDM_REFRESHRATES` retains distinct reported rates, as required by [DirectDraw7 enumeration](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdraw7-enumdisplaymodes). Monitor selection and current settings follow [the Windows discovery API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsexw).

Native `DeviceDesc` remains exactly0x834bytes with128six-byte mode records. Its callback rejects nonrepresentable dimensions and unsupported formats, deduplicates before consuming slots, and retains current/common/high-resolution choices if the array fills. The public startup size validator uses the full reported monitor inventory, independently of this bounded legacy menu. An active Vulkan device additionally clamps enumeration to its image/framebuffer limits. Before the GPU exists, integer/addressing and AO ABI bounds apply; actual creation still has to validate the device and available resources.

The shared restart-only section is:

```ini
[Display]
Mode=Game
Width=0
Height=0
Monitor=
```

Mode is Game, Windowed or Borderless. Both dimensions are zero for default sizing, or both are safe positive integers. Monitor is empty for automatic selection, or a Windows name such as `\\.\DISPLAY2`. Game with default values preserves the game's requests. Windowed custom dimensions are exact client pixels with a window frame added. Borderless always uses the selected monitor's current dimensions; a nonzero custom pair must match them, otherwise the override is rejected. No refresh override is offered because these modes retain the desktop refresh.

API for native initialization and surface creation:

- `StartupOptions(iniPath)` returns the process snapshot; `ReadOptions(iniPath)` is a fresh read for tests. `RequestedOptions` retains the original request for logging.
- `Discover(hwnd,options)` returns the relevant monitor, full inventory, current mode and whether the configured monitor was found.
- `SelectSize(options,gameWidth,gameHeight,monitor,limits)` returns actual candidate dimensions or preserves game dimensions with a rejection reason.
- `ApplyWindowOptions(hwnd,options,monitor,&previous)` applies the selected style/size once per real HWND. It measures client pixels after cross-monitor/DPI notifications and corrects the frame. A null initial cooperative-level window is not treated as a real window.
- `CaptureWindow`, `WindowState`, `AppliedWindowState` and `RestoreWindow` preserve/restore old style, frame and position for failure rollback. `DisableOverride(iniPath,reason)` falls back to Game for this process without rewriting the saved configuration.

Apply window changes during native initialization/cooperative-level handling, before surface dimensions are measured. Never apply them from WM_SIZE/WM_DPICHANGED. Existing callbacks continue deferring renderer resize; no GPU wait or COM lock is introduced into those callbacks. Renderer allocation failure and actual surface/viewport/input dimensions are checked separately by the owning presentation path.

`display_mode_policy_check` covers1080p/1440p/4K/ultrawide, correct desktop/filter behavior, refresh dedup, GPU clamps,128-slot overflow guards, strict startup parsing/restart snapshots and saved-choice preservation. It reads `tests/display-mode-options-cases.tsv`, shared with frontend validation. A read-only real-monitor discovery check and a private hidden-window exact-size/rollback check exercise Win32 without changing desktop state. These are isolated tests, not proof of live AO framebuffer dimensions or retail mouse/tooltip behavior.

Native `Randy_t::Initialize` applies startup window options before measuring client bounds, and explicit Windowed/Borderless use AO's windowed backbuffer path. An override allocation failure cleans the partial graphics attempt, restores the original window and retries once with the original AO mode; saved preferences remain unchanged. Resize stages the new colour/depth/required effect targets while retaining all seven old target pointers, size caches, depth/stencil values and emulation capability flags. It binds the replacement before releasing old targets. Failure restores those globals, old binding and the last working window. `framebuffer_transaction_check` injects construction, binding and allocation failures into the same transaction protocol to verify ownership order and rollback.

Legacy Game fullscreen still uses its original attached flip chain; live resize of that attached chain is refused safely, retaining its working framebuffer/window. The new explicit Windowed/Borderless path supports transactional replacement. This avoids pretending a mutated shared depth attachment can be rolled back by swapping only pointer arrays.

Source tracing confirms `Randy_t::GetBufferSize` reads target0's actual surface description, and `RViewPort::Update` obtains its target dimensions there before clamping the requested viewport. The seven `kWidths/kHeights` globals are per-target feature-size caches, not a universal output resolution. Construction now refreshes populated slots from their real descriptors (target0 at full size, targets5/6 at8×8) and retains256 defaults only for unallocated features. Unrecognized smaller/split viewport requests remain intentionally unchanged; the recognized full-main closure below handles the legacy initial AO size. Actual D3D viewport/camera/UI input coverage still needs integration validation.

The native main-layout closure now records the original AO request, original client size, current/previous framebuffer and latest client size. Under an active startup override, only an origin0,0 full primary/target0 viewport matching one of those full-size pairs is expanded. Arbitrary split rectangles and offscreen targets remain unchanged. Actual D3DVIEWPORT7 dimensions are logged alongside the target description and framebuffer dimensions.

The camera keeps its horizontal plane window/FOV and derives its vertical window/aspect from that effective main viewport. `PrepareDisplayCamera` runs before camera matrices/frustum setup, world-label clip projection and pixel pick rays, so these consumers share one window and viewport size. A bounded allocation-free record preserves the original camera window for offscreen/split reuse, cloning and serialization. `native_display_layout_check` exercises4K/ultrawide main-only mapping, unchanged split/offscreen/default cases and label/pick round trips using the same policy.

Native framebuffer resize inside `InsideWindowCallback` publishes only an atomic latest-size request tagged with Randy ownership/generation. Destruction cancels pending work; `DrainPendingResize` rechecks the owner and executes only outside callbacks and minimization. It must run from `RvkState::Frame` before BeginFrame. Native viewport callbacks avoid SetViewport COM too; merely deferring the framebuffer allocation would leave that reentrant path. Cooperative/display/restore mode calls retain their working state inside callbacks rather than applying a fallback mode there. No valid saved override is discarded because a callback happened. These source guarantees still need build, isolated execution and retail visual/input validation.
