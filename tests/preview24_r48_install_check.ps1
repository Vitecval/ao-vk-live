$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$work=Join-Path $root 'build\preview24\rocks-r48\production'
$client=Join-Path $work 'installer-fixture-client'
if(Test-Path -LiteralPath $client){throw 'Use a fresh fixture directory for this one-shot check.'}
New-Item -ItemType Directory -Path $client | Out-Null
. (Join-Path $root 'tools\setup-safety.ps1')
$renderer=Join-Path $client 'ao-vk.dll';$receiptPath=Join-Path $client 'ao-vk-live.install.json';$ini=Join-Path $client 'randy-vk.ini'
[IO.File]::WriteAllText($renderer,'previous renderer fixture')
[IO.File]::WriteAllText($ini,"[Renderer]`r`nRVK_SunDist=123`r`nRVK_EnhancedRocks=0.4`r`nRVK_ZoneClickContinue=1`r`n; user preference`r`n")
$cache=Join-Path $client 'randy-vk\cache\materials\0123456789abcdef.rvkm'
New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($cache)) | Out-Null
[IO.File]::WriteAllText($cache,'prepared user cache preserved')
$pack=Join-Path $client 'randy-vk\packs\EnhancedRocks'
New-Item -ItemType Directory -Path $pack | Out-Null
$diff=Join-Path $pack 'rock_face_03_diff_2k.jpg';[IO.File]::WriteAllText($diff,'previous owned diffuse')
$arm=Join-Path $pack 'rock_face_03_arm_1k.jpg';[IO.File]::WriteAllText($arm,'unowned user surface')
$original=Get-AoSetupHash $renderer;$iniHash=Get-AoSetupHash $ini;$cacheHash=Get-AoSetupHash $cache;$oldDiff=Get-AoSetupHash $diff;$unowned=Get-AoSetupHash $arm
@{product='AO-VK Live';clientDirectory=$client;addon=@{'ao-vk.dll'=$original};assets=@{'randy-vk/packs/EnhancedRocks/rock_face_03_diff_2k.jpg'=$oldDiff}}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receiptPath
$oldReceipt=Get-AoSetupHash $receiptPath
$installer=Join-Path $root 'tools\install-preview24-r48.ps1'
$refused=$false;try{& $installer -Client $client -Fixture}catch{$refused=$true}
if(!$refused -or (Get-AoSetupHash $arm) -cne $unowned -or (Get-AoSetupHash $renderer) -cne $original){throw 'Unowned asset protection failed'}
Remove-Item -LiteralPath $arm
& $installer -Client $client -Fixture
$installed=Read-AoSetupJson (Join-Path $work 'INSTALL-FIXTURE.json')
if((Get-AoSetupHash $renderer) -cne $installed.rendererSha256 -or (Get-AoSetupHash $ini) -cne $iniHash){throw 'Install verification failed'}
$installedReceipt=Get-AoSetupHash $receiptPath
& $installer -Client $client -Fixture
if((Get-AoSetupHash $receiptPath) -cne $installedReceipt){throw 'Repeat install changed receipt'}
[IO.File]::AppendAllText($renderer,'user change')
$changed=Get-AoSetupHash $renderer;$refused=$false
try{& $installer -Client $client -Fixture}catch{$refused=$true}
if(!$refused -or (Get-AoSetupHash $renderer) -cne $changed){throw 'Renderer ownership guard failed'}
Copy-Item -LiteralPath (Join-Path $work 'candidate\ao-vk.dll') -Destination $renderer
$normal=Join-Path $pack 'rock_face_03_nor_gl_2k.jpg';[IO.File]::AppendAllText($normal,'user change')
$changed=Get-AoSetupHash $normal;$refused=$false
try{& $installer -Client $client -Fixture -Revert}catch{$refused=$true}
if(!$refused -or (Get-AoSetupHash $normal) -cne $changed){throw 'Changed surface revert protection failed'}
Copy-Item -LiteralPath (Join-Path $work 'candidate\randy-vk\packs\EnhancedRocks\rock_face_03_nor_gl_2k.jpg') -Destination $normal
& $installer -Client $client -Fixture -Revert
if((Get-AoSetupHash $renderer) -cne $original -or (Get-AoSetupHash $receiptPath) -cne $oldReceipt -or (Get-AoSetupHash $ini) -cne $iniHash -or (Get-AoSetupHash $cache) -cne $cacheHash -or (Get-AoSetupHash $diff) -cne $oldDiff){throw 'Exact revert verification failed'}
foreach($name in @('rock_face_03_nor_gl_2k.jpg','rock_face_03_arm_1k.jpg','rock-face-03-source.json')){if(Test-Path -LiteralPath (Join-Path $pack $name)){throw 'New surface survived revert'}}
foreach($name in @('rock_face_03_diff_2k.jpg','rock_face_03_nor_gl_2k.jpg','rock_face_03_arm_1k.jpg','rock-face-03-source.json')){if(!(Test-AoSetupAssetPath ('randy-vk/packs/EnhancedRocks/'+$name))){throw 'Surface path refused'}}
foreach($relative in @('randy-vk/packs/EnhancedRocks/../x.jpg','randy-vk/packs/EnhancedRocks/extra.jpg','randy-vk/packs/EnhancedRocks/a.exe')){if(Test-AoSetupAssetPath $relative){throw 'Unsafe path allowed'}}
@{result='passed';install=$true;idempotency=$true;unownedAssetRefusal=$true;changedSurfaceRevertRefusal=$true;rendererOwnershipRefusal=$true;exactRevert=$true;settingsPreserved=$true;preparedCachePreserved=$true;newFilesRemovedOnRevert=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'INSTALLER-TESTS.json')
Write-Output 'Install, repeat install, file ownership checks and exact renderer/asset revert passed.'
