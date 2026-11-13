# Real installer/reset scripts against isolated dummy files. No retail files.
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath
    if ($LASTEXITCODE) { throw 'Installer option checks failed.' }; return
}
$root=Split-Path $PSScriptRoot -Parent
$out=Join-Path $root ('build\installer-options-'+[guid]::NewGuid().ToString('N'))
$client=Join-Path $out 'AO [personal]'
$package=Join-Path $out 'setup'
$loader=Join-Path $out 'shared loader'
New-Item -ItemType Directory -Path $client,$package,$loader,(Join-Path $client 'randy-vk\loading') -Force | Out-Null
function Put([string]$Path,[string]$Text) { [IO.File]::WriteAllText($Path,$Text) }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Assert($Ok,[string]$Why) { if (!$Ok) { throw $Why } }
foreach($name in @('randy31.dll','AnarchyOnline.exe','randy-vk.ini')) { Put (Join-Path $client $name) ('keep-'+$name) }
foreach($name in @('ao-vk.dll','ddraw.dll')) { Put (Join-Path $package $name) $name; Copy-Item -LiteralPath (Join-Path $package $name) -Destination $client }
Copy-Item -LiteralPath (Join-Path $package 'ddraw.dll') -Destination $loader
Put (Join-Path $loader 'AnarchyOnline.exe') 'keep-loader-exe'
Put (Join-Path $client 'randy-vk\loading\owned.png') 'owned image'
Put (Join-Path $client 'randy-vk\loading\modified.png') 'expected image'
$addon=@{'ao-vk.dll'=(Hash (Join-Path $package 'ao-vk.dll'));'ddraw.dll'=(Hash (Join-Path $package 'ddraw.dll'))}
$assets=@{'randy-vk/loading/owned.png'=(Hash (Join-Path $client 'randy-vk\loading\owned.png'));'randy-vk/loading/modified.png'=(Hash (Join-Path $client 'randy-vk\loading\modified.png'))}
$manifest=@{product='AO-VK Live';build='fixture';client=@{'randy31.dll'=(Hash (Join-Path $client 'randy31.dll'))};addon=$addon;assets=$assets}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'manifest.json')
$receipt=@{product='AO-VK Live';clientDirectory=$client;loaderDirectory=$loader;addon=$addon;assets=$assets.Clone()}
# An unrecognized receipt entry must never redirect the uninstall outside its scope.
$receipt.assets['../personal.txt']='0'*64
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json')
Put (Join-Path $out 'personal.txt') 'outside preserved'
Put (Join-Path $client 'ddraw.dll') 'personal wrapper'
Put (Join-Path $client 'randy-vk\loading\modified.png') 'personal image'
$global:OptionsSharedModule=Join-Path $loader 'ddraw.dll'
function global:Get-Process {
    param($Name,$ErrorAction)
    if ($Name -eq 'AnarchyOnline' -and $global:OptionsBusyClient) {
        [pscustomobject]@{HasExited=$false;Path=(Join-Path $global:OptionsBusyClient 'AnarchyOnline.exe');Modules=@()}
    } elseif ($Name -eq 'AnarchyOnline' -and $global:OptionsSharedModule) {
        [pscustomobject]@{HasExited=$false;Path='X:\Other AO\AnarchyOnline.exe';Modules=@(
            [pscustomobject]@{FileName='X:\Other AO\randy31.dll'},[pscustomobject]@{FileName=$global:OptionsSharedModule})}
    }
}
$result=@(& (Join-Path $root 'tools\install-live.ps1') -ClientDir $client -PackageDir $package -Uninstall)
Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Known renderer was not removed'
Assert (!(Test-Path -LiteralPath (Join-Path $client 'randy-vk\loading\owned.png'))) 'Owned loading image was not removed'
Assert ([IO.File]::ReadAllText((Join-Path $client 'ddraw.dll')) -eq 'personal wrapper') 'Personal wrapper changed'
Assert ([IO.File]::ReadAllText((Join-Path $client 'randy-vk\loading\modified.png')) -eq 'personal image') 'Personal image changed'
Assert ((Hash (Join-Path $loader 'ddraw.dll')) -eq $addon.'ddraw.dll') 'In-use shared loader changed'
Assert ([IO.File]::ReadAllText((Join-Path $out 'personal.txt')) -eq 'outside preserved') 'Outside file changed'
Assert (Test-Path -LiteralPath (Join-Path $client 'randy-vk.ini')) 'Uninstall removed settings'
$global:OptionsSharedModule=''
# Reset is idempotent and affects exactly the INI, with no receipt/manifest needed.
Put (Join-Path $client 'ao-vk.dll') 'renderer still installed'
foreach($run in 1,2) { & (Join-Path $root 'tools\reset-settings.ps1') -ClientDir $client | Out-Null }
Assert (!(Test-Path -LiteralPath (Join-Path $client 'randy-vk.ini'))) 'Settings were not reset'
Assert ([IO.File]::ReadAllText((Join-Path $client 'ao-vk.dll')) -eq 'renderer still installed') 'Reset removed renderer'
Assert ([IO.File]::ReadAllText((Join-Path $client 'randy31.dll')) -eq 'keep-randy31.dll') 'Reset changed stock file'
Assert ([IO.File]::ReadAllText((Join-Path $client 'randy-vk\loading\modified.png')) -eq 'personal image') 'Reset changed artwork'
# Interrupted setup recovery must not write while the selected client is open.
Put (Join-Path $client '.ao-vk-setup-transaction.json') '{}'
$global:OptionsBusyClient=$client
$blocked=$false
try { & (Join-Path $root 'tools\install-live.ps1') -ClientDir $client -PackageDir $package -Uninstall | Out-Null }
catch { $blocked=$_.Exception.Message -like '*Close this AO client*' }
Assert $blocked 'Recovery did not respect the running target client'
Assert ([IO.File]::ReadAllText((Join-Path $client '.ao-vk-setup-transaction.json')) -eq '{}') 'Busy recovery changed its journal'
Assert ([IO.File]::ReadAllText((Join-Path $client 'ao-vk.dll')) -eq 'renderer still installed') 'Busy recovery changed renderer'
$global:OptionsBusyClient=''
Remove-Item -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json')
Write-Output 'PASS: loading-image ownership, unknown wrapper/image/receipt paths, shared loader in use, settings preservation and reset-only idempotence.'
Write-Output $out
