$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\tools\setup-safety.ps1')
$probeRoot=Join-Path $PSScriptRoot ('..\build\journal-retry-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $probeRoot | Out-Null
$journalPath=Join-Path $probeRoot 'journal.json'
$journal=[pscustomobject]@{token=[guid]::NewGuid().ToString('N');version=1;committed=$false;entries=@()}
Save-AoTransaction $journalPath $journal
Add-Type -TypeDefinition @'
using System.IO;
using System.Threading;
public static class JournalRetryFixture {
 public static void ReleaseAfter(FileStream handle, int delay) {
  new Thread(()=>{Thread.Sleep(delay);handle.Dispose();}).Start();
 }
}
'@
$handle=[IO.File]::Open($journalPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
[JournalRetryFixture]::ReleaseAfter($handle,120)
$journal.committed=$true
Save-AoTransaction $journalPath $journal
if(!(Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json).committed){throw 'Transient lock did not recover'}
$before=[IO.File]::ReadAllText($journalPath)
$handle=[IO.File]::Open($journalPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
 $journal.committed=$false
 $refused=$false
 try {Save-AoTransaction $journalPath $journal} catch {$refused=$true}
 if(!$refused){throw 'Persistent lock was not refused'}
} finally {$handle.Dispose()}
if([IO.File]::ReadAllText($journalPath) -cne $before){throw 'Persistent lock changed the existing journal'}
if(@(Get-ChildItem -LiteralPath $probeRoot -Filter '*.writing-*').Count){throw 'Temporary journal was not cleaned up'}
for($iteration=0;$iteration -lt 100;$iteration++){Save-AoTransaction $journalPath $journal}
[pscustomobject]@{result='passed';transientLockRecovered=$true;persistentLockRefused=$true;existingJournalPreserved=$true;atomicReplacements=100}
