$setupModules = Join-Path $PSHOME 'Modules'
$env:PSModulePath = (@($setupModules) + @($env:PSModulePath -split ';' | Where-Object { $_ -and $_ -ine $setupModules })) -join ';'
# Companion archives are complete material packages, each independently hashed.
. (Join-Path $PSScriptRoot 'material-download.ps1')
. (Join-Path $PSScriptRoot 'setup-safety.ps1')
function Test-AoMaterialNeeded([string]$Relative,[string]$Package,$RequiredAssets,$PinnedHashes) {
    if ($null -ne $RequiredAssets -and !$RequiredAssets.Contains($Relative)) { return $false }
    return (Get-AoSetupHash (Join-Path $Package $Relative)) -ine $PinnedHashes[$Relative]
}
function Expand-AoMaterialParts($Package, $Manifest, [string]$Client = '', [scriptblock]$ProgressCallback, $RequiredAssets = $null) {
    if (!$Manifest.materialParts) { return }
    Assert-AoSetupPath $Package
    $pinnedHashes = @{}
    foreach ($entry in $Manifest.assets.PSObject.Properties) { $pinnedHashes[$entry.Name] = [string]$entry.Value }
    if ($Manifest.artworkLicense) {
        if ($Manifest.artworkLicense.file -cne 'LICENSE-TEXTURES.md' -or $Manifest.artworkLicense.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
            !(Test-Path -LiteralPath (Join-Path $Package 'LICENSE-TEXTURES.md')) -or
            (Get-FileHash -LiteralPath (Join-Path $Package 'LICENSE-TEXTURES.md') -Algorithm SHA256).Hash -ine $Manifest.artworkLicense.sha256) { throw 'The HD artwork license is missing or modified. Extract a fresh Full HD Setup.' }
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $cache = Join-Path $Package '.ao-vk-downloads'
    if ((Test-Path -LiteralPath $cache) -and ((Get-Item -LiteralPath $cache -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Unsafe download cache link; extract Setup to a new folder.' }
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $lock = $null
    try { $lock = [IO.File]::Open((Join-Path $cache 'setup.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch { throw 'Another Full HD setup is using this folder. Close it before retrying.' }
    try {
    $archives = @{}; $pending = @(); [long]$downloadBytes = 0; [long]$unpackedBytes = 0
    $identities = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $members = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
    $checkedMembers = 0
    foreach ($part in $Manifest.materialParts) {
        if ($part.name -notmatch '^ao-vk-live-[A-Za-z0-9.-]+-materials-[0-9]+\.zip$' -or $part.sha256 -notmatch '^[a-f0-9]{64}$') { throw 'Invalid material package descriptor.' }
        Assert-AoMaterialRelease $Manifest $part
        if (!$identities.Add([string]$part.name)) { throw 'Duplicate material package descriptor.' }
        foreach ($notice in @($part.notices.PSObject.Properties)) {
            if (!$notice) { continue }
            if ($notice.Name -cne 'LICENSE-TEXTURES.md' -or $notice.Value -cnotmatch '^[a-f0-9]{64}$' -or $notice.Value -cne $Manifest.artworkLicense.sha256) { throw 'Invalid companion artwork-license descriptor.' }
        }
        if ($Manifest.artworkLicense -and !$part.notices.'LICENSE-TEXTURES.md') { throw 'Companion is missing the pinned artwork license.' }
        foreach ($relative in $part.files) {
            $checkedMembers++
            if ($checkedMembers % 256 -eq 1) { Write-AoMaterialProgress "Checking HD texture list: $checkedMembers files ($($part.name))" -1 $ProgressCallback }
            if ($relative -notmatch '^randy-vk/(packs/ao-((modern|cyber)-icc|icc-enhanced-original|cyber-characters|cyber-world)/[A-Za-z0-9_-]+\.png|cache/materials/[a-f0-9]{16}\.rvkm)$' -or !$members.Add([string]$relative) -or $pinnedHashes[$relative] -notmatch '^[a-f0-9]{64}$') { throw 'Invalid or duplicate pinned material member.' }
        }
        if (!@($part.files | Where-Object { Test-AoMaterialNeeded ([string]$_) $Package $RequiredAssets $pinnedHashes }).Count) { continue }
        $pending += $part
        $unpackedBytes += [long]$part.unpackedBytes
        Write-AoMaterialProgress 'Checking saved Full HD packs...' -1 $ProgressCallback
        foreach ($directory in @(Get-AoMaterialSearchFolders $Package $cache)) {
            $candidate = Join-Path $directory $part.name
            Assert-AoSetupPath $candidate
            if (Test-AoMaterialArchive $candidate $part) { $archives[$part.name] = $candidate; Write-Host "Reusing verified $($part.name)."; break }
            if (Test-Path -LiteralPath $candidate) { Write-Host "Checksum/size mismatch: ignoring $candidate and seeking the pinned release copy." }
        }
        if (!$archives.ContainsKey($part.name)) { $downloadBytes += [long]$part.bytes }
    }
    if (!$pending.Count) {
        Write-AoMaterialProgress 'Existing HD textures verified; no material download or unpacking is needed.' -1 $ProgressCallback
        return
    }
    Assert-AoMaterialSpace $Package $Client ($downloadBytes + $unpackedBytes) ([long]$Manifest.installBytes)
    Write-Host ('Full HD: {0} material packs; {1:N2} GiB to download; {2:N2} GiB to prepare.' -f $pending.Count, ($downloadBytes / 1GB), ($unpackedBytes / 1GB))
    $index = 0
    foreach ($part in $pending) {
        $index++
        if (!$archives.ContainsKey($part.name)) { $archives[$part.name] = Receive-AoMaterialArchive $cache $Manifest $part "Downloading HD $index of $($pending.Count)" $ProgressCallback }
    }
    # No game file is changed until every required archive has passed its pin.
    foreach ($part in $pending) {
        $allowed = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        foreach ($relative in $part.files) {
            if ($relative -notmatch '^randy-vk/(packs/ao-((modern|cyber)-icc|icc-enhanced-original|cyber-characters|cyber-world)/[A-Za-z0-9_-]+\.png|cache/materials/[a-f0-9]{16}\.rvkm)$' -or !$allowed.Add($relative)) { throw 'Invalid or duplicate material member.' }
        }
        $needed = @($part.files | Where-Object { Test-AoMaterialNeeded ([string]$_) $Package $RequiredAssets $pinnedHashes })
        foreach ($notice in @($part.notices.PSObject.Properties)) { if ($notice) { $null = $allowed.Add($notice.Name) } }
        if (!$needed.Count) { continue } # Installer separately verifies every decoded asset hash.
        $neededSet=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::Ordinal)
        foreach ($relative in $needed) { $null=$neededSet.Add($relative) }
        $archive = $archives[$part.name]
        Write-AoMaterialProgress "Preparing verified HD textures: $($part.name)" -1 $ProgressCallback
        $stage = Join-Path $Package ('.material-stage-' + [guid]::NewGuid().ToString('N'))
        $root = [IO.Path]::GetFullPath($stage) + '\'
        New-Item -ItemType Directory -Path $stage | Out-Null
        $zip = $null
        try {
            $zip = [IO.Compression.ZipFile]::OpenRead($archive)
            $found = @{}
            $prepared = 0
            foreach ($entry in $zip.Entries) {
                $prepared++
                if ($prepared % 64 -eq 1) { Write-AoMaterialProgress "Preparing HD textures: $prepared / $($allowed.Count)" ([Math]::Min(100,[int](100.0*$prepared/$allowed.Count))) $ProgressCallback }
                $relative = $entry.FullName.Replace('\','/')
                $expectedHash = if ($part.notices.$relative) { [string]$part.notices.$relative } else { $pinnedHashes[$relative] }
                if (!$allowed.Contains($relative) -or !$expectedHash -or $found.ContainsKey($relative)) { throw 'Unexpected or duplicate companion archive entry.' }
                $target = [IO.Path]::GetFullPath((Join-Path $stage $relative))
                if (!$target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe companion archive path.' }
                $found[$relative] = $true
                if (!$neededSet.Contains($relative) -and !$part.notices.$relative) { continue }
                $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $target))
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $target, $false)
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $expectedHash) { throw 'Decoded material integrity check failed.' }
                $found[$relative] = $true
            }
            if ($found.Count -ne $allowed.Count) { throw 'Incomplete companion material package.' }
            $zip.Dispose(); $zip = $null
            $preparedFiles = 0
            foreach ($relative in $needed) {
                $preparedFiles++
                if ($preparedFiles % 64 -eq 1) { Write-AoMaterialProgress "Preparing verified HD textures: $preparedFiles / $($needed.Count)" ([Math]::Min(100,[int](100.0*$preparedFiles/$needed.Count))) $ProgressCallback }
                $target = Join-Path $Package $relative
                Assert-AoSetupPath $target
                if (Test-Path -LiteralPath $target) {
                    if ((Get-AoSetupHash $target) -ine $Manifest.assets.$relative) {
                        [IO.File]::Replace((Join-Path $stage $relative),$target,[System.Management.Automation.Language.NullString]::Value)
                    }
                } else {
                    $null = [IO.Directory]::CreateDirectory((Split-Path -Parent $target))
                    Move-Item -LiteralPath (Join-Path $stage $relative) -Destination $target
                }
            }
        } finally {
            if ($zip) { $zip.Dispose() }
            if ([IO.Path]::GetFullPath($stage).StartsWith([IO.Path]::GetFullPath($Package).TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
                try { Remove-Item -LiteralPath $stage -Recurse -Force } catch { Write-Warning "Could not clean temporary texture folder $stage : $_" }
            }
        }
    }
    Write-Progress -Activity 'AO-VK Full HD Setup' -Completed
    } finally { if ($lock) { $lock.Dispose() } }
}
