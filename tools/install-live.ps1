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
$setupModules = Join-Path $PSHOME 'Modules'
$env:PSModulePath = (@($setupModules) + @($env:PSModulePath -split ';' | Where-Object { $_ -and $_ -ine $setupModules })) -join ';'

# Match the 32-bit game: 64-bit Windows PowerShell can return only the WOW64
# loader modules, hiding randy31.dll and falsely identifying another client.
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $host32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    if (!(Test-Path -LiteralPath $host32)) { throw 'Cannot find 32-bit Windows PowerShell for safe AO process inspection.' }
    $hostArgs = @('-NoLogo', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    foreach ($key in $PSBoundParameters.Keys) {
        if ($key -eq 'ProgressCallback') { continue }
        $value = $PSBoundParameters[$key]
        if ($value -is [Management.Automation.SwitchParameter]) {
            if ($value.IsPresent) { $hostArgs += "-$key" }
        } else { $hostArgs += @("-$key", [string]$value) }
    }
    if ($ProgressCallback) { & $ProgressCallback 'Running setup in the game-compatible PowerShell host...' -1 }
    & $host32 @hostArgs
    if ($LASTEXITCODE -ne 0) { throw "AO-VK installer failed in 32-bit PowerShell (exit $LASTEXITCODE)." }
    return
}
 . (Join-Path $PSScriptRoot 'setup-safety.ps1')
$setupLocks = @()
try {
$client = Get-AoSetupDirectory $ClientDir 'AO installation folder'
$package = Get-AoSetupDirectory $PackageDir 'Setup package folder'
if ($client -ieq $package) { throw 'Extract Setup into its own folder, separate from the AO installation.' }
$setupLocks += Enter-AoSetupLock $client
if (Test-Path -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json')) {
    if ($ProgressCallback) { & $ProgressCallback 'Recovering the interrupted setup safely...' -1 }
    Recover-AoSetupTransaction $client $ProgressCallback
}
Assert-AoSetupPath (Join-Path $package 'manifest.json')
if ($ProgressCallback) { & $ProgressCallback 'Reading setup manifest...' -1 }
$manifest = Read-AoSetupJson (Join-Path $package 'manifest.json')
Assert-AoSetupManifest $manifest
if (!$Uninstall -and $manifest.assets) {
    foreach ($item in $manifest.assets.PSObject.Properties) {
        if ($item.Name -match '^randy-vk[\\/]living[\\/]') {
            throw 'Living is a separate add-on and is not included in this renderer package.'
        }
    }
}
if (!$Uninstall -and $manifest.livingAssets) { throw 'Living is a separate add-on and is not included in this renderer package.' }
$receiptPath = Join-Path $client 'ao-vk-live.install.json'
Assert-AoSetupPath $receiptPath
$receiptObserved = Get-AoSetupHash $receiptPath
$receipt = $null
if (Test-Path -LiteralPath $receiptPath) {
    if ($ProgressCallback) { & $ProgressCallback 'Reading the existing installation record...' -1 }
    $receipt = Read-AoSetupJson $receiptPath
    if ($receipt.product -ne 'AO-VK Live' -or $receipt.clientDirectory -ine $client) {
        throw 'The existing AO-VK install record does not belong to this folder.'
    }
    if (!$LoaderDir -and $receipt.loaderDirectory) { $LoaderDir = [string]$receipt.loaderDirectory }
}
$meshRelative = 'randy-vk/mesh-ids.bin'
$preserveMeshIndex = $false
if ($manifest.meshIndex -or $manifest.setupRevision -ge 4 -or
    $manifest.assets.$meshRelative -or $receipt.assets.$meshRelative) {
    if ($ProgressCallback) { & $ProgressCallback 'Checking compatibility of the existing model index...' -1 }
    . (Join-Path $PSScriptRoot 'mesh-index.ps1')
    $preserveMeshIndex = Test-AoPreserveMeshIndex $client $package $manifest $receipt ([bool]$Uninstall)
}
$targets = New-Object 'Collections.Generic.List[string]'
$targets.Add((Join-Path $client 'ao-vk.dll'))
$targets.Add((Join-Path $client 'ddraw.dll'))
if (!$LoaderDir -and !$receipt -and (Test-Path -LiteralPath (Join-Path $client 'MSWSOCK.dll'))) {
    $appData = if ($env:APPDATA) { $env:APPDATA } else { [Environment]::GetFolderPath('ApplicationData') }
    $candidate = if ($appData) { Join-Path $appData 'AOPP' } else { $null }
    if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'AnarchyOnline.exe'))) { $LoaderDir = $candidate }
}
$loader = ''
if ($LoaderDir) {
    $loader = if ($Uninstall -and !(Test-Path -LiteralPath $LoaderDir)) { [IO.Path]::GetFullPath($LoaderDir).TrimEnd('\') } else { Get-AoSetupDirectory $LoaderDir 'Shared loader folder' }
    Assert-AoSetupPath $loader
    if ($loader -ine $client) { $setupLocks += Enter-AoSetupLock $loader }
    if (!$Uninstall -and !(Test-Path -LiteralPath (Join-Path $loader 'AnarchyOnline.exe'))) { throw 'The extra loader folder must contain AnarchyOnline.exe.' }
    if ($loader -ine $client) { $targets.Add((Join-Path $loader 'ddraw.dll')) }
}
$fileSpecs = @{}
foreach ($target in $targets) {
    $name = Split-Path -Leaf $target
    $fileSpecs[$target] = @{ relative = $name; hash = $manifest.addon.$name; oldHash = $(if ($receipt) { $receipt.addon.$name }) }
}
# Asset paths are restricted to the reserved packs and the exact model-index file.
# Never trust a manifest or install record to write/remove an arbitrary path.
$assetHashes = [ordered]@{}
$newAssetHashes = @{}
$oldAssetHashes = @{}
if ($receipt -and $receipt.assets) {
    foreach ($item in $receipt.assets.PSObject.Properties) {
        $oldAssetHashes[$item.Name] = [string]$item.Value
        $assetHashes[$item.Name] = [string]$item.Value
    }
}
if ($manifest.assets) {
    if ($ProgressCallback) { & $ProgressCallback 'Building the list of files to check...' -1 }
    foreach ($item in $manifest.assets.PSObject.Properties) {
        $newAssetHashes[$item.Name] = [string]$item.Value
        $assetHashes[$item.Name] = [string]$item.Value
    }
}
foreach ($relative in $assetHashes.Keys) {
    if ($relative -notmatch '^randy-vk/(mesh-ids\.bin|packs/ao-((modern|cyber)-icc|icc-enhanced-original|cyber-characters|cyber-world)/(source-art/)?[A-Za-z0-9_-]+\.(png|json|md)|living/[A-Za-z0-9_.-]+\.(lrs|lrm|lrg|ini|txt|json))$' -or $assetHashes[$relative] -notmatch '^[a-fA-F0-9]{64}$') {
        throw "Invalid optional material pack path or hash: $relative."
    }
    if ($preserveMeshIndex -and $relative -ceq $meshRelative) { continue }
    $target = Join-Path $client $relative
    $newHash = $newAssetHashes[$relative]
    $oldHash = $oldAssetHashes[$relative]
    $fileSpecs[$target] = @{ relative = $relative; hash = $newHash; oldHash = $oldHash; asset = $true; retire = (!$newHash -and !$Uninstall) }
    $targets.Add($target)
}
$memoryPlan = @{ records=@(); changes=@(); warnings=@(); status='Left unchanged' }
if ($manifest.largeAddressAware -or $receipt.largeAddressAware) {
    . (Join-Path $PSScriptRoot 'large-address-aware.ps1')
    $memoryPlan = Get-AoLaaPlan $client $loader $manifest.largeAddressAware $receipt (!$DisableLargeAddressAware) ([bool]$Uninstall)
    foreach ($change in $memoryPlan.changes) {
        if ($fileSpecs.ContainsKey($change.target)) { throw 'Duplicate executable memory-patch target.' }
        $fileSpecs[$change.target] = $change
        $targets.Add($change.target)
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
$unchangedTargets = New-Object 'Collections.Generic.List[string]'
$changingTargets = New-Object 'Collections.Generic.List[string]'
$checkedTargets = 0
foreach ($target in $targets) {
    $checkedTargets++
    if ($ProgressCallback -and $checkedTargets % 250 -eq 0) { & $ProgressCallback "Checking existing files: $checkedTargets / $($targets.Count)" -1 }
    $name = Split-Path -Leaf $target
    $observed = Get-AoSetupHash $target
    $fileSpecs[$target].observedHash = $observed
    if (!$Uninstall -and $observed -and $observed -ieq $fileSpecs[$target].hash) {
        $fileSpecs[$target].unchanged = $true
        $unchangedTargets.Add($target)
    } else { $changingTargets.Add($target) }
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
        } catch { if ($running.HasExited) { continue }; throw 'Close the game, launcher and patcher first; a running client could not be inspected.' }
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
    $restoreStages = @{}
    try {
        $entries = New-Object 'Collections.Generic.List[object]'
        foreach ($target in $targets) {
            $stage=$null;$after=$null
            if ($fileSpecs[$target].restore) {
                $stage=$target+'.restoring-'+[guid]::NewGuid().ToString('N')
                $restoreStages[$target]=$stage
                [IO.File]::WriteAllBytes($stage,$fileSpecs[$target].bytes)
                $after=Get-AoSetupHash $stage
                if ($after -ine $fileSpecs[$target].hash) { throw 'Executable restoration staging failed.' }
            }
            if ($fileSpecs[$target].observedHash -or $stage) {
                $entries.Add(@{target=$target;stage=$stage;before=$fileSpecs[$target].observedHash;after=$after})
            }
        }
        if ($receiptObserved) { $entries.Add(@{target=$receiptPath;stage=$null;before=$receiptObserved;after=$null}) }
        Assert-AoClientClosed
        if ($entries.Count) { Invoke-AoSetupTransaction $client $loader ($entries.ToArray()) $ProgressCallback }
    } finally {
        if (!(Test-Path -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json'))) {
            foreach ($temp in $restoreStages.Values) { if (Test-Path -LiteralPath $temp) { try { Remove-Item -LiteralPath $temp } catch { Write-Warning "Temporary setup file retained: $temp" } } }
        }
    }
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
    if ((Get-AoSetupHash $source) -ine $manifest.addon.$name) {
        throw "Package integrity check failed for $name."
    }
}
foreach ($target in $targets) {
    if (Test-Path -LiteralPath $target) {
        if (!$Upgrade -and !$fileSpecs[$target].special) { throw "$target already exists. Use -Upgrade for a recognized earlier AO-VK package." }
        $name = Split-Path -Leaf $target
        $hash = $fileSpecs[$target].observedHash
        $owned = Test-OwnedHash $target $hash
        if (!$owned) {
            if ($fileSpecs[$target].asset) { throw "Refusing to replace an unrecognized existing asset: $target. Its contents were preserved." }
            throw "Refusing to replace unrecognized graphics wrapper $target."
        }
    }
}
if ($manifest.materialParts) {
    . (Join-Path $PSScriptRoot 'material-parts.ps1')
    $neededAssets = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    foreach ($target in $changingTargets) {
        $spec=$fileSpecs[$target]
        if ($spec.asset -and !$spec.retire) { $null=$neededAssets.Add([string]$spec.relative) }
    }
    Expand-AoMaterialParts $package $manifest $client $ProgressCallback $neededAssets
}
$verifiedSourceFiles = 0
foreach ($target in $targets) {
    $spec = $fileSpecs[$target]
    $verifiedSourceFiles++
    if ($ProgressCallback -and $verifiedSourceFiles % 128 -eq 1) { & $ProgressCallback "Verifying textures and files: $verifiedSourceFiles / $($targets.Count)" -1 }
    if ($spec.asset -and !$spec.retire -and !$spec.unchanged -and (Get-FileHash -LiteralPath (Join-Path $package $spec.relative) -Algorithm SHA256).Hash -ine $spec.hash) {
        throw "Package integrity check failed for $($spec.relative)."
    }
}
# Leave byte-identical DLLs in place, including a shared loader used by an unrelated installation.
foreach ($target in $unchangedTargets) {
    if ($fileSpecs[$target].asset) { continue } # No write is made to verified unchanged artwork.
    $name = Split-Path -Leaf $target
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) {
        throw 'A graphics file changed during the preflight checks. Try the update again.'
    }
}
# Prepare mandatory pipelines explicitly during installation, never in the first
# populated scene. Only source-built code is shipped; the cache stays on this GPU.
if ($manifest.cacheWarmup) {
    $warm = $manifest.cacheWarmup
    $expectedCache = if ($manifest.version -eq '0.6.0-preview.23') { 'auto' } elseif ($manifest.version -eq '0.6.0-preview.22') { 'randy-vk-preview22-v9-living.cache' } else { 'randy-vk-preview21-v9-living.cache' }
    if ($warm.helper -ne 'ao-vk-cache-warm.exe' -or $warm.relativeCache -ne $expectedCache) { throw 'Invalid cache preparation descriptor.' }
    $helper = Join-Path $package $warm.helper
    if ((Get-AoSetupHash $helper) -ine $warm.sha256) { throw 'Cache preparation helper integrity failed.' }
    Write-Output 'Preparing this release on your graphics card. This may take one or two minutes; AO is not started.'
    if ($ProgressCallback) { & $ProgressCallback 'Preparing your graphics card (one or two minutes on first use)...' -1 }
    Assert-AoClientClosed
    $cacheArgument = if ($warm.relativeCache -eq 'auto') { 'auto' } else { Join-Path $client $warm.relativeCache }
    . (Join-Path $PSScriptRoot 'setup-progress.ps1')
    $warmToken=[guid]::NewGuid().ToString('N')
    $warmFolder=Join-Path ([IO.Path]::GetTempPath()) 'AO-VK-Setup-Logs'
    Assert-AoSetupPath $warmFolder
    New-Item -ItemType Directory -Path $warmFolder -Force | Out-Null
    $warmLog=Join-Path $warmFolder ('gpu-'+$warmToken+'.log')
    $warmError=Join-Path $warmFolder ('gpu-'+$warmToken+'.error.log')
    $warmArgs='--cache "'+($cacheArgument -replace '(\\+)$','$1$1')+'"'
    $warmProcess=Start-Process -FilePath $helper -ArgumentList $warmArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput $warmLog -RedirectStandardError $warmError
    Wait-AoSetupHelper $warmProcess $warmLog $warmError 'Preparing graphics pipelines on this GPU' $ProgressCallback
}
Assert-AoClientClosed
if ($ProgressCallback) { & $ProgressCallback 'Installing verified files safely...' -1 }
$targets = @($changingTargets)
$staged = New-Object 'Collections.Generic.List[object]'
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
if ($preserveMeshIndex) {
    # Do not claim ownership of a pre-existing compatible index or remove it on uninstall.
    $recordAssets = [ordered]@{}
    foreach ($item in $manifest.assets.PSObject.Properties) {
        if ($item.Name -cne $meshRelative) { $recordAssets[$item.Name] = $item.Value }
    }
    $record.assets = $recordAssets
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
        if ($fileSpecs[$target].retire) { $staged.Add($null); continue }
        $staged.Add($temp)
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
    $entries=New-Object 'Collections.Generic.List[object]'
    for ($i=0;$i -lt $targets.Count;$i++) {
        $target=$targets[$i]
        $before=if ($target -ieq $receiptPath) { $receiptObserved } else { $fileSpecs[$target].observedHash }
        $entries.Add(@{target=$target;stage=$staged[$i];before=$before;after=$stageHashes[$target]})
    }
    Invoke-AoSetupTransaction $client $loader ($entries.ToArray()) $ProgressCallback
} finally {
    if (!(Test-Path -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json'))) {
        foreach ($temp in $staged) { if ($temp -and (Test-Path -LiteralPath $temp)) { try { Remove-Item -LiteralPath $temp } catch { Write-Warning "Temporary setup file retained: $temp" } } }
    }
}
Write-Output 'Installed the classic live addon. Start AO with the normal launcher; randy31.dll is unchanged.'
Write-Output ([pscustomobject]@{ AoVkSetupResult=$true; memoryStatus=$memoryPlan.status; memoryWarnings=@($memoryPlan.warnings) })

} catch {
    $failureLog = Write-AoSetupFailure $_
    if ($failureLog) { Write-Warning ("Setup details saved to " + $failureLog) }
    throw
} finally {
    foreach ($setupLock in $setupLocks) { try { $setupLock.ReleaseMutex() } finally { $setupLock.Dispose() } }
}
