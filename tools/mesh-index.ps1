# The index contains hashes, numeric identities and a fixed renderer category, not game meshes.
function Get-AoMeshIndexHash([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 21 -or $bytes.Length -gt 33554432 -or
        [Text.Encoding]::ASCII.GetString($bytes, 0, 8) -cne 'AOMESHID') { throw 'Invalid existing model-identification index. Its contents were preserved.' }
    $version = [BitConverter]::ToUInt32($bytes, 8)
    $count = [BitConverter]::ToUInt32($bytes, 12)
    $names = [BitConverter]::ToUInt32($bytes, 16)
    if ($version -ne 1 -or !$count -or $count -gt 1000000 -or !$names -or
        [long]$bytes.Length -ne (20L + 32L * $count + $names) -or
        $bytes[$bytes.Length - 1] -ne 0) { throw 'Invalid existing model-identification index. Its contents were preserved.' }
    $length = 20 + 32 * [int]$count
    $label = 'scenery_spaceship'
    $nameTable = [Text.Encoding]::ASCII.GetBytes(([string][char]0) + $label + ([string][char]0))
    $canonical = New-Object byte[] ($length + $nameTable.Length)
    [Array]::Copy($bytes, $canonical, $length)
    [Array]::Copy([BitConverter]::GetBytes([uint32]$nameTable.Length), 0, $canonical, 16, 4)
    [Array]::Copy($nameTable, 0, $canonical, $length, $nameTable.Length)
    for ($i = 0; $i -lt $count; $i++) {
        $offset = 20 + $i * 32 + 28
        $nameOffset = [BitConverter]::ToUInt32($bytes, $offset)
        if ($nameOffset -ge $names) { throw 'Invalid existing model-index name offset. Its contents were preserved.' }
        [Array]::Clear($canonical, $offset, 4)
        if ($names - $nameOffset -ge $label.Length -and
            [Text.Encoding]::ASCII.GetString($bytes, $length + $nameOffset, $label.Length) -ceq $label) {
            [Array]::Copy([BitConverter]::GetBytes([uint32]1), 0, $canonical, $offset, 4)
        }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($canonical))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Test-AoPreserveMeshIndex([string]$Client, [string]$Package, $Manifest, $Receipt, [bool]$Removing) {
    $relative = 'randy-vk/mesh-ids.bin'
    $owned = $Receipt -and $Receipt.assets -and $Receipt.assets.$relative
    # An uninstaller must never adopt a pre-existing, user-supplied index.
    if ($Removing) { return !$owned }
    $index = $Manifest.meshIndex
    if (!$index) {
        if ($Manifest.setupRevision -ge 4) { throw 'Incomplete setup package: the required model-identification index is missing. Download and extract a fresh setup ZIP.' }
        return $false
    }
    if ($index.file -cne $relative -or $index.formatVersion -ne 1 -or
        $index.sha256 -notmatch '^[a-f0-9]{64}$' -or $Manifest.assets.$relative -cne $index.sha256) { throw 'Invalid model-identification package descriptor.' }
    $source = Join-Path $Package $relative
    if (!(Test-Path -LiteralPath $source) -or
        (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ine $index.sha256 -or
        (Get-AoMeshIndexHash $source) -cne $index.sha256) { throw 'Required model-identification file is missing or damaged. Extract a fresh setup ZIP and check antivirus protection history.' }
    $target = Join-Path $Client $relative
    foreach ($path in @((Join-Path $Client 'randy-vk'), $target)) {
        if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'A model-index path is a link; refusing to modify it.' }
    }
    if (!(Test-Path -LiteralPath $target)) { return $false }
    if ((Get-AoMeshIndexHash $target) -cne $index.sha256) { throw 'The existing model-identification index differs from this supported client build. It was preserved; back it up before resolving the conflict.' }
    # Names are descriptive except for the renderer's scenery_spaceship prefix.
    # Preserve compatible annotations even if an older setup once owned this file.
    return (!$owned -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $index.sha256)
}
