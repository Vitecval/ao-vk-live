param(
    [Parameter(Mandatory=$true)][string]$ClientDir,
    [string]$PackageDir = $PSScriptRoot,
    [string]$LoaderDir = '',
    [switch]$Upgrade,
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$client = (Resolve-Path -LiteralPath $ClientDir).Path
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$manifest = Get-Content -LiteralPath (Join-Path $package 'manifest.json') -Raw | ConvertFrom-Json
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
    if ($relative -notmatch '^randy-vk/packs/ao-((modern|cyber)-icc|cyber-characters|cyber-world)/(source-art/)?[A-Za-z0-9_-]+\.(png|json|md)$' -or $assetHashes[$relative] -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'Invalid optional material pack path or hash.'
    }
    $target = Join-Path $client $relative
    $newHash = $(if ($manifest.assets) { $manifest.assets.$relative })
    $oldHash = $(if ($receipt -and $receipt.assets) { $receipt.assets.$relative })
    $fileSpecs[$target] = @{ relative = $relative; hash = $newHash; oldHash = $oldHash; asset = $true; retire = (!$newHash -and !$Uninstall) }
    $targets += $target
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
foreach ($target in $targets) {
    $name = Split-Path -Leaf $target
    if (!$Uninstall -and (Test-Path -LiteralPath $target) -and
        (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ieq $fileSpecs[$target].hash) {
        $unchangedTargets += $target
    } else { $changingTargets += $target }
}
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
    try {
        foreach ($target in $targets) {
            if (Test-Path -LiteralPath $target) {
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
    } catch {
        foreach ($target in $removed.Keys) { Move-Item -LiteralPath $removed[$target] -Destination $target }
        throw
    }
    foreach ($temp in $removed.Values) { Remove-Item -LiteralPath $temp }
    Write-Output 'AO-VK addon removed. The original renderer was never changed.'
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
foreach ($target in $targets) {
    $spec = $fileSpecs[$target]
    if ($spec.asset -and !$spec.retire -and (Get-FileHash -LiteralPath (Join-Path $package $spec.relative) -Algorithm SHA256).Hash -ine $spec.hash) {
        throw "Package integrity check failed for $($spec.relative)."
    }
}
foreach ($target in $targets) {
    if (Test-Path -LiteralPath $target) {
        if (!$Upgrade) { throw "$target already exists. Use -Upgrade for a recognized earlier AO-VK package." }
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
}
$targets += $receiptPath
try {
    foreach ($target in $targets) {
        $name = Split-Path -Leaf $target
        if (!(Test-Path -LiteralPath (Split-Path -Parent $target))) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        }
        $temp = Join-Path (Split-Path -Parent $target) ('.' + $name + '.installing-' + [guid]::NewGuid().ToString('N'))
        if ($fileSpecs[$target].retire) { $staged += $null; continue }
        $staged += $temp
        if ($target -ieq $receiptPath) {
            [IO.File]::WriteAllText($temp, ($record | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
        } else {
            Copy-Item -LiteralPath (Join-Path $package $fileSpecs[$target].relative) -Destination $temp
            if ((Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash -ine $fileSpecs[$target].hash) { throw 'Copy verification failed.' }
        }
        $stageHashes[$target] = (Get-FileHash -LiteralPath $temp -Algorithm SHA256).Hash
    }
    for ($i = 0; $i -lt $targets.Count; $i++) {
        $target = $targets[$i]
        if (Test-Path -LiteralPath $target) {
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

