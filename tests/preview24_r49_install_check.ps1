$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$work=Join-Path $root 'build\preview24\stability-r49';$client=Join-Path $work 'installer-fixture-client'
if(Test-Path -LiteralPath $client){throw 'Use a fresh fixture for this one-shot check.'}
. (Join-Path $root 'tools\setup-safety.ps1')
New-Item -ItemType Directory -Path $client | Out-Null
$renderer=Join-Path $client 'ao-vk.dll';$receiptPath=Join-Path $client 'ao-vk-live.install.json';$ini=Join-Path $client 'randy-vk.ini'
[IO.File]::WriteAllText($renderer,'previous r48 renderer')
$originalIni="[Renderer]`r`nRVK_SunDist=123`r`nRVK_EnhancedRocks=0 ; user test`r`nRVK_RockShape=0.1`r`n; prefs remain exact $([char]0x2603)`r`n"
[IO.File]::WriteAllText($ini,$originalIni,(New-Object Text.UTF8Encoding($true)))
$cache=Join-Path $client 'randy-vk\cache\materials\0123456789abcdef.rvkm';New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($cache)) | Out-Null
[IO.File]::WriteAllText($cache,'prepared cache remains exact');$cacheHash=Get-AoSetupHash $cache
$names=@('rock_face_03_diff_2k.jpg','rock_face_03_nor_gl_2k.jpg','rock_face_03_arm_1k.jpg','rock-face-03-source.json');$pack=Join-Path $client 'randy-vk\packs\EnhancedRocks'
New-Item -ItemType Directory -Path $pack | Out-Null;$assets=@{}
foreach($name in $names){$file=Join-Path $pack $name;[IO.File]::WriteAllText($file,'previous owned '+$name);$assets['randy-vk/packs/EnhancedRocks/'+$name]=Get-AoSetupHash $file}
$arm=Join-Path $pack 'rock_face_03_arm_1k.jpg';[IO.File]::AppendAllText($arm,'user modification');$modified=Get-AoSetupHash $arm
$personal=Join-Path $pack 'personal.txt';[IO.File]::WriteAllText($personal,'unowned personal file');$personalHash=Get-AoSetupHash $personal
$original=Get-AoSetupHash $renderer
@{product='AO-VK Live';clientDirectory=$client;addon=@{'ao-vk.dll'=$original};assets=$assets}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receiptPath
$oldReceipt=Get-AoSetupHash $receiptPath;$installer=Join-Path $root 'tools\install-preview24-r49.ps1'
& $installer -Client $client -Fixture
$record=Read-AoSetupJson (Join-Path $work 'INSTALL-FIXTURE.json')
if($record.result -ne 'passed' -or !$record.onlyEnhancedRocksPreferenceChanged -or @($record.removedAssets.PSObject.Properties).Count -ne 3){throw 'Install result failed'}
$encoding=[Text.Encoding]::GetEncoding(28591);$saved=$encoding.GetString([IO.File]::ReadAllBytes((Join-Path $record.backup 'randy-vk.ini')))
$expected=$saved.Replace('RVK_EnhancedRocks=0','RVK_EnhancedRocks=1')
if($encoding.GetString([IO.File]::ReadAllBytes($ini)) -cne $expected){throw 'Other settings bytes changed'}
foreach($name in $names){if($name -ne 'rock_face_03_arm_1k.jpg' -and (Test-Path -LiteralPath (Join-Path $pack $name))){throw 'Owned cluster-pack surface survived'}}
if((Get-AoSetupHash $arm) -cne $modified -or (Get-AoSetupHash $personal) -cne $personalHash -or (Get-AoSetupHash $cache) -cne $cacheHash){throw 'Personal/cache file changed'}
$installedReceipt=Get-AoSetupHash $receiptPath
# An intentional later preference change survives repeat install and revert.
[IO.File]::AppendAllText($ini,'; subsequent preference comment');$settingsHash=Get-AoSetupHash $ini
& $installer -Client $client -Fixture
if((Get-AoSetupHash $ini) -cne $settingsHash -or (Get-AoSetupHash $receiptPath) -cne $installedReceipt){throw 'Repeat install changed preferences or receipt'}
[IO.File]::AppendAllText($renderer,'user change');$changed=Get-AoSetupHash $renderer;$refused=$false
try{& $installer -Client $client -Fixture}catch{$refused=$true}
if(!$refused -or (Get-AoSetupHash $renderer) -cne $changed){throw 'Renderer ownership guard failed'}
Copy-Item -LiteralPath (Join-Path $work 'candidate\ao-vk.dll') -Destination $renderer
$conflict=Join-Path $pack 'rock_face_03_diff_2k.jpg';[IO.File]::WriteAllText($conflict,'new user file');$conflictHash=Get-AoSetupHash $conflict;$refused=$false
try{& $installer -Client $client -Fixture -Revert}catch{$refused=$true}
if(!$refused -or (Get-AoSetupHash $conflict) -cne $conflictHash){throw 'Revert overwrote a new user file'}
Remove-Item -LiteralPath $conflict
& $installer -Client $client -Fixture -Revert
if((Get-AoSetupHash $renderer) -cne $original -or (Get-AoSetupHash $receiptPath) -cne $oldReceipt -or (Get-AoSetupHash $ini) -cne $settingsHash -or (Get-AoSetupHash $cache) -cne $cacheHash){throw 'Revert failed'}
foreach($name in $names){if($name -ne 'rock_face_03_arm_1k.jpg' -and (Get-AoSetupHash (Join-Path $pack $name)) -cne $assets['randy-vk/packs/EnhancedRocks/'+$name]){throw 'Surface restore differed'}}
if((Get-AoSetupHash $arm) -cne $modified -or (Get-AoSetupHash $personal) -cne $personalHash){throw 'Revert changed personal files'}
@{result='passed';install=$true;idempotency=$true;rendererOwnershipGuard=$true;ownedSurfaceRemoval=$true;modifiedAndUnownedSurfacesPreserved=$true;newFileRevertRefusal=$true;exactRendererReceiptSurfaceRevert=$true;onlyRequestedPreferenceChanged=$true;laterPreferencesAndPreparedCachePreserved=$true}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'INSTALLER-TESTS.json')
Write-Output 'Install, owned surface removal, preferences, repeat install, ownership and exact revert passed.'
