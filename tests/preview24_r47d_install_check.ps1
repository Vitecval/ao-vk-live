$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$work=Join-Path $root 'build\preview24\zoning-r47d'
$client=Join-Path $work 'installer-fixture-client'
if(Test-Path -LiteralPath $client){throw 'Use a fresh fixture directory for this one-shot check.'}
New-Item -ItemType Directory -Path $client | Out-Null
$renderer=Join-Path $client 'ao-vk.dll';$receiptPath=Join-Path $client 'ao-vk-live.install.json';$ini=Join-Path $client 'randy-vk.ini'
[IO.File]::WriteAllText($renderer,'previous renderer fixture')
[IO.File]::WriteAllText($ini,"[Renderer]`r`nRVK_SunDist=123`r`nRVK_EnhancedRocks=0.4`r`n; user preference`r`n")
$hash={param($path)(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
$original=& $hash $renderer;$iniHash=& $hash $ini
@{product='AO-VK Live';clientDirectory=$client;addon=@{'ao-vk.dll'=$original};assets=@{'fixture.txt'='preserved'}}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receiptPath
$oldReceipt=& $hash $receiptPath
$installer=Join-Path $root 'tools\install-preview24-r47d.ps1'
& $installer -Client $client -Fixture
$installed=Get-Content -Raw -LiteralPath (Join-Path $work 'INSTALL-FIXTURE.json')|ConvertFrom-Json
if((& $hash $renderer) -cne $installed.rendererSha256 -or (& $hash $ini) -cne $iniHash){throw 'Install verification failed'}
& $installer -Client $client -Fixture
[IO.File]::AppendAllText($renderer,'user change')
$changed=& $hash $renderer;$refused=$false
try{& $installer -Client $client -Fixture}catch{$refused=$true}
if(!$refused -or (& $hash $renderer) -cne $changed){throw 'Ownership guard failed'}
Copy-Item -LiteralPath (Join-Path $work 'candidate\ao-vk.dll') -Destination $renderer
& $installer -Client $client -Fixture -Revert
if((& $hash $renderer) -cne $original -or (& $hash $receiptPath) -cne $oldReceipt -or (& $hash $ini) -cne $iniHash){throw 'Revert verification failed'}
@{result='passed';install=$true;idempotency=$true;ownershipRefusal=$true;exactRevert=$true;settingsPreserved=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'INSTALLER-TESTS.json')
Write-Output 'Install, repeat install, changed-file protection and exact revert passed.'
