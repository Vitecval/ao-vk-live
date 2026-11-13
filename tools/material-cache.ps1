# Bind portable payloads to the source timestamps in the install transaction.
function Write-AoMaterialBindings([string]$Catalog, [string]$Output, [string]$Client, $Sources, $Manifest) {
    if ((Get-Item -LiteralPath $Catalog).Length -gt 16MB) { throw 'Prepared-material catalogue is too large.' }
    $lines = [IO.File]::ReadAllLines($Catalog)
    if ($lines.Count -lt 2 -or $lines.Count -gt 30001 -or $lines[0] -cne 'RVK-RECIPES-1') { throw 'Invalid prepared-material catalogue.' }
    $stamps = @{}
    $writer = New-Object IO.StreamWriter($Output, $false, (New-Object Text.UTF8Encoding($false)))
    $writer.NewLine = "`n"
    try {
        $writer.WriteLine('RVK-BINDINGS-1')
        foreach ($line in $lines[1..($lines.Count-1)]) {
            $fields = $line.Split([char]9)
            if ($fields.Count -ne 11 -or $fields[0] -cnotmatch '^[a-f0-9]{16}$') { throw 'Invalid prepared-material recipe.' }
            $cache = 'randy-vk/cache/materials/' + $fields[0] + '.rvkm'
            if (!$Manifest.assets.$cache) { throw 'A prepared recipe has no pinned payload.' }
            $writer.Write($fields[0])
            foreach ($relative in $fields[1..10]) {
                if (!$relative) { $writer.Write("`t`t0`t0"); continue }
                if ($relative -cnotmatch '^randy-vk/packs/ao-((modern|cyber)-icc|icc-enhanced-original|cyber-characters|cyber-world)/[A-Za-z0-9_-]+\.png$' -or !$Manifest.assets.$relative) { throw 'Invalid prepared source path.' }
                if (!$stamps.ContainsKey($relative)) {
                    $source = if ($Sources.ContainsKey($relative)) { $Sources[$relative] } else { Join-Path $Client $relative }
                    Assert-AoSetupPath $source
                    $item = Get-Item -LiteralPath $source -Force
                    if ($item.PSIsContainer) { throw 'Prepared source is a folder.' }
                    $stamps[$relative] = "`t" + $relative + "`t" + $item.Length.ToString([Globalization.CultureInfo]::InvariantCulture) + "`t" + $item.LastWriteTimeUtc.ToFileTimeUtc().ToString([Globalization.CultureInfo]::InvariantCulture)
                }
                $writer.Write($stamps[$relative])
            }
            $writer.WriteLine()
        }
    } finally { $writer.Dispose() }
}
