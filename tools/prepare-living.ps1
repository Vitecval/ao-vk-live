param([string]$ClientDir, [switch]$NewlandTest, [switch]$CityTest)
$ErrorActionPreference='Stop'
if (!$ClientDir) {
    Add-Type -AssemblyName System.Windows.Forms
    $picker=New-Object Windows.Forms.FolderBrowserDialog
    $picker.Description='Select the Anarchy Online folder. Local resources stay inside this folder.'
    if ($picker.ShowDialog() -ne 'OK') { return }
    $ClientDir=$picker.SelectedPath
}
$client=(Resolve-Path -LiteralPath $ClientDir).Path
foreach ($name in @('AnarchyOnline','Anarchy','AnarchyPatcher','randy_harness')) {
    foreach ($running in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
        $runningPath=$null
        try { $runningPath=$running.Path } catch { }
        if (!$runningPath) { throw 'Cannot identify a running AO installation. Close that client or launcher before preparation.' }
        if ($runningPath.StartsWith($client.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Close clients and launchers from the selected AO folder before local asset preparation.' }
    }
}
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach ($item in $manifest.client.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $client $item.Name) -Algorithm SHA256).Hash -ine $item.Value) { throw 'Unsupported AO client build.' }
}
foreach ($item in $manifest.livingAssets.files.PSObject.Properties) {
    if ($item.Name.Contains('..') -or $item.Name -notmatch '^(living-preparer|living-metadata)/[A-Za-z0-9_./-]+$' -or
        (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $item.Name) -Algorithm SHA256).Hash -ine $item.Value) { throw 'Local preparation payload integrity failed.' }
}
$helper=Join-Path $PSScriptRoot 'living-preparer/ao-vk-living-prepare.exe'
if ((Get-FileHash -LiteralPath $helper -Algorithm SHA256).Hash -ine $manifest.livingAssets.helperSha256) { throw 'Preparation helper integrity failed.' }
$arguments=@('--client',$client,'--metadata',(Join-Path $PSScriptRoot 'living-metadata'))
if ($NewlandTest) { $arguments+=@('--playfield','566','--review-routes') }
elseif ($CityTest) { $arguments+='--review-routes' }
& $helper @arguments
if ($LASTEXITCODE) { throw "Local asset preparation failed ($LASTEXITCODE)." }
Write-Output 'Prepared locally. Open Ctrl+Shift+F12 > Living Rubi-Ka. Start with Ambient People Low; the new appearance and aircraft settings are independently optional.'
