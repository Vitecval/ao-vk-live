param(
    [Parameter(Mandatory=$true)][string]$ClientDir,
    [string]$PackageDir = $PSScriptRoot,
    [string]$LoaderDir = '',
    [switch]$Upgrade,
    [switch]$Uninstall,
    [switch]$DisableLargeAddressAware,
    [scriptblock]$ProgressCallback
)
$ErrorActionPreference = 'Stop'
# Prefer this PowerShell host's built-in modules when launched from another host.
$env:PSModulePath = (Join-Path $PSHOME 'Modules') + ';' + $env:PSModulePath

# Match the 32-bit game: 64-bit Windows PowerShell can return only the WOW64
# loader modules, hiding randy31.dll and falsely identifying another client.
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $host32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    if (!(Test-Path -LiteralPath $host32)) { throw 'Cannot find 32-bit Windows PowerShell for safe AO process inspection.' }
    $hostArgs = @('-NoLogo', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    foreach ($key in $PSBoundParameters.Keys) {
        $value = $PSBoundParameters[$key]
        if ($value -is [Management.Automation.SwitchParameter]) {
            if ($value.IsPresent) { $hostArgs += "-$key" }
        } else { $hostArgs += @("-$key", [string]$value) }
    }
    & $host32 @hostArgs
    if ($LASTEXITCODE -ne 0) { throw "AO-VK installer failed in 32-bit PowerShell (exit $LASTEXITCODE)." }
    return
}
$client = (Resolve-Path -LiteralPath $ClientDir).Path
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
if (!$Uninstall -and $manifest.edition -eq 'full' -and $manifest.artworkLicense) {
    Write-Host 'Free to use with any compatible launcher. Redistribution of the HD artwork is not permitted without authorization.'
    Write-Host 'See LICENSE-TEXTURES.md. Third-party licenses and permissions previously granted remain in force.'
}
if (!$Uninstall -and $manifest.assets) {
    foreach ($item in $manifest.assets.PSObject.Properties) {
        if ($item.Name -match '^randy-vk[\\/]living[\\/]') {
            throw 'Living Rubi-Ka assets must come from a validated local client provider, not this package.'
        }
    }
}
if (!$Uninstall -and $manifest.livingAssets) {
    if ($manifest.livingAssets.provider -ne 'installation-local-v1' -or $manifest.livingAssets.status -ne 'local-test-candidate') {
        throw 'This candidate has no recognized Living Rubi-Ka asset provider.'
    }
    $livingHelper = Join-Path $package 'living-preparer\ao-vk-living-prepare.exe'
    if (!(Test-Path -LiteralPath $livingHelper) -or (Get-FileHash -LiteralPath $livingHelper -Algorithm SHA256).Hash -ine $manifest.livingAssets.helperSha256) {
        throw 'Local Living preparation helper integrity failed.'
    }
    foreach ($item in $manifest.livingAssets.files.PSObject.Properties) {
        if ($item.Name -notmatch '^(living-preparer/[A-Za-z0-9_./-]+|living-metadata/[A-Za-z0-9_./-]+)$' -or $item.Name.Contains('..') -or
            !(Test-Path -LiteralPath (Join-Path $package $item.Name)) -or
            (Get-FileHash -LiteralPath (Join-Path $package $item.Name) -Algorithm SHA256).Hash -ine $item.Value) {
            throw 'Local Living preparation payload integrity failed.'
        }
    }
}
$receiptPath = Join-Path $client 'ao-vk-live.install.json'
$receipt = $null
if (Test-Path -LiteralPath $receiptPath) {
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    if ($receipt.product -ne 'AO-VK Live' -or $receipt.clientDirectory -ine $client) {
        throw 'The existing AO-VK install record does not belong to this folder.'
    }
    if (!$LoaderDir -and $receipt.loaderDirectory) { $LoaderDir = [string]$receipt.loaderDirectory }
}
$targets = @((Join-Path $client 'ao-vk.dll'), (Join-Path $client 'ddraw.dll'))
if (!$LoaderDir -and !$receipt -and (Test-Path -LiteralPath (Join-Path $client 'MSWSOCK.dll'))) {
    $candidate = Join-Path $env:APPDATA 'AOPP'
    if (Test-Path -LiteralPath (Join-Path $candidate 'AnarchyOnline.exe')) { $LoaderDir = $candidate }
}
$loader = ''
if ($LoaderDir) {
    $loader = [IO.Path]::GetFullPath($LoaderDir)
    if (!$Uninstall -and !(Test-Path -LiteralPath (Join-Path $loader 'AnarchyOnline.exe'))) { throw 'The extra loader folder must contain AnarchyOnline.exe.' }
    if ($loader -ine $client) { $targets += Join-Path $loader 'ddraw.dll' }
}
$fileSpecs = @{}
foreach ($target in $targets) {
    $name = Split-Path -Leaf $target
    $fileSpecs[$target] = @{ relative = $name; hash = $manifest.addon.$name; oldHash = $(if ($receipt) { $receipt.addon.$name }) }
}
# Asset paths are deliberately restricted to this optional pack. Never trust a manifest
# or install record to write/remove an arbitrary path outside its reserved directory.
$assetHashes = [ordered]@{}
if ($receipt -and $receipt.assets) {
    foreach ($item in $receipt.assets.PSObject.Properties) { $assetHashes[$item.Name] = [string]$item.Value }
}
if ($manifest.assets) {
    foreach ($item in $manifest.assets.PSObject.Properties) { $assetHashes[$item.Name] = [string]$item.Value }
}
foreach ($relative in $assetHashes.Keys) {
    if ($relative -notmatch '^randy-vk/(packs/ao-((modern|cyber)-icc|cyber-characters|cyber-world)/(source-art/)?[A-Za-z0-9_-]+\.(png|json|md)|living/[A-Za-z0-9_.-]+\.(lrs|lrm|lrg|ini|txt|json))$' -or $assetHashes[$relative] -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'Invalid optional material pack path or hash.'
    }
    $target = Join-Path $client $relative
    $newHash = $(if ($manifest.assets) { $manifest.assets.$relative })
    $oldHash = $(if ($receipt -and $receipt.assets) { $receipt.assets.$relative })
    $fileSpecs[$target] = @{ relative = $relative; hash = $newHash; oldHash = $oldHash; asset = $true; retire = (!$newHash -and !$Uninstall) }
    $targets += $target
}
$memoryPlan = @{ records=@(); changes=@(); warnings=@(); status='Left unchanged' }
if ($manifest.largeAddressAware -or $receipt.largeAddressAware) {
    . (Join-Path $PSScriptRoot 'large-address-aware.ps1')
    $memoryPlan = Get-AoLaaPlan $client $loader $manifest.largeAddressAware $receipt (!$DisableLargeAddressAware) ([bool]$Uninstall)
    foreach ($change in $memoryPlan.changes) {
        if ($fileSpecs.ContainsKey($change.target)) { throw 'Duplicate executable memory-patch target.' }
        $fileSpecs[$change.target] = $change
        $targets += $change.target
    }
}
function Test-OwnedHash($Target, $Hash) {
    $spec = $fileSpecs[$Target]
    if ($Hash -ieq $spec.hash -or ($spec.oldHash -and $Hash -ieq $spec.oldHash)) { return $true }
    if (!$spec.asset) {
        $name = Split-Path -Leaf $Target
        foreach ($previous in $manifest.previousAddons) { if ($Hash -ieq $previous.$name) { return $true } }
    }
    return $false
}
$unchangedTargets = @()
$changingTargets = @()
$checkedTargets = 0
foreach ($target in $targets) {
    $checkedTargets++
    if ($ProgressCallback -and $checkedTargets % 250 -eq 0) { & $ProgressCallback "Checking existing files: $checkedTargets / $($targets.Count)" -1 }
    $name = Split-Path -Leaf $target
    if (!$Uninstall -and (Test-Path -LiteralPath $target) -and
        (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ieq $fileSpecs[$target].hash) {
        $unchangedTargets += $target
    } else { $changingTargets += $target }
}
function Assert-AoClientClosed {
foreach ($name in @('AnarchyOnline', 'Anarchy', 'AnarchyPatcher', 'randy_harness')) {
    # Windows can keep terminated processes in its list while another program holds a handle.
    foreach ($running in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
        if ($running.HasExited) { continue }
        # Other AO installations may stay open, provided none of their loaded files are being changed.
        # The renderer's path also identifies clients relaunched from a shared AppData executable.
        try {
            $processPath = $running.Path
            $modulePaths = @($running.Modules | ForEach-Object { $_.FileName })
        } catch { throw 'Close the game, launcher and patcher first; a running client could not be inspected.' }
        $usesClient = !$processPath -or $processPath.StartsWith($client + '\', [StringComparison]::OrdinalIgnoreCase)
        $knownRenderer = $false
        foreach ($modulePath in $modulePaths) {
            if (!$modulePath) { continue }
            if ($modulePath.StartsWith($client + '\', [StringComparison]::OrdinalIgnoreCase) -or $changingTargets -icontains $modulePath) { $usesClient = $true }
            if ([IO.Path]::GetFileName($modulePath) -ieq 'randy31.dll') { $knownRenderer = $true }
        }
        if ($usesClient -or (($name -eq 'AnarchyOnline' -or $name -eq 'randy_harness') -and !$knownRenderer)) {
            throw 'Close the game, launcher and patcher first; a running process uses this installation or its loader, or is still starting.'
        }
    }
}
}
Assert-AoClientClosed
if ($Uninstall) {
    foreach ($target in $targets) {
        $name = Split-Path -Leaf $target
        if (Test-Path -LiteralPath $target) {
            $actual = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
            $owned = Test-OwnedHash $target $actual
            if (!$owned) {
                throw "Refusing to remove an unrecognized $name."
            }
        }
    }
    # Move all files out of the loader search path before deleting any, so a locked file can be rolled back.
    $removed = @{}
    $restoreStages = @{}
    $restored = @()
    try {
        foreach ($target in $targets) {
            if ($fileSpecs[$target].restore) {
                $temp = $target + '.restoring-' + [guid]::NewGuid().ToString('N')
                $restoreStages[$target] = $temp
                [IO.File]::WriteAllBytes($temp, $fileSpecs[$target].bytes)
                if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) { throw 'Executable restoration staging failed.' }
            }
        }
        foreach ($target in $targets) {
            if ($fileSpecs[$target].executable -and !(Test-Path -LiteralPath $target)) { throw 'An executable disappeared during uninstall; retry after the game updater has finished.' }
            if (Test-Path -LiteralPath $target) {
                if ($fileSpecs[$target].special -and !(Test-OwnedHash $target (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)) { throw 'An executable or memory backup changed during uninstall; nothing was removed.' }
                $temp = $target + '.uninstalling-' + [guid]::NewGuid().ToString('N')
                Move-Item -LiteralPath $target -Destination $temp
                $removed[$target] = $temp
            }
        }
        if ($receipt) {
            $temp = $receiptPath + '.uninstalling-' + [guid]::NewGuid().ToString('N')
            Move-Item -LiteralPath $receiptPath -Destination $temp
            $removed[$receiptPath] = $temp
        }
        foreach ($target in $restoreStages.Keys) {
            Move-Item -LiteralPath $restoreStages[$target] -Destination $target
            $restored += $target
        }
    } catch {
        foreach ($target in $restored) {
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ieq $fileSpecs[$target].hash) { Remove-Item -LiteralPath $target }
        }
        foreach ($target in $removed.Keys) { Move-Item -LiteralPath $removed[$target] -Destination $target }
        throw
    } finally {
        foreach ($temp in $restoreStages.Values) { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
    }
    foreach ($temp in $removed.Values) { Remove-Item -LiteralPath $temp }
    Write-Output 'AO-VK addon removed. The original renderer was never changed.'
    Write-Output ([pscustomobject]@{ AoVkSetupResult=$true; memoryStatus=$memoryPlan.status; memoryWarnings=@($memoryPlan.warnings) })
    return
}
foreach ($item in $manifest.client.PSObject.Properties) {
    $target = Join-Path $client $item.Name
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $item.Value) {
        throw "Unsupported or modified $($item.Name). This package requires the audited classic live client."
    }
}
if (!(Test-Path -LiteralPath (Join-Path $client 'AnarchyOnline.exe'))) { throw 'Select the client folder containing AnarchyOnline.exe.' }
foreach ($name in @('ddraw.dll', 'ao-vk.dll')) {
    $source = Join-Path $package $name
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ine $manifest.addon.$name) {
        throw "Package integrity check failed for $name."
    }
}
if ($manifest.materialParts) {
    . (Join-Path $PSScriptRoot 'material-parts.ps1')
    Expand-AoMaterialParts $package $manifest $client $ProgressCallback
}
$verifiedSourceFiles = 0
foreach ($target in $targets) {
    $spec = $fileSpecs[$target]
    $verifiedSourceFiles++
    if ($ProgressCallback -and $verifiedSourceFiles % 128 -eq 1) { & $ProgressCallback "Verifying textures and files: $verifiedSourceFiles / $($targets.Count)" -1 }
    if ($spec.asset -and !$spec.retire -and (Get-FileHash -LiteralPath (Join-Path $package $spec.relative) -Algorithm SHA256).Hash -ine $spec.hash) {
        throw "Package integrity check failed for $($spec.relative)."
    }
}
foreach ($target in $targets) {
    if (Test-Path -LiteralPath $target) {
        if (!$Upgrade -and !$fileSpecs[$target].special) { throw "$target already exists. Use -Upgrade for a recognized earlier AO-VK package." }
        $name = Split-Path -Leaf $target
        $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        $owned = Test-OwnedHash $target $hash
        if (!$owned) { throw "Refusing to replace unrecognized graphics wrapper $target." }
    }
}
# Leave byte-identical DLLs in place, including a shared loader used by an unrelated installation.
foreach ($target in $unchangedTargets) {
    $name = Split-Path -Leaf $target
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) {
        throw 'A graphics file changed during the preflight checks. Try the update again.'
    }
}
# Prepare mandatory pipelines explicitly during installation, never in the first
# populated scene. Only source-built code is shipped; the cache stays on this GPU.
if ($manifest.cacheWarmup) {
    $warm = $manifest.cacheWarmup
    $expectedCache = if ($manifest.version -eq '0.6.0-preview.22') { 'randy-vk-preview22-v9-living.cache' } else { 'randy-vk-preview21-v9-living.cache' }
    if ($warm.helper -ne 'ao-vk-cache-warm.exe' -or $warm.relativeCache -ne $expectedCache) { throw 'Invalid cache preparation descriptor.' }
    $helper = Join-Path $package $warm.helper
    if ((Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash -ine $warm.sha256) { throw 'Cache preparation helper integrity failed.' }
    Write-Output 'Preparing this release on your graphics card. This may take one or two minutes; AO is not started.'
    if ($ProgressCallback) { & $ProgressCallback 'Preparing your graphics card (one or two minutes on first use)...' -1 }
    Assert-AoClientClosed
    & $helper --cache (Join-Path $client $warm.relativeCache)
    if ($LASTEXITCODE -ne 0) { throw 'GPU cache preparation failed. Renderer files were not changed.' }
}
Assert-AoClientClosed
if ($ProgressCallback) { & $ProgressCallback 'Installing verified files safely...' -1 }
$targets = @($changingTargets)
$staged = @()
$installed = @()
$backups = @{}
$succeeded = $false
$stageHashes = @{}
$record = [ordered]@{
    product = 'AO-VK Live'
    build = $manifest.build
    clientDirectory = $client
    loaderDirectory = $(if ($LoaderDir) { $loader } else { '' })
    addon = $manifest.addon
    assets = $manifest.assets
    edition = $manifest.edition
    largeAddressAware = @($memoryPlan.records)
}
$targets += $receiptPath
try {
    $stagingFiles = 0
    foreach ($target in $targets) {
        $stagingFiles++
        if ($ProgressCallback -and $stagingFiles % 128 -eq 1) { & $ProgressCallback "Preparing installation: $stagingFiles / $($targets.Count)" -1 }
        $name = Split-Path -Leaf $target
        if (!(Test-Path -LiteralPath (Split-Path -Parent $target))) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        }
        $temp = Join-Path (Split-Path -Parent $target) ('.' + $name + '.installing-' + [guid]::NewGuid().ToString('N'))
        if ($fileSpecs[$target].retire) { $staged += $null; continue }
        $staged += $temp
        if ($target -ieq $receiptPath) {
            [IO.File]::WriteAllText($temp, ($record | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
        } elseif ($fileSpecs[$target].special) {
            [IO.File]::WriteAllBytes($temp, $fileSpecs[$target].bytes)
            if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) { throw 'Executable memory-patch staging verification failed.' }
        } else {
            Copy-Item -LiteralPath (Join-Path $package $fileSpecs[$target].relative) -Destination $temp
            if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) { throw 'Copy verification failed.' }
        }
        $stageHashes[$target] = (Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash
    }
    Assert-AoClientClosed
    for ($i = 0; $i -lt $targets.Count; $i++) {
        if ($ProgressCallback -and $i % 128 -eq 0) { & $ProgressCallback "Installing verified files: $i / $($targets.Count)" -1 }
        $target = $targets[$i]
        if ($fileSpecs[$target].executable -and !(Test-Path -LiteralPath $target)) { throw 'An executable disappeared during installation; retry after the game updater has finished.' }
        if (Test-Path -LiteralPath $target) {
            if ($fileSpecs[$target].special -and !(Test-OwnedHash $target (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash)) { throw 'An executable or memory backup changed during installation.' }
            $backup = $target + '.upgrade-' + [guid]::NewGuid().ToString('N')
            Move-Item -LiteralPath $target -Destination $backup
            $backups[$target] = $backup
        }
        if ($fileSpecs[$target].retire) { continue }
        Move-Item -LiteralPath $staged[$i] -Destination $target
        $installed += $target
    }
    $succeeded = $true
} catch {
    foreach ($target in $installed) {
        if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ieq $stageHashes[$target]) {
            Remove-Item -LiteralPath $target
        }
    }
    foreach ($target in $backups.Keys) {
        if (!(Test-Path -LiteralPath $target)) { Move-Item -LiteralPath $backups[$target] -Destination $target }
    }
    throw
} finally {
    foreach ($temp in $staged) { if ($temp -and (Test-Path -LiteralPath $temp)) { Remove-Item -LiteralPath $temp } }
    if ($succeeded) { foreach ($backup in $backups.Values) { Remove-Item -LiteralPath $backup } }
}
Write-Output 'Installed the classic live addon. Start AO with the normal launcher; randy31.dll is unchanged.'
if ($manifest.livingAssets) {
    # Optional cosmetics use a separate atomic cache generation. A failed local
    # conversion leaves the normal renderer installed and the previous cache intact.
    try {
        Assert-AoClientClosed
        if ($ProgressCallback) { & $ProgressCallback 'Preparing Living assets locally inside the selected AO folder' -1 }
        & $livingHelper --client $client --metadata (Join-Path $package 'living-metadata')
        if ($LASTEXITCODE -ne 0) { throw "Local preparation exited with code $LASTEXITCODE." }
        Write-Output 'Local Living assets prepared. New settings default Off; unverified routes require the separate Newland test command.'
    } catch {
        Write-Warning ('The renderer is installed. Optional Living preparation did not finish: ' + $_.Exception.Message + ' Keep Living Off and retry preparation. Original archives and the previous cache are unchanged.')
    }
}
Write-Output ([pscustomobject]@{ AoVkSetupResult=$true; memoryStatus=$memoryPlan.status; memoryWarnings=@($memoryPlan.warnings) })
