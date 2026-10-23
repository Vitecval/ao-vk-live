# Building AO-VK Live on Windows

Players should use the prebuilt Windows release ZIP. These steps are for developers.

Install Visual Studio / Build Tools with **Desktop development with C++**, the Windows SDK, CMake and Ninja; LLVM with **clang-cl** and **lld-link**; Python 3; and the Windows Vulkan SDK (headers, `glslc`, `spirv-cross`). The SDK's `VULKAN_SDK` environment variable should point at its root. VMA and the FidelityFX source subsets are included in this repository.

From the source folder:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build-release.ps1
python tools/package-release.py --version 0.2.0-preview.3 --build-dir build/windows-release
```

The script detects Visual Studio with `vswhere`, enters the x86 compiler environment, and builds with clang-cl and LLD. The live binaries must be **32-bit**, even on 64-bit Windows. LLD is required to preserve the original decorated C++ export aliases. Do not substitute Microsoft's linker without rechecking all exports.

Tools outside PATH can be supplied with `-ClangCl`, `-LldLink`, `-VulkanSdk`, `-Glslc` and `-SpirvCross`. `-BuildDir` chooses the output folder. Use a recent LLVM compatible with your installed MSVC headers. `-ResourceDir` and `-AllowOlderClang` exist for the development machine's older, relocated compiler; they are not needed for a normal LLVM installation.

The initial distributed build used clang 19.1, LLD 15, Visual Studio 2026 headers, Vulkan headers 1.4.321, VMA 3.3.0, shaderc's 2026-10-21 Windows build and SPIRV-Cross commit `300681bbcb8365a8d1eacdacdbdd0249bbf1d6b1`. The SDK tool versions available on another machine can produce different DLL hashes.

Installer tests need no game files:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/install-live.tests.ps1
```

In a PowerShell session, the renderer's tests can be built with `& ./tools/build-release.ps1 -Targets @('ao_vk_live','ao_vk_loader','rvk_demo','live_check','loader_check','randy_harness')`. Native compatibility tests require a player's own supported game files in an isolated local test folder; see docs/live-client.md. Do not upload those files or test folders.
