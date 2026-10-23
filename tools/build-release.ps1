param(
    [string]$VulkanSdk = $env:VULKAN_SDK,
    [string]$ClangCl = '',
    [string]$LldLink = '',
    [string]$Glslc = '',
    [string]$SpirvCross = '',
    [string]$ResourceDir = '',
    [string]$BuildDir = '',
    [string[]]$Targets = @('ao_vk_live', 'ao_vk_loader', 'loader_check'),
    [switch]$AllowOlderClang,
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$BuildDir) { $BuildDir = Join-Path $root 'build\windows-release' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio Build Tools with Desktop development with C++ and a Windows SDK.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'No Visual Studio C++ toolset found.' }
& (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch x86 -HostArch amd64 -SkipAutomaticLocation
function Find-Tool($Explicit, $Name, $Fallback) {
    if ($Explicit) { return (Resolve-Path -LiteralPath $Explicit).Path }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if ($Fallback -and (Test-Path -LiteralPath $Fallback)) { return $Fallback }
    throw "Cannot find $Name. Install it or pass its path explicitly."
}
$cmake = Find-Tool '' 'cmake.exe' (Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
$ninja = Find-Tool '' 'ninja.exe' (Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe')
$ClangCl = Find-Tool $ClangCl 'clang-cl.exe' (Join-Path $env:ProgramFiles 'LLVM\bin\clang-cl.exe')
$LldLink = Find-Tool $LldLink 'lld-link.exe' (Join-Path (Split-Path -Parent $ClangCl) 'lld-link.exe')
if (!$VulkanSdk -or !(Test-Path -LiteralPath (Join-Path $VulkanSdk 'Include\vulkan\vulkan.h'))) {
    throw 'Set VULKAN_SDK or pass -VulkanSdk with a Vulkan SDK installation containing Include\vulkan\vulkan.h.'
}
$Glslc = Find-Tool $Glslc 'glslc.exe' (Join-Path $VulkanSdk 'Bin\glslc.exe')
$SpirvCross = Find-Tool $SpirvCross 'spirv-cross.exe' (Join-Path $VulkanSdk 'Bin\spirv-cross.exe')
$flags = '--target=i686-pc-windows-msvc /EHsc /GR'
if ($AllowOlderClang) { $flags += ' /D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH' }
if ($ResourceDir) { $flags += ' -resource-dir="' + $ResourceDir + '"' }
# An initial cache preserves embedded compiler-option quotes under Windows PowerShell 5.1 as well as pwsh.
New-Item -ItemType Directory -Force $BuildDir | Out-Null
$initialCache = Join-Path $BuildDir 'ao-vk-initial-cache.cmake'
$cacheText = 'set(CMAKE_CXX_FLAGS [==[' + $flags + ']==] CACHE STRING "Compiler flags" FORCE)'
[IO.File]::WriteAllText($initialCache, $cacheText)
& $cmake -C $initialCache -S $root -B $BuildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$ClangCl" `
    "-DCMAKE_LINKER=$LldLink" "-DCMAKE_AR=$env:VCToolsInstallDir\bin\Hostx64\x86\lib.exe" `
    '-DCMAKE_BUILD_TYPE=Release' "-DVULKAN_SDK_DIR=$VulkanSdk" "-DGLSLC=$Glslc" "-DSPIRV_CROSS=$SpirvCross"
if ($LASTEXITCODE) { throw 'Configure failed.' }
if ($ConfigureOnly) { return }
& $cmake --build $BuildDir --target @Targets --parallel 6
if ($LASTEXITCODE) { throw 'Build failed.' }
Write-Output "Built x86 live binaries in $BuildDir"
