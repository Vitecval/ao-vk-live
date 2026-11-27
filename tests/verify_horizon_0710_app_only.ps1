param(
 [Parameter(Mandatory=$true)][string]$Candidate,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedSha256,
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedCoreSha256,
 [Parameter(Mandatory=$true)][long]$ExpectedCoreBytes,
 [Parameter(Mandatory=$true)][int]$ExpectedEngineRevision,
 [string]$StateRoot=('F:\AO-VK-D2\horizon-0.8.0\app-only-check-'+[guid]::NewGuid().ToString('N')),
 [string]$LegacyExe='C:\AOSharp\AO-VK\ao-vk-main\build\horizon-0.7.9\publish\Rubi-Ka-Horizon-Setup.exe',
 [ValidateSet('All','PrepareLegacy')][string]$Phase='All',
 [switch]$StagedCandidate
)
$ErrorActionPreference='Stop'
function Check([bool]$condition,[string]$message){if(!$condition){throw $message}}
function Sha([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Json($value,[string]$path){$value|ConvertTo-Json -Depth 12|Set-Content -LiteralPath $path -Encoding UTF8}
Check $StagedCandidate 'Run only after the release owner confirms the exact candidate is staged.'
Check ((Sha $Candidate) -ceq $ExpectedSha256.ToLowerInvariant()) 'Candidate differs from its approved verification hash.'
Check ((Get-Item -LiteralPath $Candidate).Length -lt 512L*1024*1024) 'Candidate exceeds the old updater limit.'
Check ([Diagnostics.FileVersionInfo]::GetVersionInfo($Candidate).FileVersion -ceq '0.8.0.0') 'Expected the actual 0.8.0 candidate.'
$coreSha256=$ExpectedCoreSha256.ToLowerInvariant()
$bundle=[IO.File]::OpenRead($Candidate)
try{
 $reader=New-Object IO.BinaryReader($bundle)
 $bundle.Position=$bundle.Length-56
 Check ([Text.Encoding]::ASCII.GetString($reader.ReadBytes(8)) -ceq 'RKHCORE1') 'The app has no valid bundled graphics footer.'
 $coreOffset=$reader.ReadInt64();$coreBytes=$reader.ReadInt64();$footerSha=([BitConverter]::ToString($reader.ReadBytes(32))).Replace('-','').ToLowerInvariant()
 Check ($footerSha -ceq $coreSha256 -and $coreBytes -eq $ExpectedCoreBytes -and $coreOffset -gt 0 -and $coreOffset+$coreBytes+56 -eq $bundle.Length) 'The updater differs from its approved graphics core pin.'
 $bundle.Position=$coreOffset;$remaining=$coreBytes;$buffer=New-Object byte[] 1048576;$digest=[Security.Cryptography.SHA256]::Create()
 try{
  while($remaining -gt 0){$count=$bundle.Read($buffer,0,[int][Math]::Min($buffer.Length,$remaining));Check ($count -gt 0) 'The bundled graphics core was truncated.';[void]$digest.TransformBlock($buffer,0,$count,$buffer,0);$remaining-=$count}
  [void]$digest.TransformFinalBlock([byte[]]@(),0,0)
  Check (([BitConverter]::ToString($digest.Hash)).Replace('-','').ToLowerInvariant() -ceq $coreSha256) 'Actual bundled core bytes differ from the approved release.'
 }finally{$digest.Dispose()}
}finally{$bundle.Dispose()}
if($Phase -eq 'All'){
 $candidateAssembly=[Reflection.Assembly]::LoadFrom($Candidate)
 Check ($candidateAssembly.GetType('Horizon.SetupService').GetField('Version').GetRawConstantValue() -ceq '0.8.0' -and $candidateAssembly.GetType('Horizon.UpdateService').GetField('VersionTag').GetRawConstantValue() -ceq 'v0.8.0') 'App and updater version fields disagree.'
 $engine=$candidateAssembly.GetType('Horizon.App').GetMethod('ResourceText').Invoke($null,@('engine.json'))|ConvertFrom-Json
 Check ($engine.revision -eq $ExpectedEngineRevision -and $engine.fullSetupSha256 -ceq $coreSha256 -and $engine.fullSetupBytes -eq $ExpectedCoreBytes) 'Actual embedded engine resource differs from the supplied approved core identity.'
 $runtimeScripts=[ordered]@{}
 foreach($item in @(@{file='install-live.ps1';resource='Horizon.InstallLive.ps1'},@{file='setup-safety.ps1';resource='Horizon.SetupSafety.ps1'})){
  $resource=$candidateAssembly.GetManifestResourceStream($item.resource);Check ($null -ne $resource) 'The packaged runtime installer resource is missing.'
  $hash=[Security.Cryptography.SHA256]::Create()
  try{$actual=([BitConverter]::ToString($hash.ComputeHash($resource))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose();$resource.Dispose()}
  $source=Join-Path (Split-Path -Parent $PSScriptRoot) ('tools\'+$item.file)
  Check ($actual -ceq (Sha $source)) 'The app would run a different installer implementation from the tested source.'
  $runtimeScripts[$item.file]=$actual
 }
}
$StateRoot=[IO.Path]::GetFullPath($StateRoot)
Check ($StateRoot.StartsWith('F:\AO-VK-D2\',[StringComparison]::OrdinalIgnoreCase)) 'Use the authorized physical F: test folder.'
for($directory=New-Object IO.DirectoryInfo($StateRoot);$directory;$directory=$directory.Parent){
 if($directory.Exists -and ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Test state must have no junction ancestry.'}
}
New-Item -ItemType Directory -Path $StateRoot -Force|Out-Null
if($Phase -eq 'PrepareLegacy'){
 $baselineBuild=Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetDirectoryName($LegacyExe))) 'candidate\BUILD.json'
 $baseline=Get-Content -LiteralPath $baselineBuild -Raw|ConvertFrom-Json
 Check ($baseline.version -ceq '0.7.9' -and (Sha $LegacyExe) -ceq $baseline.sha256) 'Legacy sender differs from the preserved published 0.7.9 installer.'
 $assembly=[Reflection.Assembly]::LoadFrom($LegacyExe)
 $assembly.GetType('Horizon.App').GetField('StateRoot').SetValue($null,[string]$StateRoot)
 $service=$assembly.GetType('Horizon.UpdateService');$flags=[Reflection.BindingFlags]'Public,NonPublic,Static'
 Check ($service.GetField('VersionTag').GetRawConstantValue() -ceq 'v0.7.9') 'The legacy sender is not 0.7.9.'
 $downloadRoot=Join-Path $StateRoot 'updates\v0.8.0';New-Item -ItemType Directory -Path $downloadRoot -Force|Out-Null
 $download=Join-Path $downloadRoot 'Rubi-Ka-Horizon-Setup.exe';Copy-Item -LiteralPath $Candidate -Destination $download
 $repo=[string]$service.GetField('Repository').GetRawConstantValue()
 $asset=[ordered]@{name='Rubi-Ka-Horizon-Setup.exe';state='uploaded';size=(Get-Item -LiteralPath $download).Length;digest=('sha256:'+$ExpectedSha256.ToLowerInvariant());browser_download_url=($repo+'/releases/download/v0.8.0/Rubi-Ka-Horizon-Setup.exe')}
 $metadata=ConvertTo-Json -InputObject @([ordered]@{tag_name='v0.8.0';draft=$false;prerelease=$false;assets=@($asset)}) -Depth 8 -Compress
 $release=$service.GetMethod('Select',$flags).Invoke($null,@([string]$metadata))
 $legacyClient=Join-Path $StateRoot 'legacy-079-client';New-Item -ItemType Directory -Path $legacyClient -Force|Out-Null
 foreach($name in @('AnarchyOnline.exe','randy31.dll','personal.txt')){[IO.File]::WriteAllText((Join-Path $legacyClient $name),'isolated fixture, never executed or loaded')}
 $ticket=[string]$service.GetMethod('PrepareHandoff').Invoke($null,@([string]$download,$release,[string]$legacyClient,'full',$true))
 $rawTicket=Get-Content -LiteralPath $ticket -Raw|ConvertFrom-Json
 Check (!$rawTicket.PSObject.Properties['updateScope']) 'Published 0.7.9 legacy ticket unexpectedly has a scope field.'
 Check ($rawTicket.format -eq 1 -and $rawTicket.edition -ceq 'full') 'Legacy positive control must retain format1 and Full selection.'
 Json ([ordered]@{result='passed';ticket=$ticket;ticketSha256=(Sha $ticket);client=$legacyClient;sender=$LegacyExe;senderSha256=(Sha $LegacyExe);senderVersion='0.7.9';candidateSha256=(Sha $Candidate);scopeFieldAbsent=$true;edition='full';networkContacted=$false;gameLaunched=$false;retailClientsChanged=$false}) (Join-Path $StateRoot 'legacy-handoff-input.json')
 exit 0
}
$windowsPowerShell=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
$prepareArgs=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-Candidate',('"'+$Candidate+'"'),'-ExpectedSha256',$ExpectedSha256,'-ExpectedCoreSha256',$ExpectedCoreSha256,'-ExpectedCoreBytes',$ExpectedCoreBytes,'-ExpectedEngineRevision',$ExpectedEngineRevision,'-StateRoot',('"'+$StateRoot+'"'),'-LegacyExe',('"'+$LegacyExe+'"'),'-Phase','PrepareLegacy','-StagedCandidate')
$prepared=Start-Process -FilePath $windowsPowerShell -ArgumentList $prepareArgs -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $StateRoot 'legacy-prepare.stdout.log') -RedirectStandardError (Join-Path $StateRoot 'legacy-prepare.stderr.log')
Check ($prepared.ExitCode -eq 0) 'Actual legacy 0.7.9 sender fixture failed; inspect its isolated logs.'
$originalCache=Join-Path $env:LOCALAPPDATA 'AO-VK\pipelines-v1'
$cacheBefore=[ordered]@{}
if(Test-Path -LiteralPath $originalCache){foreach($file in Get-ChildItem -LiteralPath $originalCache -File){$cacheBefore[$file.FullName]=Sha $file.FullName}}
$env:LOCALAPPDATA=Join-Path $StateRoot 'private-localappdata';$env:TEMP=Join-Path $StateRoot 'private-temp';$env:TMP=$env:TEMP
New-Item -ItemType Directory -Path $env:LOCALAPPDATA,$env:TEMP -Force|Out-Null
$process=Start-Process -FilePath $Candidate -ArgumentList @('--self-test-app-only-updates','--state-root',('"'+$StateRoot+'"')) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $StateRoot 'app-only.stdout.log') -RedirectStandardError (Join-Path $StateRoot 'app-only.stderr.log')
# Keep the process handle open: Windows PowerShell can otherwise lose ExitCode
# when the child exits before the first property read after WaitForExit.
[void]$process.Handle
if(!$process.WaitForExit(600000)){throw 'Conditional workflow fixture timed out; no process was forcibly interrupted.'}
$process.Refresh()
Check ($process.ExitCode -eq 0) ('App-only workflow fixture failed (exit '+$process.ExitCode+'); inspect the isolated report and failure log.')
$reportPath=Join-Path $StateRoot 'APP-ONLY-UPDATE-RESULTS.json';$report=Get-Content -LiteralPath $reportPath -Raw|ConvertFrom-Json
Check ($report.result -ceq 'passed') 'App-only workflow fixture did not pass.'
Check ($report.actualLegacy079Ticket -eq $true) 'The actual old 0.7.9 sender ticket was not exercised.'
Check ($report.policy -ceq 'app-and-existing-graphics-only' -and $report.installedEditionPreserved -eq $true -and $report.neverInstallMissing -eq $true) 'Existing-only update policy evidence is missing.'
Check ($report.absentGraphicsRuns -eq 0 -and $report.currentGraphicsRuns -eq 0 -and $report.removedGraphicsRuns -eq 0 -and $report.unknownGraphicsRuns -eq 0) 'Updater attempted to install missing/current/removed/unmanaged graphics.'
Check ($report.existingLiteRuns -eq 1 -and $report.existingFullRuns -eq 1 -and $report.explicitGraphicsRuns -eq 1) 'Installed-edition update or explicit graphics-action positive control failed.'
Check ($report.sharedCurrentRuns -eq 0 -and $report.sharedStaleRuns -eq 1) 'Shared-loader current/stale update policy failed.'
Check ($report.legacyAbsentRuns -eq 0 -and $report.legacyExistingRuns -eq 1) 'Actual old ticket does not follow the conditional installed-only policy.'
Check ($report.clientFilesUnchanged -eq $true -and $report.networkContacted -eq $false -and $report.retailClientsChanged -eq $false -and $report.gameLaunched -eq $false) 'Required no-side-effect workflow evidence is missing.'
Check ((Sha $Candidate) -ceq $ExpectedSha256.ToLowerInvariant()) 'Candidate changed during the test.'
foreach($path in $cacheBefore.Keys){Check ((Sha $path) -ceq $cacheBefore[$path]) 'Existing graphics cache changed during an app-only test.'}
Json ([ordered]@{result='passed';policy='app-and-existing-graphics-only';candidate=$Candidate;candidateSha256=(Sha $Candidate);bytes=(Get-Item -LiteralPath $Candidate).Length;version='0.8.0';engineRevision=$ExpectedEngineRevision;bundledCoreSha256=$coreSha256;bundledCoreBytes=$coreBytes;graphicsCorePinVerified=$true;runtimeScriptsSha256=$runtimeScripts;verifierSha256=(Sha $PSCommandPath);workflowReport=$reportPath;workflowReportSha256=(Sha $reportPath);legacySenderReport=(Join-Path $StateRoot 'legacy-handoff-input.json');legacySenderReportSha256=(Sha (Join-Path $StateRoot 'legacy-handoff-input.json'));originalCacheFilesUnchanged=$true;originalCacheSha256=$cacheBefore;networkContacted=$false;gameLaunched=$false;gpuUsed=$false;retailClientsChanged=$false;published=$false}) (Join-Path $StateRoot 'VERIFIED-EXISTING-ONLY-UPDATE.json')
Get-Content -LiteralPath (Join-Path $StateRoot 'VERIFIED-EXISTING-ONLY-UPDATE.json')
