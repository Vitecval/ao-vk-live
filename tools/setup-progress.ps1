# Native preparation remains outside the UI thread. Observe activity, never kill
# a working helper merely because a fixed wall-clock deadline has elapsed.
function New-AoSetupClock { return [Diagnostics.Stopwatch]::StartNew() }
function Wait-AoSetupHelper($Process, [string]$OutputLog, [string]$ErrorLog, [string]$Label, [scriptblock]$ProgressCallback) {
    $null = $Process.Handle
    $timer = New-AoSetupClock
    $activityAt = 0.0
    $cpu = -1L
    $length = -1L
    $previousErrorLength = -1L
    $message = $Label
    $reader = $null
    try {
        while ($true) {
            $finished = $Process.WaitForExit(200)
            if (!$reader -and (Test-Path -LiteralPath $OutputLog)) {
                $stream = [IO.File]::Open($OutputLog, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                $reader = New-Object IO.StreamReader($stream)
            }
            if ($reader) {
                while (!$reader.EndOfStream) {
                    $line = $reader.ReadLine()
                    if ($line -match '^(Preparing|Validating|Checking|Reusing|Local cache|City [0-9]+)') {
                        $message = $line.Substring(0, [Math]::Min(220, $line.Length))
                    } elseif ($line -match 'startup-event:.*stage=(.+)$') {
                        $names = @{ instance='Starting Vulkan'; gpu='Selecting the graphics card'; device='Opening the graphics device'; cache='Reading the local GPU cache'; frames='Preparing frame buffers'; profiler='Preparing GPU timers'; targets='Preparing render targets'; swapchain='Preparing presentation'; pipelines='Compiling graphics pipelines' }
                        $stage = $Matches[1]
                        $message = if ($names.ContainsKey($stage)) { $names[$stage] } else { $stage }
                    }
                }
                $newLength = $reader.BaseStream.Length
                if ($newLength -ne $length) { $length = $newLength; $activityAt = $timer.Elapsed.TotalSeconds }
            }
            $Process.Refresh()
            try {
                $newCpu = $Process.TotalProcessorTime.Ticks
                if ($newCpu -ne $cpu) { $cpu = $newCpu; $activityAt = $timer.Elapsed.TotalSeconds }
            } catch { }
            if ($ErrorLog -and (Test-Path -LiteralPath $ErrorLog)) {
                $errorLength=(Get-Item -LiteralPath $ErrorLog).Length
                if ($errorLength -ne $previousErrorLength) { $previousErrorLength=$errorLength; $activityAt=$timer.Elapsed.TotalSeconds }
            }
            $idle=$timer.Elapsed.TotalSeconds-$activityAt
            if (!$finished -and $idle -ge 300) {
                try { $Process.Kill(); $null=$Process.WaitForExit(2000) } catch { }
                throw "$Label stopped responding: no CPU or log activity for five minutes. Setup has not changed your game files. See $OutputLog and $ErrorLog."
            }
            $detail = @{ elapsedSeconds=$timer.Elapsed.TotalSeconds; idleSeconds=($timer.Elapsed.TotalSeconds-$activityAt); logPath=$OutputLog }
            if ($ProgressCallback) { & $ProgressCallback $message -1 $detail }
            else { Write-Progress -Activity $Label -Status ($message + (' ({0:m\:ss})' -f $timer.Elapsed)) -PercentComplete -1 }
            if ($finished) { break }
            Start-Sleep -Milliseconds 300
        }
        $Process.WaitForExit()
        if ($Process.ExitCode -ne 0) {
            $errorText = if ($ErrorLog -and (Test-Path -LiteralPath $ErrorLog)) { (Get-Content -LiteralPath $ErrorLog -Tail 12) -join "`n" } else { '' }
            throw "$Label failed ($($Process.ExitCode)). $errorText See $OutputLog."
        }
    } finally {
        if ($reader) { $reader.Dispose() }
        $Process.Dispose()
    }
}
