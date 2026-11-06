param(
    [Parameter(Mandatory=$true)][string]$ClientDir,
    [string]$PackageDir = $PSScriptRoot,
    [string]$LoaderDir = '',
    [switch]$Upgrade,
    [switch]$Uninstall,
    [switch]$DisableLargeAddressAware,
    [switch]$ReviewLivingRoutes,
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
if ($ProgressCallback) { & $ProgressCallback 'Reading setup manifest...' -1 }
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
if (!$Uninstall -and $manifest.assets) {
    foreach ($item in $manifest.assets.PSObject.Properties) {
        if ($item.Name -match '^randy-vk[\\/]living[\\/]') {
            throw 'Living Rubi-Ka assets must come from a validated local client provider, not this package.'
        }
    }
}
if (!$Uninstall -and $manifest.livingAssets) {
    if ($ProgressCallback) { & $ProgressCallback 'Checking local Living preparation tools and recipes...' -1 }
    if ($manifest.livingAssets.provider -ne 'installation-local-v1' -or $manifest.livingAssets.status -ne 'local-test-candidate') {
        throw 'This candidate has no recognized Living Rubi-Ka asset provider.'
    }
    $livingHelper = Join-Path $package 'living-preparer\ao-vk-living-prepare.exe'
    if (!(Test-Path -LiteralPath $livingHelper) -or (Get-FileHash -LiteralPath $livingHelper -Algorithm SHA256).Hash -ine $manifest.livingAssets.helperSha256) {
        throw 'Local Living preparation helper integrity failed.'
    }
    $livingChecked = 0
    foreach ($item in $manifest.livingAssets.files.PSObject.Properties) {
        $livingChecked++
        if ($ProgressCallback -and $livingChecked % 8 -eq 1) { & $ProgressCallback "Checking Living preparation file $livingChecked : $($item.Name)" -1 }
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
    if ($ProgressCallback) { & $ProgressCallback 'Reading the existing installation record...' -1 }
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
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
    $candidate = Join-Path $env:APPDATA 'AOPP'
    if (Test-Path -LiteralPath (Join-Path $candidate 'AnarchyOnline.exe')) { $LoaderDir = $candidate }
}
$loader = ''
if ($LoaderDir) {
    $loader = [IO.Path]::GetFullPath($LoaderDir)
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
    if (!$Uninstall -and (Test-Path -LiteralPath $target) -and
        (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ieq $fileSpecs[$target].hash) {
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
        if (!$owned) {
            if ($fileSpecs[$target].asset) { throw "Refusing to replace an unrecognized existing asset: $target. Its contents were preserved." }
            throw "Refusing to replace unrecognized graphics wrapper $target."
        }
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
    $expectedCache = if ($manifest.version -eq '0.6.0-preview.23') { 'auto' } elseif ($manifest.version -eq '0.6.0-preview.22') { 'randy-vk-preview22-v9-living.cache' } else { 'randy-vk-preview21-v9-living.cache' }
    if ($warm.helper -ne 'ao-vk-cache-warm.exe' -or $warm.relativeCache -ne $expectedCache) { throw 'Invalid cache preparation descriptor.' }
    $helper = Join-Path $package $warm.helper
    if ((Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash -ine $warm.sha256) { throw 'Cache preparation helper integrity failed.' }
    Write-Output 'Preparing this release on your graphics card. This may take one or two minutes; AO is not started.'
    if ($ProgressCallback) { & $ProgressCallback 'Preparing your graphics card (one or two minutes on first use)...' -1 }
    Assert-AoClientClosed
    $cacheArgument = if ($warm.relativeCache -eq 'auto') { 'auto' } else { Join-Path $client $warm.relativeCache }
    . (Join-Path $PSScriptRoot 'setup-progress.ps1')
    $warmToken=[guid]::NewGuid().ToString('N')
    $warmLog=Join-Path $package ('.ao-vk-cache-'+$warmToken+'.log')
    $warmError=Join-Path $package ('.ao-vk-cache-'+$warmToken+'.error.log')
    $warmArgs='--cache "'+($cacheArgument -replace '(\\+)$','$1$1')+'"'
    $warmProcess=Start-Process -FilePath $helper -ArgumentList $warmArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput $warmLog -RedirectStandardError $warmError
    Wait-AoSetupHelper $warmProcess $warmLog $warmError 'Preparing graphics pipelines on this GPU' $ProgressCallback
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
$livingStatus=''
$livingWarnings=@()
if ($manifest.livingAssets) {
    # Optional cosmetics use a separate atomic cache generation. A failed local
    # conversion leaves the normal renderer installed and the previous cache intact.
    try {
        Assert-AoClientClosed
        . (Join-Path $PSScriptRoot 'setup-progress.ps1')
        Preserve-AoInterruptedLivingLock $client
        if ($ProgressCallback) { & $ProgressCallback 'Preparing Living assets locally inside the selected AO folder' -1 }
        $livingToken=[guid]::NewGuid().ToString('N')
        $livingLog=Join-Path $client ('.ao-vk-living-'+$livingToken+'.log')
        $livingErrorLog=Join-Path $client ('.ao-vk-living-'+$livingToken+'.error.log')
        $livingClientArg='"'+($client -replace '(\\+)$','$1$1')+'"'
        $livingMetadataArg='"'+((Join-Path $package 'living-metadata') -replace '(\\+)$','$1$1')+'"'
        $livingArguments='--client '+$livingClientArg+' --metadata '+$livingMetadataArg
        if ($ReviewLivingRoutes) { $livingArguments+=' --review-routes' }
        $livingProcess=Start-Process -FilePath $livingHelper -ArgumentList $livingArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $livingLog -RedirectStandardError $livingErrorLog
        Wait-AoSetupHelper $livingProcess $livingLog $livingErrorLog 'Preparing local Living resources' $ProgressCallback
        $livingStatus='Living assets prepared locally. To test them, run Prepare Living City Test.cmd, then open Ctrl+Shift+F12. New controls default Off.'
        $livingSummary=Get-Content -LiteralPath $livingLog -Tail 1 -ErrorAction SilentlyContinue
        if ($livingSummary -match '([0-9]+) unsupported asset families' -and [int]$Matches[1] -gt 0) {
            $livingWarnings=@('Some cosmetic assets are unavailable; affected instances will be skipped. See the local preparation log.')
        }
        Write-Output 'Local Living assets prepared. Run Prepare Living City Test.cmd to opt into city route review before enabling Living options.'
    } catch {
        $livingStatus='The renderer is installed, but Living assets were not prepared. Keep Living Off and retry preparation.'
        $livingWarnings=@($_.Exception.Message)
        Write-Warning ($livingStatus+' '+$_.Exception.Message+' Original archives were not modified by preparation.')
    }
}
Write-Output ([pscustomobject]@{ AoVkSetupResult=$true; memoryStatus=$memoryPlan.status; memoryWarnings=@($memoryPlan.warnings); livingStatus=$livingStatus; livingWarnings=$livingWarnings })
