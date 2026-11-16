# End-to-end dummy installs; no retail game files or processes are touched.
param([string]$Output='')
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Output $Output
    if ($LASTEXITCODE) { throw 'Asset index tests failed in the installer host.' }; return
}
$root=Split-Path $PSScriptRoot -Parent
if (!$Output) { $Output=Join-Path $root ('build/asset-index-tests-'+[guid]::NewGuid().ToString('N')) }
$Output=[IO.Path]::GetFullPath($Output)
$client=Join-Path $Output 'AO fixture'; $package=Join-Path $Output 'package'
New-Item -ItemType Directory -Force $client,$package | Out-Null
$installer=Join-Path $root 'tools/install-live.ps1'
function global:Get-Process { param($Name,$ErrorAction) }
$global:IndexAssetReads=0
function global:Get-FileHash {
    param($LiteralPath,$Algorithm)
    if ($LiteralPath -like '*AO fixture*randy-vk*') { $global:IndexAssetReads++ }
    Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm
}
function Hash($path) { (Microsoft.PowerShell.Utility\Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert($condition,$message) { if (!$condition) { throw "ASSERT: $message" } }
function WriteBytes($path,$text) { [IO.File]::WriteAllText($path,$text) }
$checks=New-Object 'Collections.Generic.List[string]'
function Run($full=$false) {
    $global:IndexAssetReads=0
    $result=@(& $installer -ClientDir $client -PackageDir $package -Upgrade -VerifyAllAssets:$full) | Where-Object { $_.AoVkSetupResult }
    Assert ($null -ne $result) 'Installer returned no result'
    return $result
}
function SaveManifest { $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'manifest.json') -Encoding UTF8 }
try {
    WriteBytes (Join-Path $client 'AnarchyOnline.exe') 'dummy not executable'
    WriteBytes (Join-Path $client 'randy31.dll') 'original untouched'
    WriteBytes (Join-Path $package 'ao-vk.dll') 'dummy renderer'
    WriteBytes (Join-Path $package 'ddraw.dll') 'dummy loader'
    $assets=@{}; $sourceFolder=Join-Path $package 'randy-vk/packs/ao-cyber-world'
    New-Item -ItemType Directory -Force $sourceFolder | Out-Null
    foreach($i in 1..80) {
        $relative="randy-vk/packs/ao-cyber-world/1010004_${i}_d.png"
        WriteBytes (Join-Path $package $relative) (('asset-'+$i+';')*400)
        $assets[$relative]=Hash (Join-Path $package $relative)
    }
    $manifest=@{build='index-fixture';edition='full';client=@{'randy31.dll'=(Hash (Join-Path $client 'randy31.dll'))};addon=@{'ao-vk.dll'=(Hash (Join-Path $package 'ao-vk.dll'));'ddraw.dll'=(Hash (Join-Path $package 'ddraw.dll'))};assets=$assets}
    SaveManifest
    $first=Run
    Assert ($first.assetsReadInFull -eq 80 -and $first.indexedAssetsReused -eq 0) 'First install must verify every asset'
    $checks.Add('Existing installations without an index receive full verification before indexing')
    $index=Join-Path $client '.ao-vk-asset-index-v1.bin'
    Assert (Test-Path -LiteralPath $index) 'Index not saved'
    $second=Run
    Assert ($second.indexedAssetsReused -eq 80 -and $second.assetsReadInFull -eq 0 -and $global:IndexAssetReads -eq 0) 'Unchanged indexed artwork was reread'
    $checks.Add('Routine updates reuse all unchanged verified assets without content reads')
    $repair=Run $true
    Assert ($repair.assetsReadInFull -eq 80 -and $repair.indexedAssetsReused -eq 0) 'Repair used the index'
    $checks.Add('Repair forces complete asset hashing')
    $one='randy-vk/packs/ao-cyber-world/1010004_1_d.png'; $file=Join-Path $client $one
    Remove-Item -LiteralPath $file
    $missing=Run
    Assert ($missing.indexedAssetsReused -eq 79 -and (Hash $file) -eq $assets[$one]) 'A deleted indexed file was not restored'
    $checks.Add('Missing files are detected and restored while other assets remain cached')
    WriteBytes (Join-Path $package $one) 'updated approved asset'
    $assets[$one]=Hash (Join-Path $package $one); SaveManifest
    $updated=Run
    Assert ($updated.indexedAssetsReused -eq 79 -and (Hash $file) -eq $assets[$one]) 'Changed manifest did not replace the asset'
    $checks.Add('Updated asset hashes trigger byte verification and refresh the index')
    $null=Run
    $stamp=(Get-Item -LiteralPath $file).LastWriteTimeUtc
    WriteBytes $file ('X'*(Get-Item -LiteralPath $file).Length)
    (Get-Item -LiteralPath $file).LastWriteTimeUtc=$stamp
    $rejected=$false; try { $null=Run $true } catch { $rejected=$_.Exception.Message -like '*unrecognized existing asset*' }
    Assert $rejected 'Repair failed to detect a same-size edit with restored timestamp'
    $checks.Add('Full Repair catches edits even when file size and timestamp are preserved')
    [IO.File]::Copy((Join-Path $package $one),$file,$true)
    $null=Run $true
    $valid=[IO.File]::ReadAllBytes($index); $broken=$valid.Clone(); $broken[20]=$broken[20] -bxor 1; [IO.File]::WriteAllBytes($index,$broken)
    $corrupt=Run
    Assert ($corrupt.indexedAssetsReused -eq 0 -and $corrupt.assetsReadInFull -eq 80) 'Corrupt index was trusted'
    $checks.Add('Index checksum corruption falls back to full verification and rebuilds')
    [IO.File]::AppendAllText((Join-Path $client 'ao-vk-live.install.json'),"`n")
    $stale=Run
    Assert ($stale.indexedAssetsReused -eq 0) 'Stale receipt binding was trusted'
    $checks.Add('A changed or interrupted installation receipt invalidates the index')
    . (Join-Path $root 'tools/asset-index.ps1'); Initialize-AoAssetIndex
    $receiptHash=Hash (Join-Path $client 'ao-vk-live.install.json')
    $loaded=[AoVk.AssetIndexV1]::Load($client,$receiptHash)
    foreach($entry in $loaded.Values) { $entry.VerifiedUtc=[DateTime]::UtcNow.AddDays(-31).Ticks }
    [AoVk.AssetIndexV1]::Save($client,$receiptHash,$loaded)
    $expired=Run
    Assert ($expired.indexedAssetsReused -eq 0 -and $expired.assetsReadInFull -eq 80) 'Expired verification was extended'
    $checks.Add('30-day full-verification expiry cannot be postponed by routine updates')
    $other='randy-vk/packs/ao-cyber-world/1010004_2_d.png'
    WriteBytes (Join-Path $client $other) 'personal changed asset'
    $assets.Remove($other); SaveManifest
    $removed=Run
    Assert (([IO.File]::ReadAllText((Join-Path $client $other))) -eq 'personal changed asset') 'Index authorized removal of a modified retired file'
    $checks.Add('Removed assets are verified in full and modified user files remain untouched')
    & $installer -ClientDir $client -PackageDir $package -Uninstall | Out-Null
    Assert (Test-Path -LiteralPath (Join-Path $client $other)) 'Uninstall removed an unowned modified file'
    $checks.Add('Uninstall retains full ownership verification independently of the cache')
    @{result='passed';checks=$checks;pointerBytes=[IntPtr]::Size;retailClientsChanged=$false}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $Output 'RESULTS.json') -Encoding UTF8
    Write-Output "PASS: $($checks.Count) asset-index scenarios; $Output"
} finally {
    Remove-Item Function:\Get-Process,Function:\Get-FileHash -ErrorAction SilentlyContinue
}
