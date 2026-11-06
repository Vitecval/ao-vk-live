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
Wait-AoSetupHelper $p $p.Output '' 'Preparing GPU' $callback
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
try{Wait-AoSetupHelper $p $p.Output $errorLog 'Preparing GPU' $callback}catch{
 $caught=$_.Exception.Message -like '*failed (7)*unsupported synthetic resource*'
}
Assert ($caught -and $p.disposed) 'Native helper failure was falsely reported as success'
Write-Output 'PASS: active preparation beyond ten minutes, actual task messages, idle activity and native failure.'
