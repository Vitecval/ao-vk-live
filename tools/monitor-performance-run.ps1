param([string]$Client='F:\AO_SInstall\Anarchy Online',[int]$Minutes=10)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$session=Join-Path $root ('build\preview24\perf-run-r49\'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $session | Out-Null
$source=Join-Path $Client 'randy-vk.log';$ini=Join-Path $Client 'randy-vk.ini';Copy-Item -LiteralPath $ini -Destination (Join-Path $session 'settings-before.ini')
$targets=@()
foreach($p in @(Get-Process -Name AnarchyOnline,Anarchy -ErrorAction SilentlyContinue)){
    try{
        $path=$p.Path;$matches=$path -and $path.StartsWith($Client+'\',[StringComparison]::OrdinalIgnoreCase)
        if(!$matches){foreach($module in $p.Modules){if($module.FileName -and $module.FileName.StartsWith($Client+'\',[StringComparison]::OrdinalIgnoreCase)){$matches=$true;break}}}
        if($matches){$targets+=$p.Id}
    }catch{}
}
if(!$targets.Count){throw 'No running F: client was found.'}
$started=[DateTime]::UtcNow;$ready=[ordered]@{result='recording';client=$Client;pids=$targets;startedUtc=$started.ToString('o');folder=$session;durationLimitMinutes=$Minutes;graphicsSettingsChanged=$false;gpuTimings='existing renderer timestamps';samplingSeconds=1}
$ready|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $session 'SESSION.json') -Encoding UTF8
$ready|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'build\preview24\perf-run-r49\ACTIVE.json') -Encoding UTF8
$share=[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
$stream=[IO.FileStream]::new($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,$share)
$startOffset=$stream.Length;$stream.Position=$startOffset
$raw=[IO.StreamWriter]::new((Join-Path $session 'renderer-live.log'),$false,[Text.UTF8Encoding]::new($false));$events=[IO.StreamWriter]::new((Join-Path $session 'events.jsonl'),$false,[Text.UTF8Encoding]::new($false));$cpu=[IO.StreamWriter]::new((Join-Path $session 'process.csv'),$false,[Text.UTF8Encoding]::new($false))
$raw.AutoFlush=$events.AutoFlush=$cpu.AutoFlush=$true
$cpu.WriteLine('utc,pid,cpu_core_percent,cpu_machine_percent,working_set_mib,private_mib,threads,handles,responding')
$last=@{};$tail='';$decoder=[Text.Encoding]::UTF8.GetDecoder();$lineCount=0;$eventCount=0;$samples=0
Write-Output ('MONITOR READY: F: PID '+($targets -join ',')+'; '+$session)
try{
    while(([DateTime]::UtcNow-$started).TotalMinutes -lt $Minutes -and !(Test-Path -LiteralPath (Join-Path $session 'STOP'))){
        $now=[DateTime]::UtcNow
        if($stream.Length -lt $stream.Position){$stream.Position=0;$tail='';$decoder.Reset()}
        while($stream.Position -lt $stream.Length){
            $bytes=New-Object byte[] ([int][Math]::Min(65536,$stream.Length-$stream.Position));$n=$stream.Read($bytes,0,$bytes.Length);if(!$n){break}
            $chars=New-Object char[] ($n+2);$count=$decoder.GetChars($bytes,0,$n,$chars,0,$false);$tail+=[string]::new($chars,0,$count)
            $lines=$tail -split "`n";$tail=$lines[-1]
            for($i=0;$i -lt $lines.Count-1;$i++){
                $line=$lines[$i].TrimEnd("`r");$raw.WriteLine($line);$lineCount++
                if($line -match 'gpu ms:|cpu ms |slow frame:|presented \d+ frames:|point shadows:|lightning|strike|storm|ring buffer full|GPU pressure|retired cold|pipeline.*(driver|compil)|presentation:|time:|sky.*weather|sun shadow|ground grass:|building family|loading|zoning|playfield'){
                    $events.WriteLine((@{receivedUtc=$now.ToString('o');line=$line}|ConvertTo-Json -Compress));$eventCount++
                }
            }
        }
        $alive=0
        foreach($pidValue in $targets){
            $p=Get-Process -Id $pidValue -ErrorAction SilentlyContinue;if(!$p){continue};$alive++;$p.Refresh()
            $total=$p.TotalProcessorTime.TotalMilliseconds;$pct=0
            if($last.ContainsKey($pidValue)){$delta=($now-$last[$pidValue].time).TotalMilliseconds;if($delta -gt 0){$pct=100*($total-$last[$pidValue].cpu)/$delta}}
            $last[$pidValue]=@{time=$now;cpu=$total}
            $cpu.WriteLine(('{0},{1},{2:F2},{3:F2},{4:F1},{5:F1},{6},{7},{8}' -f $now.ToString('o'),$pidValue,$pct,($pct/[Environment]::ProcessorCount),($p.WorkingSet64/1MB),($p.PrivateMemorySize64/1MB),$p.Threads.Count,$p.HandleCount,$p.Responding));$samples++
        }
        if(!$alive){break}
        Start-Sleep -Milliseconds 1000
    }
}finally{
    $raw.Dispose();$events.Dispose();$cpu.Dispose();$stream.Dispose()
    $ready.result='complete';$ready.endedUtc=[DateTime]::UtcNow.ToString('o');$ready.lines=$lineCount;$ready.events=$eventCount;$ready.processSamples=$samples;$ready.startOffset=$startOffset
    $ready|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $session 'SESSION.json') -Encoding UTF8
    $ready|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'build\preview24\perf-run-r49\ACTIVE.json') -Encoding UTF8
    Write-Output "MONITOR COMPLETE: $lineCount log lines, $eventCount events, $samples process samples; $session"
}
