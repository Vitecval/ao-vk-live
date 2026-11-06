# Native preparation remains outside the UI thread. Observe activity, never kill
# a working helper merely because a fixed wall-clock deadline has elapsed.
function New-AoSetupClock { return [Diagnostics.Stopwatch]::StartNew() }
function Wait-AoSetupHelper($Process, [string]$OutputLog, [string]$ErrorLog, [string]$Label, [scriptblock]$ProgressCallback) {
    $null = $Process.Handle
    $timer = New-AoSetupClock
    $activityAt = 0.0
    $cpu = -1L
    $length = -1L
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
            $detail = @{ elapsedSeconds=$timer.Elapsed.TotalSeconds; idleSeconds=($timer.Elapsed.TotalSeconds-$activityAt); logPath=$OutputLog }
            if ($ProgressCallback) { & $ProgressCallback $message -1 $detail }
            else { Write-Progress -Activity $Label -Status ($message + (' ({0:m\:ss})' -f $timer.Elapsed)) -PercentComplete -1 }
            if ($finished) { break }
            Start-Sleep -Milliseconds 300
        }
        $Process.WaitForExit()
        if ($Process.ExitCode -ne 0) {
            $errorText = if (Test-Path -LiteralPath $ErrorLog) { (Get-Content -LiteralPath $ErrorLog -Tail 12) -join "`n" } else { '' }
            throw "$Label failed ($($Process.ExitCode)). $errorText See $OutputLog."
        }
    } finally {
        if ($reader) { $reader.Dispose() }
        $Process.Dispose()
    }
}

function Preserve-AoInterruptedLivingLock([string]$Client) {
    $root = Join-Path $Client 'randy-vk\living-local'
    $lock = Join-Path $root 'preparing.lock'
    if (!(Test-Path -LiteralPath $lock)) { return }
    foreach ($path in @((Join-Path $Client 'randy-vk'), $root, $lock)) {
        if ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Local Living preparation paths may not be links.' }
    }
    $ownerText = (Get-Content -LiteralPath $lock -Raw).Trim()
    [int]$ownerId = 0
    if (![int]::TryParse($ownerText, [ref]$ownerId) -or $ownerId -le 0) { throw 'An unrecognized Living preparation lock was preserved. Inspect preparing.lock before retrying.' }
    try {
        $owner = [Diagnostics.Process]::GetProcessById($ownerId)
        try { if (!$owner.HasExited) { throw 'Another Living preparation process is still running. Wait for it to finish.' } }
        finally { $owner.Dispose() }
    } catch [ArgumentException] { }
    # Keep evidence of the interrupted run. Its unfinished generation and the
    # previous active cache are not removed or adopted by the installer.
    Move-Item -LiteralPath $lock -Destination (Join-Path $root ('interrupted-' + [guid]::NewGuid().ToString('N') + '.lock'))
}
