# Exercise the actual local r42 updater without any retail writes.
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath
    if($LASTEXITCODE){throw 'Local updater fixture failed'};return
}
$root=Split-Path $PSScriptRoot -Parent
$work=Join-Path $root 'build\preview23\atmosphere-r42'
$folder=Join-Path $work ('updater-fixture-'+[guid]::NewGuid().ToString('N'))
$client=Join-Path $folder 'AO [test]'
$audit=Join-Path $folder 'audit'
New-Item -ItemType Directory -Path $client,(Join-Path $client 'randy-vk'),$audit,(Join-Path $audit 'candidate') -Force | Out-Null
foreach($name in @('ao-vk.dll','Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')) {
    [IO.File]::WriteAllText((Join-Path $client $name),'original-'+$name)
}
$hash=(Get-FileHash -LiteralPath (Join-Path $client 'ao-vk.dll') -Algorithm SHA256).Hash
@{product='AO-VK Live';clientDirectory=$client;addon=@{'ao-vk.dll'=$hash};unrelated='keep'} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json')
$original="[Renderer]`r`nRVK_SkipHugeShadows=1`r`nRVK_AmbientBright=2.35`r`nOtherChoice=17`r`n[Other]`r`nKeepMe=yes`r`n"
[IO.File]::WriteAllText((Join-Path $client 'randy-vk.ini'),$original)
Copy-Item -LiteralPath (Join-Path $work 'candidate\ao-vk.dll') -Destination (Join-Path $audit 'candidate\ao-vk.dll')
$expected=(Get-FileHash -LiteralPath (Join-Path $audit 'candidate\ao-vk.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
@{result='passed';rendererSha256=$expected;scope='Dummy updater fixture only'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $audit 'VERIFIED.json')
function global:Get-Process {param($Name,$ErrorAction)}
$script=Get-Content -LiteralPath (Join-Path $root 'tools\install-preview23-r42.ps1') -Raw
$script=$script.Replace('F:\AO_SInstall\Anarchy Online',$client)
$script=$script.Replace('$root=Split-Path $PSScriptRoot -Parent',('$root='''+$root+''''))
$script=$script.Replace('$work=Join-Path $root ''build\preview23\atmosphere-r42''',('$work='''+$audit+''''))
$fixtureScript=Join-Path $folder 'updater.ps1'
[IO.File]::WriteAllText($fixtureScript,$script)
& $fixtureScript -Client $client
if([IO.File]::ReadAllText((Join-Path $client 'randy-vk.ini')) -cne $original.Replace('RVK_SkipHugeShadows=1','RVK_SkipHugeShadows=0')) { throw 'Updater changed unrelated preferences' }
$receipt=Get-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json') -Raw | ConvertFrom-Json
if($receipt.unrelated -ne 'keep') { throw 'Updater changed unrelated receipt data' }
if((Get-FileHash -LiteralPath (Join-Path $client 'ao-vk.dll')).Hash -ine $expected) { throw 'Updater renderer mismatch' }
# Settings reset followed by update must work with no INI at all.
Remove-Item -LiteralPath (Join-Path $client 'randy-vk.ini')
& $fixtureScript -Client $client
if([IO.File]::ReadAllText((Join-Path $client 'randy-vk.ini')) -notmatch 'RVK_SkipHugeShadows=0') { throw 'Missing INI update failed' }
@{result='passed';rendererSha256=$expected;scope='Actual local updater against isolated dummy client; custom preferences, receipt data and missing INI';fixture=$folder} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'UPDATER-CHECK.json') -Encoding UTF8
Write-Output 'Local updater check passed: custom preferences, receipt, renderer, missing INI and protected files.'
