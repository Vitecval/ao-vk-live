# Self-contained installer tests. Fixtures contain dummy bytes, never game binaries.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $root ('build\installer-tests-' + [guid]::NewGuid().ToString('N'))
$client = Join-Path $fixture 'AO folder with spaces'
$package = Join-Path $fixture 'package'
$loader = Join-Path $fixture 'AppData\AOPP'
New-Item -ItemType Directory -Force $client, $package, $loader | Out-Null
$installer = Join-Path $root 'tools\install-live.ps1'
$savedAppData = $env:APPDATA
$env:APPDATA = Join-Path $fixture 'AppData'
$global:AoInstallerTestBusy = $false
$global:AoInstallerTestFailTarget = ''
function global:Get-Process {
    param($Name, $ErrorAction)
    if ($global:AoInstallerTestBusy -and $Name -eq 'AnarchyOnline') { [pscustomobject]@{ Id = 123 } }
}
function global:Move-Item {
    param($LiteralPath, $Destination)
    if ($Destination -eq $global:AoInstallerTestFailTarget -and $LiteralPath -like '*.installing-*') {
        throw 'simulated commit failure'
    }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
function Write-Bytes($Path, $Value) { [IO.File]::WriteAllText($Path, $Value) }
function Hash($Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert($Condition, $Message) { if (!$Condition) { throw "ASSERT: $Message" } }
function Reject($Arguments, $Message) {
    $caught = $false
    try { & $installer @Arguments | Out-Null } catch {
        $caught = $true
        Assert ($_.Exception.Message -like ('*' + $Message + '*')) ('Unexpected rejection: ' + $_.Exception.Message)
    }
    Assert $caught ('Expected rejection: ' + $Message)
}
function Save-Manifest { $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'manifest.json') }
try {
    $originals = [ordered]@{}
    foreach ($name in @('randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll')) {
        Write-Bytes (Join-Path $client $name) ('stock ' + $name)
        $originals[$name] = Hash (Join-Path $client $name)
    }
    Write-Bytes (Join-Path $client 'AnarchyOnline.exe') 'fixture executable, not executable code'
    Write-Bytes (Join-Path $client 'randy-vk.ini') "[Renderer]`r`nRVK_GrassOn=1`r`nRVK_PerfHud=0"
    $preferences = Hash (Join-Path $client 'randy-vk.ini')
    Write-Bytes (Join-Path $package 'ao-vk.dll') 'addon version one'
    Write-Bytes (Join-Path $package 'ddraw.dll') 'loader version one'
    $oldHashes = @{ 'ao-vk.dll' = (Hash (Join-Path $package 'ao-vk.dll')); 'ddraw.dll' = (Hash (Join-Path $package 'ddraw.dll')) }
    $manifest = @{ build = 'fixture-v1'; client = $originals; addon = $oldHashes; previousAddons = @() }
    Save-Manifest
    $arguments = @{ ClientDir = $client; PackageDir = $package }
    $global:AoInstallerTestBusy = $true
    Reject $arguments 'Close the game'
    $global:AoInstallerTestBusy = $false
    Write-Bytes (Join-Path $client 'ddraw.dll') 'unrelated wrapper'
    Reject $arguments 'already exists'
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Refused install changed files'
    Remove-Item -LiteralPath (Join-Path $client 'ddraw.dll')
    Write-Bytes (Join-Path $client 'N3.dll') 'unsupported client'
    Reject $arguments 'Unsupported or modified'
    Write-Bytes (Join-Path $client 'N3.dll') 'stock N3.dll'
    & $installer @arguments | Out-Null
    Assert (Test-Path -LiteralPath (Join-Path $client 'ao-vk-live.install.json')) 'Install record absent'
    Reject $arguments 'already exists'
    Write-Bytes (Join-Path $package 'ao-vk.dll') 'addon version two'
    Write-Bytes (Join-Path $package 'ddraw.dll') 'loader version two'
    $manifest.build = 'fixture-v2'
    $manifest.addon = @{ 'ao-vk.dll' = (Hash (Join-Path $package 'ao-vk.dll')); 'ddraw.dll' = (Hash (Join-Path $package 'ddraw.dll')) }
    Save-Manifest
    $arguments.Upgrade = $true
    $global:AoInstallerTestFailTarget = Join-Path $client 'ddraw.dll'
    Reject $arguments 'simulated commit failure'
    $global:AoInstallerTestFailTarget = ''
    foreach ($name in @('ao-vk.dll', 'ddraw.dll')) {
        Assert ((Hash (Join-Path $client $name)) -eq $oldHashes[$name]) 'Failed update did not restore previous files'
    }
    $oldRecord = Get-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json') -Raw | ConvertFrom-Json
    Assert ($oldRecord.build -eq 'fixture-v1') 'Failed update changed the install record'
    Write-Bytes (Join-Path $loader 'AnarchyOnline.exe') 'relocated executable fixture'
    $arguments.LoaderDir = $loader
    & $installer @arguments | Out-Null
    Assert ((Hash (Join-Path $loader 'ddraw.dll')) -eq $manifest.addon.'ddraw.dll') 'Extra loader absent'
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq $preferences) 'Preferences changed'
    Remove-Item -LiteralPath (Join-Path $loader 'AnarchyOnline.exe')
    Write-Bytes (Join-Path $client 'randy31.dll') 'a later official update'
    Write-Bytes (Join-Path $client 'ddraw.dll') 'replacement wrapper not owned by AO-VK'
    $uninstall = @{ ClientDir = $client; PackageDir = $package; Uninstall = $true }
    Reject $uninstall 'unrecognized'
    Assert (Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll')) 'Rejected uninstall partially removed addon'
    Copy-Item -LiteralPath (Join-Path $package 'ddraw.dll') -Destination (Join-Path $client 'ddraw.dll') -Force
    & $installer @uninstall | Out-Null
    foreach ($target in @((Join-Path $client 'ao-vk.dll'), (Join-Path $client 'ddraw.dll'),
            (Join-Path $client 'ao-vk-live.install.json'), (Join-Path $loader 'ddraw.dll'))) {
        Assert (!(Test-Path -LiteralPath $target)) ('Uninstall left ' + $target)
    }
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq $preferences) 'Uninstall changed preferences'
    Assert ([IO.File]::ReadAllText((Join-Path $client 'randy31.dll')) -eq 'a later official update') 'Uninstall changed stock renderer'
    Write-Output 'PASS: busy guard, wrapper refusal, build validation, install, duplicate refusal, update rollback, receipt-based update, extra loader, uninstall after client update, modified-file refusal, preferences preserved.'
} finally {
    $env:APPDATA = $savedAppData
    Remove-Item Function:\Get-Process -ErrorAction SilentlyContinue
    Remove-Item Function:\Move-Item -ErrorAction SilentlyContinue
    Remove-Variable AoInstallerTestBusy, AoInstallerTestFailTarget -Scope Global -ErrorAction SilentlyContinue
}
