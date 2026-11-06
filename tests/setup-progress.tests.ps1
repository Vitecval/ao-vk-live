$ErrorActionPreference='Stop'
$env:PSModulePath=(Join-Path $PSHOME 'Modules')+';'+$env:PSModulePath
. (Join-Path $PSScriptRoot '..\tools\setup-progress.ps1')
$folder=Join-Path $PSScriptRoot ('..\build\setup-progress-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder | Out-Null
function Assert($condition,$message){if(!$condition){throw $message}}
# Advance a real helper wait contract beyond ten minutes without a ten-minute
# test delay. The simulated process exposes CPU, output, exit and disposal state.
$script:clock=[pscustomobject]@{seconds=650.0}
$script:clock|Add-Member ScriptProperty Elapsed {return [TimeSpan]::FromSeconds($this.seconds)}
function New-AoSetupClock {return $script:clock}
$script:messages=New-Object Collections.ArrayList
function Make-Process([int]$exitCode,[bool]$active) {
 $p=[pscustomobject]@{Handle=[IntPtr]1;calls=0;ExitCode=$exitCode;active=$active;disposed=$false;TotalProcessorTime=[TimeSpan]::Zero;Output=(Join-Path $folder ('helper-'+[guid]::NewGuid().ToString('N')+'.log'))}
 [IO.File]::WriteAllText($p.Output,'')
 $p|Add-Member ScriptMethod WaitForExit {
  param([int]$milliseconds=0)
  if(!$milliseconds){return}
  $this.calls++
  if($this.active){[IO.File]::AppendAllText($this.Output,('Preparing local city '+$this.calls+"`n"))}
  return $this.calls -ge 4
 }
 $p|Add-Member ScriptMethod Refresh {
  $script:clock.seconds+=90
  if($this.active){$this.TotalProcessorTime=[TimeSpan]::FromSeconds($this.calls)}
 }
 $p|Add-Member ScriptMethod Kill {throw 'A helper must not be killed merely for elapsed time'}
 $p|Add-Member ScriptMethod Dispose {$this.disposed=$true}
 return $p
}
$callback={param([string]$message,[int]$percent,[object]$detail) $null=$script:messages.Add(@{message=$message;detail=$detail})}
$p=Make-Process 0 $true
Wait-AoSetupHelper $p $p.Output '' 'Preparing Living' $callback
Assert ($p.calls -eq 4 -and $p.disposed) 'Active helper did not finish and release its handle'
Assert ($script:messages.Count -eq 4) 'Helper activity was not reported repeatedly'
Assert ($script:messages[0].detail.elapsedSeconds -gt 600) 'Test did not cross the former ten-minute cutoff'
Assert ($script:messages[-1].message -eq 'Preparing local city 4') 'Actual helper stage was not forwarded'
$script:messages.Clear()
$p=Make-Process 0 $false
Wait-AoSetupHelper $p $p.Output '' 'Waiting for storage' $callback
Assert ($script:messages[-1].detail.idleSeconds -gt 120) 'An idle wait was falsely reported as recent activity'
$p=Make-Process 7 $true
$errorLog=Join-Path $folder 'error.log'
[IO.File]::WriteAllText($errorLog,'unsupported synthetic resource')
$caught=$false
try{Wait-AoSetupHelper $p $p.Output $errorLog 'Preparing Living' $callback}catch{
 $caught=$_.Exception.Message -like '*failed (7)*unsupported synthetic resource*'
}
Assert ($caught -and $p.disposed) 'Native helper failure was falsely reported as success'
$client=Join-Path $folder 'dummy client'
$root=Join-Path $client 'randy-vk\living-local'
New-Item -ItemType Directory -Path $root -Force | Out-Null
$lock=Join-Path $root 'preparing.lock'
[IO.File]::WriteAllText((Join-Path $root 'active.txt'),'previous-generation')
[IO.File]::WriteAllText((Join-Path $root 'unfinished.bin'),'preserve interrupted data')
[IO.File]::WriteAllText($lock,'2147483647')
Preserve-AoInterruptedLivingLock $client
Assert (!(Test-Path -LiteralPath $lock)) 'A dead process lock still blocks preparation'
Assert (@(Get-ChildItem -LiteralPath $root -Filter 'interrupted-*.lock').Count -eq 1) 'Interrupted lock evidence was deleted'
Assert ([IO.File]::ReadAllText((Join-Path $root 'unfinished.bin')) -eq 'preserve interrupted data') 'Interrupted data was changed'
Assert ([IO.File]::ReadAllText((Join-Path $root 'active.txt')) -eq 'previous-generation') 'Previous active cache was changed'
[IO.File]::WriteAllText($lock,[string]$PID)
$caught=$false
try{Preserve-AoInterruptedLivingLock $client}catch{$caught=$_.Exception.Message -like '*still running*'}
Assert ($caught -and (Test-Path -LiteralPath $lock)) 'An active preparation lock was removed'
[IO.File]::WriteAllText($lock,'unrecognized owner')
$caught=$false
try{Preserve-AoInterruptedLivingLock $client}catch{$caught=$_.Exception.Message -like '*unrecognized*'}
Assert ($caught -and [IO.File]::ReadAllText($lock) -eq 'unrecognized owner') 'An unknown lock was changed'
Write-Output 'PASS: active preparation beyond ten minutes, real task messages, idle activity, native failure, stale-lock preservation, active/unknown-lock refusal.'
