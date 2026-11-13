# Self-contained installer tests. Fixtures contain dummy bytes, never game binaries.
$ErrorActionPreference = 'Stop'
# Keep mocks and the installer in the same host; exercise Desktop in x86 just
# as the public setup does. Separate host tests cover the x64 forwarding path.
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath
    if ($LASTEXITCODE -ne 0) { throw '32-bit installer fixture suite failed.' }
    return
}
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
$global:AoInstallerTestExited = $false
$global:AoInstallerTestOther = $false
$global:AoInstallerTestSharedPath = ''
$global:AoInstallerTestFailTarget = ''
function global:Get-Process {
    param($Name, $ErrorAction)
    if ($global:AoInstallerTestBusy -and $Name -eq 'AnarchyOnline') { [pscustomobject]@{ Id = 123 } }
    elseif ($global:AoInstallerTestExited -and $Name -eq 'AnarchyOnline') { [pscustomobject]@{ Id = 124; HasExited = $true } }
    elseif ($global:AoInstallerTestOther -and $Name -eq 'AnarchyOnline') {
        [pscustomobject]@{ Id = 125; HasExited = $false; Path = 'X:\Other AO\AnarchyOnline.exe'; Modules = @([pscustomobject]@{ FileName = 'X:\Other AO\randy31.dll' }, [pscustomobject]@{ FileName = $global:AoInstallerTestSharedPath }) }
    }
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
    $manifest.assets=@{'randy-vk/living/ao-transporter.lrm'=('0'*64)}
    Save-Manifest
    Reject $arguments 'separate add-on'
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Prohibited population payload changed renderer'
    $manifest.Remove('assets')
    $manifest.livingAssets=@{status='ready';provider='legacy-private-cache'}
    Save-Manifest
    Reject $arguments 'separate add-on'
    $manifest.livingAssets=@{status='local-test-candidate';provider='installation-local-v1';helperSha256=('0'*64)}
    Save-Manifest
    Reject $arguments 'separate add-on'
    $manifest.Remove('livingAssets')
    Save-Manifest
    $manifest.cacheWarmup=@{helper='ao-vk-cache-warm.exe';relativeCache='../randy31.dll';sha256=('0'*64)}
    Save-Manifest
    Reject $arguments 'Invalid cache preparation descriptor'
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Rejected warmup descriptor changed renderer'
    $manifest.cacheWarmup.relativeCache='randy-vk-preview21-v9-living.cache'
    Write-Bytes (Join-Path $package 'ao-vk-cache-warm.exe') 'not executable and never run'
    Save-Manifest
    Reject $arguments 'helper integrity failed'
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Rejected warmup hash changed renderer'
    $manifest.Remove('cacheWarmup')
    Save-Manifest
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
    $global:AoInstallerTestExited = $true
    & $installer @arguments | Out-Null
    $global:AoInstallerTestExited = $false
    Assert (Test-Path -LiteralPath (Join-Path $client 'ao-vk-live.install.json')) 'Install record absent'
    Reject $arguments 'already exists'
    Write-Bytes (Join-Path $package 'ao-vk.dll') 'addon version two'
    Write-Bytes (Join-Path $package 'ddraw.dll') 'loader version two'
    $manifest.build = 'fixture-v2'
    $manifest.addon = @{ 'ao-vk.dll' = (Hash (Join-Path $package 'ao-vk.dll')); 'ddraw.dll' = (Hash (Join-Path $package 'ddraw.dll')) }
    Save-Manifest
    $arguments.Upgrade = $true
    $global:AoInstallerTestOther = $true
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
    $global:AoInstallerTestSharedPath = Join-Path $loader 'ddraw.dll'
    Reject $arguments 'Close the game'
    $global:AoInstallerTestSharedPath = ''
    & $installer @arguments | Out-Null
    $global:AoInstallerTestOther = $false
    Assert ((Hash (Join-Path $loader 'ddraw.dll')) -eq $manifest.addon.'ddraw.dll') 'Extra loader absent'
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq $preferences) 'Preferences changed'
    # An older DLL-only installation upgrades to the optional pack transactionally.
    $assetRelative = 'randy-vk/packs/ao-modern-icc/1010004_295992_d.png'
    $assetSource = Join-Path $package $assetRelative
    $assetTarget = Join-Path $client $assetRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $assetSource) -Force | Out-Null
    Write-Bytes $assetSource 'fixture texture one'
    $manifest.assets = @{ $assetRelative = (Hash $assetSource) }
    $enhancedRelative = 'randy-vk/packs/ao-icc-enhanced-original/1010004_295610_orm.png'
    $enhancedSource = Join-Path $package $enhancedRelative
    $enhancedTarget = Join-Path $client $enhancedRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $enhancedSource) -Force | Out-Null
    Write-Bytes $enhancedSource 'fixture Enhanced Original material'
    $manifest.assets[$enhancedRelative] = Hash $enhancedSource
    # Setup 4's real manifests include this metadata outside the artwork packs.
    $meshRelative = 'randy-vk/mesh-ids.bin'
    $meshSource = Join-Path $package $meshRelative
    $meshTarget = Join-Path $client $meshRelative
    $meshNames = [Text.Encoding]::ASCII.GetBytes(([string][char]0) + 'scenery_spaceship' + ([string][char]0))
    $meshBytes = New-Object byte[] (52 + $meshNames.Length)
    [Array]::Copy([Text.Encoding]::ASCII.GetBytes('AOMESHID'), $meshBytes, 8)
    [Array]::Copy([BitConverter]::GetBytes([uint32]1), 0, $meshBytes, 8, 4)
    [Array]::Copy([BitConverter]::GetBytes([uint32]1), 0, $meshBytes, 12, 4)
    [Array]::Copy([BitConverter]::GetBytes([uint32]$meshNames.Length), 0, $meshBytes, 16, 4)
    [Array]::Copy($meshNames, 0, $meshBytes, 52, $meshNames.Length)
    [IO.File]::WriteAllBytes($meshSource, $meshBytes)
    $manifest.assets[$meshRelative] = Hash $meshSource
    $manifest.meshIndex = @{ file=$meshRelative; formatVersion=1; sha256=(Hash $meshSource) }
    $cyberRelative = 'randy-vk/packs/ao-cyber-icc/1010004_295610_orm.png'
    $cyberSource = Join-Path $package $cyberRelative
    $cyberTarget = Join-Path $client $cyberRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $cyberSource) -Force | Out-Null
    Write-Bytes $cyberSource 'fixture cyber material'
    $manifest.assets[$cyberRelative] = Hash $cyberSource
    $characterRelative = 'randy-vk/packs/ao-cyber-characters/1010004_256236_d.png'
    $characterSource = Join-Path $package $characterRelative
    $characterTarget = Join-Path $client $characterRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $characterSource) -Force | Out-Null
    Write-Bytes $characterSource 'fixture character artwork'
    $manifest.assets[$characterRelative] = Hash $characterSource
    $worldRelative = 'randy-vk/packs/ao-cyber-world/1010004_6055_d.png'
    $worldSource = Join-Path $package $worldRelative
    $worldTarget = Join-Path $client $worldRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $worldSource) -Force | Out-Null
    Write-Bytes $worldSource 'fixture world artwork'
    $manifest.assets[$worldRelative] = Hash $worldSource
    Save-Manifest
    Write-Bytes $assetSource 'corrupt fixture texture'
    Reject $arguments 'Package integrity check failed'
    Write-Bytes $assetSource 'fixture texture one'
    $global:AoInstallerTestFailTarget = $assetTarget
    Reject $arguments 'simulated commit failure'
    $global:AoInstallerTestFailTarget = ''
    Assert (!(Test-Path -LiteralPath $assetTarget)) 'Asset failure partially installed pack'
    Assert (!(Test-Path -LiteralPath $cyberTarget)) 'Asset failure partially installed cyber pack'
    Assert (!(Test-Path -LiteralPath $characterTarget)) 'Asset failure partially installed character pack'
    Assert (!(Test-Path -LiteralPath $worldTarget)) 'Asset failure partially installed world pack'
    & $installer @arguments | Out-Null
    Assert ((Hash $assetTarget) -eq $manifest.assets[$assetRelative]) 'Pack asset absent'
    Assert ((Hash $cyberTarget) -eq $manifest.assets[$cyberRelative]) 'Cyber pack asset absent'
    Assert ((Hash $enhancedTarget) -eq $manifest.assets[$enhancedRelative]) 'Enhanced Original asset absent'
    Assert ((Hash $meshTarget) -eq $manifest.assets[$meshRelative]) 'Setup 4 model index absent'
    Assert ((Hash $characterTarget) -eq $manifest.assets[$characterRelative]) 'Character pack asset absent'
    Assert ((Hash $worldTarget) -eq $manifest.assets[$worldRelative]) 'World pack asset absent'
    $otherPack = Join-Path $client 'randy-vk/packs/personal/keep.txt'
    New-Item -ItemType Directory -Path (Split-Path -Parent $otherPack) -Force | Out-Null
    Write-Bytes $otherPack 'personal pack'
    Write-Bytes $assetSource 'fixture texture two'
    $manifest.assets[$assetRelative] = Hash $assetSource
    Save-Manifest
    & $installer @arguments | Out-Null
    Assert ((Hash $assetTarget) -eq $manifest.assets[$assetRelative]) 'Pack asset update failed'
    # An already verified HD installation must update without re-downloading or
    # unpacking the same texture just to verify an unused package-side copy.
    $manifest.materialParts=@(@{name='ao-vk-live-fixture-materials-1.zip';sha256=('0'*64);bytes=1;unpackedBytes=1;files=@($assetRelative)})
    Move-Item -LiteralPath $assetSource -Destination ($assetSource+'.saved')
    Save-Manifest
    & $installer @arguments | Out-Null
    Assert ((Hash $assetTarget) -eq $manifest.assets[$assetRelative]) 'Verified texture was changed during archive-free update'
    Move-Item -LiteralPath ($assetSource+'.saved') -Destination $assetSource
    $manifest.Remove('materialParts')
    $manifest.assets['../escape.png'] = Hash $assetSource
    Save-Manifest
    Reject $arguments 'Invalid optional material pack'
    $manifest.assets.Remove('../escape.png')
    foreach ($unsafeIndex in @('randy-vk/mesh-idsXbin', 'randy-vk/other.bin', 'randy-vk/../mesh-ids.bin')) {
        $manifest.assets[$unsafeIndex] = Hash $meshSource
        Save-Manifest
        Reject $arguments 'Invalid optional material pack'
        $manifest.assets.Remove($unsafeIndex)
    }
    Save-Manifest
    # Full -> Lite removes only owned optional files transactionally, preserving settings.
    $fullAssets=$manifest.assets.Clone()
    $manifest.assets=@{ $meshRelative=$fullAssets[$meshRelative] }
    Save-Manifest
    # A removal rollback is tested by failing receipt replacement.
    $global:AoInstallerTestFailTarget=Join-Path $client 'ao-vk-live.install.json'
    Reject $arguments 'simulated commit failure'
    $global:AoInstallerTestFailTarget=''
    Assert (Test-Path -LiteralPath $assetTarget) 'Failed Lite switch lost previous HD assets'
    & $installer @arguments | Out-Null
    foreach ($path in @($assetTarget,$cyberTarget,$characterTarget,$worldTarget,$enhancedTarget)) {Assert (!(Test-Path -LiteralPath $path)) 'Lite switch left HD texture'}
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq $preferences) 'Edition switch changed preferences'
    $manifest.assets=$fullAssets
    Save-Manifest
    & $installer @arguments | Out-Null
    Assert (Test-Path -LiteralPath $worldTarget) 'Lite -> Full failed to restore artwork'
    # An unrelated running client may keep an identical shared loader mapped. No replace/move is needed.
    $global:AoInstallerTestOther = $true
    $global:AoInstallerTestSharedPath = Join-Path $loader 'ddraw.dll'
    $sharedLock = [IO.File]::Open($global:AoInstallerTestSharedPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try { & $installer @arguments | Out-Null } finally { $sharedLock.Dispose() }
    Assert ((Hash (Join-Path $loader 'ddraw.dll')) -eq $manifest.addon.'ddraw.dll') 'Identical shared loader changed'
    $global:AoInstallerTestOther = $false
    $global:AoInstallerTestSharedPath = ''
    Remove-Item -LiteralPath (Join-Path $loader 'AnarchyOnline.exe')
    Write-Bytes (Join-Path $client 'randy31.dll') 'a later official update'
    Write-Bytes (Join-Path $client 'ddraw.dll') 'replacement wrapper not owned by AO-VK'
    $uninstall = @{ ClientDir = $client; PackageDir = $package; Uninstall = $true }
    Write-Bytes $assetTarget 'personal modification'
    & $installer @uninstall | Out-Null
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Uninstall did not continue past modified files'
    Assert ([IO.File]::ReadAllText((Join-Path $client 'ddraw.dll')) -eq 'replacement wrapper not owned by AO-VK') 'Uninstall changed unknown wrapper'
    Assert ([IO.File]::ReadAllText($assetTarget) -eq 'personal modification') 'Uninstall changed modified artwork'
    Assert (!(Test-Path -LiteralPath $cyberTarget)) 'Uninstall left cyber pack asset'
    Assert (!(Test-Path -LiteralPath $enhancedTarget)) 'Uninstall left Enhanced Original asset'
    Assert (!(Test-Path -LiteralPath $meshTarget)) 'Uninstall left owned model index'
    Assert (!(Test-Path -LiteralPath $characterTarget)) 'Uninstall left character pack asset'
    Assert (!(Test-Path -LiteralPath $worldTarget)) 'Uninstall left world pack asset'
    Assert ([IO.File]::ReadAllText($otherPack) -eq 'personal pack') 'Uninstall changed personal pack'
    foreach ($target in @((Join-Path $client 'ao-vk.dll'),
            (Join-Path $client 'ao-vk-live.install.json'), (Join-Path $loader 'ddraw.dll'))) {
        Assert (!(Test-Path -LiteralPath $target)) ('Uninstall left ' + $target)
    }
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq $preferences) 'Uninstall changed preferences'
    Assert ([IO.File]::ReadAllText((Join-Path $client 'randy31.dll')) -eq 'a later official update') 'Uninstall changed stock renderer'
    Write-Output 'PASS: process guards, locked identical shared loader, wrapper refusal, build validation, install/update/rollback, optional pack install/update/rollback/uninstall, unsafe path refusal, modified files skipped while recognized files uninstall, personal packs and preferences preserved.'
} finally {
    $env:APPDATA = $savedAppData
    Remove-Item Function:\Get-Process -ErrorAction SilentlyContinue
    Remove-Item Function:\Move-Item -ErrorAction SilentlyContinue
    Remove-Variable AoInstallerTestBusy, AoInstallerTestExited, AoInstallerTestOther, AoInstallerTestSharedPath, AoInstallerTestFailTarget -Scope Global -ErrorAction SilentlyContinue
}
