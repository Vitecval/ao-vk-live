param([string[]]$Targets = @('ao_vk_live', 'ao_vk_loader', 'randy_harness', 'native_check', 'live_check', 'loader_check', 'client_build_check'))
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vs = 'C:\Program Files\Microsoft Visual Studio\18\Community'
& "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch x86 -HostArch amd64 -SkipAutomaticLocation
$cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$clang = 'C:\Program Files\dotnet\packs\Microsoft.NET.Runtime.Emscripten.3.1.56.Sdk.win-x64\10.0.12\tools\bin\clang.exe'
$deps = Join-Path $root 'build\deps'
$clangCl = Join-Path $deps 'clang-cl.exe'
if (!(Test-Path -LiteralPath $clangCl)) { Copy-Item -LiteralPath $clang -Destination $clangCl }
$lld = Join-Path $deps 'lld-link.exe'
if (!(Test-Path -LiteralPath $lld)) {
    Copy-Item -LiteralPath 'C:\Program Files\dotnet\packs\Microsoft.NET.Runtime.Emscripten.3.1.12.Sdk.win-x64\7.0.20\tools\bin\lld.exe' -Destination $lld
}
$resource = & $clang -print-resource-dir
$sdk = Join-Path $deps 'Vulkan-Headers-1.4.321'
New-Item -ItemType Directory -Force "$sdk\include\vma" | Out-Null
$vmaTarget = "$sdk\include\vma\vk_mem_alloc.h"
if (!(Test-Path -LiteralPath $vmaTarget) -or (Get-FileHash -LiteralPath $vmaTarget).Hash -ne (Get-FileHash -LiteralPath "$deps\vk_mem_alloc.h").Hash) {
    Copy-Item -LiteralPath "$deps\vk_mem_alloc.h" -Destination $vmaTarget -Force
}
$common = @('-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_CXX_COMPILER=$clangCl", "-DCMAKE_AR=$env:VCToolsInstallDir\bin\Hostx64\x86\lib.exe", "-DCMAKE_LINKER=$lld", "-DCMAKE_CXX_FLAGS=--target=i686-pc-windows-msvc /EHsc /GR /D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -resource-dir=`"$resource`"", '-DCMAKE_BUILD_TYPE=Release')
if (!(Test-Path -LiteralPath "$deps\spirv-build\spirv-cross.exe")) {
    & $cmake --fresh -S "$deps\SPIRV-Cross-main" -B "$deps\spirv-build" @common "-DCMAKE_C_COMPILER=$clangCl" "-DCMAKE_C_FLAGS=--target=i686-pc-windows-msvc -resource-dir=`"$resource`"" '-DSPIRV_CROSS_ENABLE_TESTS=OFF' '-DSPIRV_CROSS_ENABLE_C_API=OFF'
    if ($LASTEXITCODE) { throw 'SPIRV-Cross configure failed' }
    & $cmake --build "$deps\spirv-build" --target spirv-cross --parallel 6
    if ($LASTEXITCODE) { throw 'SPIRV-Cross build failed' }
}
& $cmake --fresh -S $root -B "$root\build\windows-live" @common "-DVULKAN_SDK_DIR=$sdk" "-DGLSLC=$deps\shaderc\bin\glslc.exe" "-DSPIRV_CROSS=$deps\spirv-build\spirv-cross.exe"
if ($LASTEXITCODE) { throw 'AO-VK configure failed' }
& $cmake --build "$root\build\windows-live" --target @Targets --parallel 6
if ($LASTEXITCODE) { throw 'AO-VK build failed' }
