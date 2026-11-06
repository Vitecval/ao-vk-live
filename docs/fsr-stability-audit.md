# FSR stability audit — revision 10

The earlier full-game hangs are still unresolved. Revision 9 was not tested in the game before this audit. This revision hardens confirmed failure paths and GPU requirements; it does not establish that the driver presentation hang is fixed.

## What the freeze evidence establishes

Both archived revision-8 game captures stop in Windows/NVIDIA presentation: the window owner is in `NtGdiDdDDIPresent`, the SDK presenter awaits its synchronous handoff, the interpolation thread awaits GPU work, and the render worker has stopped after its timeout. The FSR upscaler setting was zero in these captures: the implicated feature is frame generation, which is independent of FSR upscaling.

The two affected processes had entered termination but retained an unsignaled process/thread handle. Standard termination and native termination requests could not finish the pending kernel work. Windows records an unclean reboot at 2026-11-06 19:14:30 local time. The audit found no captured driver/kernel dump that proves the internal driver cause. A user-space timeout cannot cancel a driver call already blocked in the kernel.

Twitch/RTSS were absent in the second actual-game freeze. They therefore are not necessary for that freeze. The NVIDIA presentation layer was not isolated in that live test. Do not claim that an overlay caused the full-game hang, or that the absence of validation errors proves the driver cannot hang.

## Confirmed defects fixed

- Optical-flow and frame-interpolation pipeline helpers returned errors that their callers ignored. A failed compute pipeline could leave context creation reporting success. All helper and binding-patch results now propagate.
- FSR 2 and the three FSR 3 components leaked partial initialization on resource, capability or pipeline failure. Each component tracks successful backend ownership, including context ID zero, and unwinds its partial state. The outer FSR 3 context unwinds completed components and shared resources in reverse order.
- The copy-resource release helper added one to a null resource index, potentially releasing another allocation during partial cleanup. It now ignores null bases.
- Both Vulkan backends leaked temporary shader modules on compute pipeline failure. Partial pipeline layouts are now exposed to cleanup before the first allocation, and descriptor cleanup skips unallocated sets.
- Initial Vulkan backend allocations now unwind when uniform buffers, memory mapping or binding fail. The FSR 3 backend destroys its live mutex before resetting its storage.
- FSR 2 ignored failed descriptor allocations and subsequently updated invalid handles. The Vulkan fault test reproduced an access violation. These results now propagate in both backends.
- GPU selection requires graphics and compute in the selected queue family. FSR checks compute subgroups, quad operations, extended storage images and the workgroup envelope extracted from the shipped shaders (up to 256 invocations, 256 in X, 16 in Y, 4 in Z). FSR 2 rejects native subgroup sizes other than 32/64; FSR 3 is checked separately. Frame generation additionally requires subgroup arithmetic, timeline semaphores and three independent queues. Unsupported paths retain normal presentation/TAA and log a reason.
- Required wave64 compute subgroup sizes are requested only when the feature and compute-stage support exist. The renderer enables supported formatless storage image features. Per-format compatibility still needs real-device validation; Vulkan 1.3 can expose formatless access through format feature flags.
- Frame generation remains off by default and is identified as experimental in settings.

No new driver/overlay exclusions, global driver settings, registry changes, GPU reset, or game launch were performed. The revision-9 presentation ordering work remains in place.

## Evidence and limitations

Compiled SPIR-V audit: 116 shader modules. All shipped variants use FP32; no module declares Float16 or Int16 capability. Filenames/permutation aliases alone do not establish the shader's requirements.

CPU host fault audit: the initial two production modes had 103 creation steps; 101 injected failures leaked or were swallowed. After fixing them, an expanded audit covers 289 failure points across upscaling-only, interpolation-only, combined, and asynchronous-context configurations, including capability failures and another context's canary resource. All pass. These are mocked backend calls, not simulated GPU execution.

Hardware policy audit: 31 synthetic capability profiles pass, including subgroup 16/32/64 and one/two/three queues. This checks fallback decisions and is not AMD/Intel/NVIDIA hardware certification.

Real Vulkan failure audit: the original pipeline-failure test reported 419 leaked GPU objects at device destruction. The corrected test reports zero. Rejecting descriptor allocation reproduced an access violation before the check was added; the corrected run exits normally without validation errors.

Validation uses the official Khronos nightly Windows x64 layer in an isolated x64 renderer fixture, with the same runtime source and shader bytes. The game DLL and primary lifecycle regressions are x86. Validation binaries are local diagnostics only; nothing was installed globally. Implicit graphics layers are disabled only in diagnostic child processes to obtain attributable results. With Twitch/RTSS enabled, the fixture reports null-render-pass framebuffer calls and leaked semaphores even without FSR; disabling NVIDIA's presentation layer alone does not remove these reports, while disabling Twitch/RTSS does. These layer anomalies are distinct from the actual-game freeze that also occurred without those overlays.

Final checks: 17 x64 core/synchronization validation scenarios exit successfully with zero validation errors; all 12 x86 GPU lifecycle/regression scenarios pass, including actual generated presentations, acquisition timeouts, delayed presentation, resize/replacement and owner message handoff. The four sent-message wait cases and 12 render-worker failure cases pass.

Final validation/regression receipts are in `build/preview23/checks/fsr-stability-audit`. The actual revision-10 full-game test and physical AMD/Intel/additional NVIDIA testing remain pending.

## Release assessment

Keep frame generation experimental and opt-in. Do not label this build stable across GPU vendors or distribute it as a verified frame-generation fix based on one RTX 2070 SUPER and short fixtures. Before broader release, verify long sessions in the actual x86 game on NVIDIA, AMD and Intel hardware, including native and scaled resolution, rapid mode changes, resize/fullscreen/Alt-Tab, minimization, multiple clients, overlays and constrained memory. Present completion/resource retirement and the shared queue locking still warrant review if the real-game stall reproduces; a rendering fence alone does not prove presentation retirement.

Primary references: [FSR frame generation and upscaling](https://gpuopen.com/fidelityfx-super-resolution-3/), [Vulkan presentation semaphore reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html), [Vulkan swapchain recreation and present fences](https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html), [Khronos validation layers](https://github.com/KhronosGroup/Vulkan-ValidationLayers).
