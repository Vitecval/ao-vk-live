param([string]$ClientDir, [switch]$NewlandTest)
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
    # A separate preparation command cannot safely inspect all shared x86 loaders
    # from both PowerShell hosts. Ask for a closed session instead of guessing.
    if (Get-Process -Name $name -ErrorAction SilentlyContinue) { throw 'Close AO clients and launchers before local asset preparation.' }
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
& $helper @arguments
if ($LASTEXITCODE) { throw "Local asset preparation failed ($LASTEXITCODE)." }
Write-Output 'Prepared locally. Enable Ambient People Low in the Living Rubi-Ka options group for the isolated Newland test.'
