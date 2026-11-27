param([Parameter(Mandatory=$true)][string]$FixtureRoot,[Parameter(Mandatory=$true)][string]$ResultsPath)
# Isolated synthetic bytes only. Never execute game/native binaries or helpers.
$ErrorActionPreference='Stop'
$hostModules=Join-Path $PSHOME 'Modules'
$env:PSModulePath=(@($hostModules)+@($env:PSModulePath -split ';'|Where-Object {$_ -and $_ -ine $hostModules})) -join ';'
$repo=Split-Path -Parent $PSScriptRoot
$installer=Join-Path $repo 'tools/install-live.ps1'
$result=[ordered]@{result='failed';host=$PSVersionTable.PSEdition;pointerBits=([IntPtr]::Size*8);checks=@();evidence=@();networkContacted=$false;gpuUsed=$false;retailClientsChanged=$false;gameLaunched=$false}
function Assert($condition,[string]$message){if(!$condition){throw ('ASSERT: '+$message)}}
function Check([string]$name){$result.checks+=@($name)}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Write-Fixture([string]$path,[string]$text){New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force|Out-Null;[IO.File]::WriteAllText($path,$text)}
function Snapshot([string]$directory){$rows=@{};foreach($file in Get-ChildItem -LiteralPath $directory -Recurse -Force -File){$rows[$file.FullName.Substring($directory.Length)]=(Hash $file.FullName)+':'+$file.Length+':'+$file.LastWriteTimeUtc.Ticks};return $rows}
function Same($a,$b){if($a.Count -ne $b.Count){return $false};foreach($key in $a.Keys){if(!$b.ContainsKey($key)-or $a[$key] -cne $b[$key]){return $false}};return $true}
function Reject($arguments,[string]$expected='changed or were removed'){
 $caught=$false;try{& $installer @arguments|Out-Null}catch{$caught=$true;Assert ($_.Exception.Message -like ('*'+$expected+'*')) ('Unexpected refusal: '+$_.Exception.Message)}
 Assert $caught ('Expected refusal: '+$expected)
}
function Fixture([string]$name,[string]$edition){
 $base=Join-Path $FixtureRoot $name;$client=Join-Path $base 'AO private folder';$package=Join-Path $base 'package';New-Item -ItemType Directory -Path $client,$package -Force|Out-Null
 $originals=@{};foreach($file in @('randy31.dll','DisplaySystem.dll','Gamecode.dll','GUI.dll','N3.dll','FXS.dll')){Write-Fixture (Join-Path $client $file) ('dummy stock '+$file);$originals[$file]=Hash (Join-Path $client $file)}
 Write-Fixture (Join-Path $client 'AnarchyOnline.exe') 'dummy, never executable';Write-Fixture (Join-Path $client 'Anarchy.exe') 'dummy, never executable'
 Write-Fixture (Join-Path $client 'randy-vk.ini') "[User]`r`nPreserve=exact";Write-Fixture (Join-Path $client 'personal.txt') 'keep personal files'
 Write-Fixture (Join-Path $package 'ao-vk.dll') 'dummy addon v1';Write-Fixture (Join-Path $package 'ddraw.dll') 'dummy loader v1'
 $assets=@{};if($edition -ceq 'full'){$relative='randy-vk/packs/ao-penumbra-078/1010006_999_d.png';Write-Fixture (Join-Path $package $relative) 'dummy texture v1';$assets[$relative]=Hash (Join-Path $package $relative)}
 $manifest=@{product='AO-VK Live';version='0.6.0-preview.24';build='guard-fixture-v1';edition=$edition;client=$originals;addon=@{'ao-vk.dll'=(Hash (Join-Path $package 'ao-vk.dll'));'ddraw.dll'=(Hash (Join-Path $package 'ddraw.dll'))};assets=$assets;previousAddons=@()}
 $manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $package 'manifest.json')
 return @{client=$client;package=$package;manifest=$manifest;originals=$originals}
}
function Install-Baseline($fixture){& $installer -ClientDir $fixture.client -PackageDir $fixture.package -DisableLargeAddressAware|Out-Null}
function Next-Package($fixture){
 Write-Fixture (Join-Path $fixture.package 'ao-vk.dll') 'dummy addon v2';Write-Fixture (Join-Path $fixture.package 'ddraw.dll') 'dummy loader v2'
 $fixture.manifest.addon.'ao-vk.dll'=Hash (Join-Path $fixture.package 'ao-vk.dll');$fixture.manifest.addon.'ddraw.dll'=Hash (Join-Path $fixture.package 'ddraw.dll');$fixture.manifest.build='guard-fixture-v2'
 foreach($relative in @($fixture.manifest.assets.Keys)){Write-Fixture (Join-Path $fixture.package $relative) 'dummy texture v2';$fixture.manifest.assets[$relative]=Hash (Join-Path $fixture.package $relative)}
 $fixture.manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $fixture.package 'manifest.json')
}
function Arguments($fixture){return @{ClientDir=$fixture.client;PackageDir=$fixture.package;Upgrade=$true;RequireExisting=$true;DisableLargeAddressAware=$true}}
function global:Get-Process{param($Name,$ErrorAction)}
$savedAppData=$env:APPDATA;$savedTemp=$env:TEMP;$savedTmp=$env:TMP
try{
 $FixtureRoot=[IO.Path]::GetFullPath($FixtureRoot);Assert ($FixtureRoot.StartsWith('F:\AO-VK-D2\',[StringComparison]::OrdinalIgnoreCase)) 'Only the authorized F: fixture root is allowed'
 New-Item -ItemType Directory -Path $FixtureRoot -Force|Out-Null
 $env:APPDATA=Join-Path $FixtureRoot 'AppData';$env:TEMP=Join-Path $FixtureRoot 'Temp';$env:TMP=$env:TEMP;New-Item -ItemType Directory -Path $env:APPDATA,$env:TEMP -Force|Out-Null
 $missing=Fixture 'absent' 'full';$before=Snapshot $missing.client
 # A broken, nonexistent helper is a safety tripwire, never executable. The
 # existing-only guard must refuse before any pipeline preparation is reached.
 $missing.manifest.cacheWarmup=@{helper='ao-vk-cache-warm.exe';relativeCache='auto';sha256=('0'*64)};$missing.manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $missing.package 'manifest.json')
 Reject (Arguments $missing);Assert (Same $before (Snapshot $missing.client)) 'Absent graphics refusal wrote client files'
 Check 'missing_graphics_refused_before_preparation_or_installation'
 foreach($edition in @('lite','full')){
  $f=Fixture ('owned-'+$edition) $edition;Install-Baseline $f;$ini=Hash (Join-Path $f.client 'randy-vk.ini');$personal=Hash (Join-Path $f.client 'personal.txt');Next-Package $f
  $updateArguments=Arguments $f;& $installer @updateArguments|Out-Null
  $receipt=Get-Content -LiteralPath (Join-Path $f.client 'ao-vk-live.install.json') -Raw|ConvertFrom-Json
  Assert ($receipt.edition -ceq $edition -and $receipt.build -ceq 'guard-fixture-v2') 'Existing update lost installed edition or new receipt'
  foreach($name in @('ao-vk.dll','ddraw.dll')){Assert ((Hash (Join-Path $f.client $name)) -ceq $f.manifest.addon.$name) 'Existing update did not install verified addon bytes'}
  Assert ((Hash (Join-Path $f.client 'randy-vk.ini')) -ceq $ini -and (Hash (Join-Path $f.client 'personal.txt')) -ceq $personal) 'Existing update changed preferences or personal files'
  foreach($name in $f.originals.Keys){Assert ((Hash (Join-Path $f.client $name)) -ceq $f.originals[$name]) 'Existing update changed a stock game file'}
  $result.evidence+=@(@{case='owned-update';edition=$edition;client=$f.client;receiptSha256=(Hash (Join-Path $f.client 'ao-vk-live.install.json'))})
  Check ('owned_'+$edition+'_updated_without_edition_change')
 }
 $mismatch=Fixture 'edition-mismatch' 'lite';Install-Baseline $mismatch;Next-Package $mismatch;$mismatch.manifest.edition='full';$mismatch.manifest|ConvertTo-Json -Depth 12|Set-Content -LiteralPath (Join-Path $mismatch.package 'manifest.json');$before=Snapshot $mismatch.client
 Reject (Arguments $mismatch);Assert (Same $before (Snapshot $mismatch.client)) 'Edition mismatch changed files';Check 'existing_update_cannot_switch_installed_edition'
 $unknown=Fixture 'unknown-wrapper' 'full';Install-Baseline $unknown;Next-Package $unknown;Write-Fixture (Join-Path $unknown.client 'ddraw.dll') 'unknown wrapper, preserve';$before=Snapshot $unknown.client
 Reject (Arguments $unknown);Assert (Same $before (Snapshot $unknown.client)) 'Unknown wrapper refusal changed files';Check 'unknown_wrapper_refused_without_mutation'
 $noUpgrade=Fixture 'require-upgrade' 'lite';Install-Baseline $noUpgrade;$arguments=Arguments $noUpgrade;$arguments.Remove('Upgrade');$before=Snapshot $noUpgrade.client
 Reject $arguments;Assert (Same $before (Snapshot $noUpgrade.client)) 'RequireExisting without Upgrade changed files';Check 'require_existing_cannot_be_used_as_fresh_install'
 $race=Fixture 'removed-after-preparation' 'full';Install-Baseline $race;Next-Package $race
 $global:GuardRemovalTarget=$race.client;$global:GuardRemoved=$false;$global:GuardAfterRemoval=$null;$global:GuardCommitted=$false
 $callback={param($message,$percent)
  if($message -like 'Installing verified files:*'){$global:GuardCommitted=$true}
  if(!$global:GuardRemoved -and $message -like 'Preparing installation:*'){
   foreach($name in @('ao-vk.dll','ddraw.dll','ao-vk-live.install.json')){Remove-Item -LiteralPath (Join-Path $global:GuardRemovalTarget $name)}
   $global:GuardAfterRemoval=Snapshot $global:GuardRemovalTarget;$global:GuardRemoved=$true
  }
 }
 $arguments=Arguments $race;$arguments.ProgressCallback=$callback;Reject $arguments
 Assert ($global:GuardRemoved -and !$global:GuardCommitted) 'Removal injection did not reach staging or transaction began despite removal'
 Assert (Same $global:GuardAfterRemoval (Snapshot $race.client)) 'Update reinstalled removed graphics or left staged files after refusal'
 Assert (!(Test-Path -LiteralPath (Join-Path $race.client '.ao-vk-setup-transaction.json'))) 'A transaction journal was created for removed graphics'
 Check 'removal_after_preparation_rechecked_before_transaction_no_reinstall'
 $explicit=Fixture 'explicit-upgrade-without-existing-policy' 'lite';$arguments=Arguments $explicit;$arguments.Remove('RequireExisting');& $installer @arguments|Out-Null
 Assert (Test-Path -LiteralPath (Join-Path $explicit.client 'ao-vk.dll')) 'Explicit Upgrade positive control no longer supports installing graphics'
 Check 'explicit_installation_upgrade_remains_available'
 $result.result='passed'
}catch{$result.error=$_.Exception.ToString();throw}finally{
 $env:APPDATA=$savedAppData;$env:TEMP=$savedTemp;$env:TMP=$savedTmp
 $result|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $ResultsPath -Encoding UTF8
 Remove-Item Function:\Get-Process -ErrorAction SilentlyContinue
}
