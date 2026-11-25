param(
 [string]$Candidate='C:\AOSharp\AO-VK\ao-vk-main\build\horizon-0.7.8\candidate\Rubi-Ka-Horizon-Setup.exe',
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedSha256,
 [string]$StateRoot='F:\AO-VK-D2\horizon-078-public-verification',
 [string]$StockSource='F:\AO_SInstall\Anarchy Online',
 [string]$OldExe='C:\AOSharp\AO-VK\ao-vk-main\build\horizon-0.7.7\publish\Rubi-Ka-Horizon-Setup.exe',
 [int]$ExpectedRevision=72,
 [ValidateSet('All','App','Setup','LegacyPrepare','LegacyAccept')][string]$Phase='All',
 [switch]$StagedCandidate,[switch]$AllowPipelinePreparation
)
$ErrorActionPreference='Stop'
function Sha([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Write-Json($Value,[string]$Path){$Value | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding UTF8}
function Check([bool]$Condition,[string]$Message){if(!$Condition){throw $Message}}
if(!$StagedCandidate){throw 'Run only after the release owner confirms the candidate passed staging.'}
Check ((Sha $Candidate) -ceq $ExpectedSha256.ToLowerInvariant()) 'Candidate differs from the staged SHA-256.'
Check ((Get-Item -LiteralPath $Candidate).Length -le 512L*1024*1024) 'Candidate exceeds the old0.7.7 updater limit.'
Check ([IO.Path]::GetFileName($Candidate) -ceq 'Rubi-Ka-Horizon-Setup.exe') 'Stable updater asset filename changed.'
$StateRoot=[IO.Path]::GetFullPath($StateRoot)
Check ($StateRoot.StartsWith('F:\AO-VK-D2\',[StringComparison]::OrdinalIgnoreCase)) 'Use the authorized physical F: verification folder.'
for($directory=New-Object IO.DirectoryInfo($StateRoot);$directory;$directory=$directory.Parent){
 if($directory.Exists -and ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Verification must use physical paths, without junction ancestry.'}
}
New-Item -ItemType Directory -Path $StateRoot -Force | Out-Null
$windowsPowerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$flags=[Reflection.BindingFlags]'Public,NonPublic,Static'

if($Phase -eq 'LegacyPrepare'){
 $legacyRoot=Join-Path $StateRoot 'legacy-updater';New-Item -ItemType Directory -Path $legacyRoot -Force | Out-Null
 $oldBuild=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($OldExe))) 'candidate\BUILD.json'
 $oldPin=Get-Content -LiteralPath $oldBuild -Raw | ConvertFrom-Json
 Check ($oldPin.version -ceq '0.7.7' -and (Sha $OldExe) -ceq $oldPin.sha256) 'Old updater is not the verified published0.7.7 executable.'
 $assembly=[Reflection.Assembly]::LoadFrom($OldExe)
 $app=$assembly.GetType('Horizon.App');$app.GetField('StateRoot').SetValue($null,$legacyRoot)
 $update=$assembly.GetType('Horizon.UpdateService')
 $repo=[string]$update.GetField('Repository').GetRawConstantValue()
 $asset=[ordered]@{name='Rubi-Ka-Horizon-Setup.exe';state='uploaded';size=(Get-Item -LiteralPath $Candidate).Length;digest=('sha256:'+$ExpectedSha256.ToLowerInvariant());browser_download_url=($repo+'/releases/download/v0.7.8/Rubi-Ka-Horizon-Setup.exe')}
 $metadata=ConvertTo-Json -InputObject @([ordered]@{tag_name='v0.7.8';draft=$false;prerelease=$false;name='Rubi-Ka Horizon0.7.8';assets=@($asset)}) -Depth 8 -Compress
 $release=$update.GetMethod('Select',$flags).Invoke($null,@($metadata))
 Check ($release.Tag -ceq 'v0.7.8') 'Actual old updater did not select the real candidate metadata.'
 Check ([int]$update.GetMethod('Compare',$flags).Invoke($null,@('v0.7.8','v0.7.7')) -gt 0) 'Old updater does not consider0.7.8 newer.'
 $downloads=Join-Path $legacyRoot 'updates\v0.7.8';New-Item -ItemType Directory -Path $downloads -Force | Out-Null
 $download=Join-Path $downloads 'Rubi-Ka-Horizon-Setup.exe';Copy-Item -LiteralPath $Candidate -Destination $download
 $update.GetMethod('ValidateBundle',$flags).Invoke($null,@($download))
 $client=Join-Path $legacyRoot 'AO dummy client';New-Item -ItemType Directory -Path $client -Force | Out-Null
 [IO.File]::WriteAllText((Join-Path $client 'AnarchyOnline.exe'),'dummy never executed')
 [IO.File]::WriteAllText((Join-Path $client 'randy31.dll'),'dummy never loaded')
 $ticket=[string]$update.GetMethod('PrepareHandoff').Invoke($null,@($download,$release,$client,'lite',$true))
 $report=[ordered]@{result='passed';oldVersion='0.7.7';oldExecutable=$OldExe;oldSha256=(Sha $OldExe);candidate=$Candidate;candidateSha256=(Sha $Candidate);candidateBytes=(Get-Item -LiteralPath $Candidate).Length;selectedTag=$release.Tag;bundleAccepted=$true;ticket=$ticket;download=$download;client=$client;metadata=$metadata;networkContacted=$false;gameLaunched=$false;retailClientsChanged=$false}
 Write-Json $report (Join-Path $StateRoot 'LEGACY-PREPARE.json');exit 0
}
if($Phase -eq 'LegacyAccept'){
 $prepared=Get-Content -LiteralPath (Join-Path $StateRoot 'LEGACY-PREPARE.json') -Raw | ConvertFrom-Json
 Check ($prepared.candidateSha256 -ceq $ExpectedSha256.ToLowerInvariant()) 'Legacy prepare belongs to another candidate.'
 $assembly=[Reflection.Assembly]::LoadFrom($prepared.download)
 $assembly.GetType('Horizon.App').GetField('StateRoot').SetValue($null,(Join-Path $StateRoot 'legacy-updater'))
 $update=$assembly.GetType('Horizon.UpdateService')
 Check ($update.GetField('VersionTag').GetRawConstantValue() -ceq 'v0.7.8') 'New handoff receiver has the wrong version.'
 $ticket=$update.GetMethod('ReadHandoff',$flags).Invoke($null,@([string]$prepared.ticket))
 Check ($ticket['format'] -eq 1 -and $ticket['tag'] -ceq 'v0.7.8' -and $ticket['client'] -ceq $prepared.client -and $ticket['edition'] -ceq 'lite' -and $ticket['skipMemoryChange'] -eq $true) 'Actual old-to-new handoff lost selected preferences.'
 $update.GetMethod('WaitForParent',$flags).Invoke($null,@($ticket))
 Write-Json ([ordered]@{result='passed';candidateSha256=$ExpectedSha256.ToLowerInvariant();oldPrepareSha256=(Sha (Join-Path $StateRoot 'LEGACY-PREPARE.json'));actualOldSelect=$true;actualOldBundleValidation=$true;actualOldPrepare=$true;actualNewReadHandoff=$true;actualNewParentWait=$true;format=1;selectedClientPreserved=$true;editionPreserved=$true;memoryPreferencePreserved=$true;networkContacted=$false;appInstalled=$false;gameLaunched=$false;retailClientsChanged=$false}) (Join-Path $StateRoot 'LEGACY-UPDATER-RESULTS.json')
 exit 0
}

$assembly=[Reflection.Assembly]::LoadFrom($Candidate)
Check ($assembly.GetType('Horizon.SetupService').GetField('Version').GetRawConstantValue() -ceq '0.7.8') 'Packaged SetupService version is not0.7.8.'
Check ($assembly.GetType('Horizon.UpdateService').GetField('VersionTag').GetRawConstantValue() -ceq 'v0.7.8') 'Packaged updater version is notv0.7.8.'
Check ([Diagnostics.FileVersionInfo]::GetVersionInfo($Candidate).FileVersion -ceq '0.7.8.0') 'Packaged assembly file version is not0.7.8.0.'
$app=$assembly.GetType('Horizon.App')
$engine=$app.GetMethod('ResourceText').Invoke($null,@('engine.json')) | ConvertFrom-Json
Check ($engine.revision -eq $ExpectedRevision) 'Packaged engine revision differs from the staged release.'
$originalLocalAppData=$env:LOCALAPPDATA
$baselineCache=Join-Path $originalLocalAppData 'AO-VK\pipelines-v1'
$baselineHashes=[ordered]@{}
if(Test-Path -LiteralPath $baselineCache){foreach($file in Get-ChildItem -LiteralPath $baselineCache -File){$baselineHashes[$file.FullName]=Sha $file.FullName}}
# The unchanged setup helper intentionally clears RANDYVK_PIPELINE_CACHE when
# asked for "auto". Redirect only this verifier and its children via LOCALAPPDATA.
$privateLocal=Join-Path $StateRoot 'private-localappdata';New-Item -ItemType Directory -Path $privateLocal -Force | Out-Null
$privateTemp=Join-Path $StateRoot 'private-temp';New-Item -ItemType Directory -Path $privateTemp -Force | Out-Null
$env:LOCALAPPDATA=$privateLocal;$env:TEMP=$privateTemp;$env:TMP=$privateTemp
$env:VK_LOADER_LAYERS_DISABLE='~implicit~'
$checks=@()
if($Phase -in @('All','App')){
 $checks+=@(
  @{name='settings';argument='--self-test';report='TEST-RESULTS.json'},
  @{name='updates';argument='--self-test-updates';report='UPDATE-TEST-RESULTS.json'},
  @{name='features';argument='--self-test-feature-ui';report='FEATURE-UI-RESULTS.json'},
  @{name='tray';argument='--self-test-tray';report='TRAY-TEST-RESULTS.json'},
  @{name='display';argument='--self-test-display-ui';report='DISPLAY-UI-RESULTS.json'},
  @{name='tooltips';argument='--self-test-tooltips-ui';report='TOOLTIP-UI-RESULTS.json'}
 )
}
if($Phase -in @('All','Setup')){
 Check $AllowPipelinePreparation 'Actual Lite setup prepares mandatory pipelines; require the release owner to grant its GPU slot.'
 $lite=$app.GetMethod('ResourceText').Invoke($null,@('lite-manifest.json')) | ConvertFrom-Json
 $stock=Join-Path $StateRoot 'verified-stock-inputs';New-Item -ItemType Directory -Path $stock -Force | Out-Null
 $stockProof=@()
 foreach($item in $lite.client.PSObject.Properties){
  $original=Join-Path $StockSource $item.Name;Check ((Sha $original) -ceq $item.Value) ('Stock source differs: '+$item.Name)
  $copy=Join-Path $stock $item.Name;Copy-Item -LiteralPath $original -Destination $copy
  Check ((Sha $copy) -ceq $item.Value) 'Stock copy differs from its manifest pin.'
  $stockProof+=[ordered]@{name=$item.Name;source=$original;sourceSha256=(Sha $original);copied=$copy;sha256=(Sha $copy)}
 }
 Write-Json $stockProof (Join-Path $StateRoot 'STOCK-INPUTS.json')
 $checks+=@{name='setup';argument='--self-test-setup';report='SETUP-TEST-RESULTS.json';extra=@('--stock-fixture-dir',('"'+$stock+'"'))}
}
$results=@();$processes=@()
foreach($check in $checks){
 $state=Join-Path $StateRoot ($check.name+'-'+[guid]::NewGuid().ToString('N'))
 $arguments=@($check.argument,'--state-root',('"'+$state+'"'))
 if($check.extra){$arguments+=@($check.extra)}
 $process=Start-Process -FilePath $Candidate -ArgumentList $arguments -WindowStyle Hidden -PassThru
 $processes+=[pscustomobject]@{name=$check.name;argument=$check.argument;process=$process;state=$state;report=$check.report;started=[DateTime]::UtcNow;finished=$false}
}
while(@($processes | Where-Object {!$_.finished}).Count){
 foreach($check in $processes | Where-Object {!$_.finished}){
  $check.process.Refresh()
  if(!$check.process.HasExited){if(([DateTime]::UtcNow-$check.started).TotalMinutes -gt 15){throw ('Isolated test timed out: '+$check.name)};continue}
  $check.finished=$true
  Check ($check.process.ExitCode -eq 0) ('Packaged '+$check.name+' failed; inspect '+$check.state)
  Check ((Sha $Candidate) -ceq $ExpectedSha256.ToLowerInvariant()) 'Candidate changed while verification was running.'
  $path=Join-Path $check.state $check.report;$report=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
  Check ($report.result -ceq 'passed') ('Packaged report did not pass: '+$path)
  if($check.name -eq 'updates'){Check ($report.networkContacted -eq $false -and $report.emailSent -eq $false) 'Update/report tests contacted a real service.'}
  Check ($report.retailClientsChanged -eq $false) 'A test did not confirm retail isolation.'
  $results+=[ordered]@{name=$check.name;argument=$check.argument;result='passed';report=$path;reportSha256=(Sha $path);checkCount=$(if($report.PSObject.Properties['checks']){@($report.checks).Count}else{$null});exitCode=$check.process.ExitCode}
  if($check.name -eq 'settings'){
   $preset=Join-Path $check.state 'PRESET-TEST-RESULTS.json';Check ((Get-Content -LiteralPath $preset -Raw | ConvertFrom-Json).result -ceq 'passed') 'Preset checks did not pass.'
   $results+=[ordered]@{name='presets';result='passed';report=$preset;reportSha256=(Sha $preset)}
  }
  Write-Output ($check.name+' passed')
 }
 Start-Sleep -Milliseconds 500
}
if($Phase -in @('All','App')){
 foreach($mode in @('LegacyPrepare','LegacyAccept')){
  $args=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Candidate',('"'+$Candidate+'"'),'-ExpectedSha256',$ExpectedSha256,'-StateRoot',('"'+$StateRoot+'"'),'-OldExe',('"'+$OldExe+'"'),'-Phase',$mode,'-StagedCandidate')
  $process=Start-Process -FilePath $windowsPowerShell -ArgumentList $args -WindowStyle Hidden -Wait -PassThru
  Check ($process.ExitCode -eq 0) ('Actual old/new updater compatibility failed: '+$mode)
 }
 $legacy=Join-Path $StateRoot 'LEGACY-UPDATER-RESULTS.json'
 $results+=[ordered]@{name='legacy-updater';result='passed';report=$legacy;reportSha256=(Sha $legacy)}
}
if($Phase -in @('All','Setup')){
 foreach($item in $stockProof){Check ((Sha $item.source) -ceq $item.sourceSha256) 'A stock source changed during isolated setup.'}
}
foreach($path in $baselineHashes.Keys){Check ((Sha $path) -ceq $baselineHashes[$path]) 'The existing product pipeline cache changed.'}
Write-Json ([ordered]@{result='passed';originalCacheDirectory=$baselineCache;originalFileSha256=$baselineHashes;privateLocalAppData=$privateLocal;originalFilesUnchanged=$true;privateCacheFiles=@(Get-ChildItem -LiteralPath $privateLocal -Recurse -File | Where-Object {$_.Extension -eq '.cache'} | ForEach-Object {[ordered]@{file=$_.FullName;sha256=(Sha $_.FullName);bytes=$_.Length}})}) (Join-Path $StateRoot 'CACHE-ISOLATION.json')
$final=[ordered]@{result='passed';scope=$Phase;source=$Candidate;sourceSha256=(Sha $Candidate);bytes=(Get-Item -LiteralPath $Candidate).Length;verifierSha256=(Sha $PSCommandPath);version='0.7.8';engineRevision=$ExpectedRevision;checks=$results;actualPackagedExe=$true;oldUpdaterLimitBytes=512L*1024*1024;setupPipelinePreparation=($Phase -in @('All','Setup'));networkContacted=$false;emailSent=$false;gameLaunched=$false;retailClientsChanged=$false;publicProfileChanged=$false;publicationPerformed=$false}
Write-Json $final (Join-Path $StateRoot ('PACKAGED-'+$Phase.ToUpperInvariant()+'-RESULTS.json'))
$final | ConvertTo-Json -Depth 8
