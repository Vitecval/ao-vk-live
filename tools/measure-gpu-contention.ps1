param([string]$Output='build\preview24\icc-r58\gpu-sample.json',[int]$Samples=5)
$ErrorActionPreference='Stop'
$start=[DateTime]::UtcNow
$timer=[Diagnostics.Stopwatch]::StartNew()
$cpuBefore=@{}
foreach($p in @(Get-Process -Name AnarchyOnline -ErrorAction SilentlyContinue)){$cpuBefore[$p.Id]=$p.TotalProcessorTime.TotalSeconds}
$measurements=Get-Counter -Counter '\GPU Engine(*)\Utilization Percentage' -SampleInterval 1 -MaxSamples $Samples
$timer.Stop()
$engines=@($measurements | ForEach-Object {$_.CounterSamples} | Where-Object {$_.InstanceName -match '^pid_\d+_'} |
    Group-Object InstanceName | ForEach-Object {
        $instance=$_.Name
        $processId=[int]([regex]::Match($instance,'^pid_(\d+)_')).Groups[1].Value
        $p=Get-Process -Id $processId -ErrorAction SilentlyContinue
        [pscustomobject]@{pid=$processId;process=$p.ProcessName;path=$p.Path;engine=$instance;
            averageUtilization=(($_.Group.CookedValue|Measure-Object -Average).Average);samples=$_.Count}
    })
$cpu=@(Get-Process -Name AnarchyOnline -ErrorAction SilentlyContinue | ForEach-Object {
    if($cpuBefore.ContainsKey($_.Id)){
        $seconds=$_.TotalProcessorTime.TotalSeconds-$cpuBefore[$_.Id]
        [pscustomobject]@{pid=$_.Id;path=$_.Path;cpuSeconds=$seconds;
            averageCpuPercent=100*$seconds/$timer.Elapsed.TotalSeconds/[Environment]::ProcessorCount}
    }
})
$ao3d=@($engines|Where-Object {$_.process -ieq 'AnarchyOnline' -and $_.engine -match 'engtype_3d$'}|
    Group-Object pid | ForEach-Object {
        [pscustomobject]@{pid=[int]$_.Name;path=$_.Group[0].path;average3dPercent=($_.Group.averageUtilization|Measure-Object -Sum).Sum}
    }|Sort-Object average3dPercent -Descending)
$result=[ordered]@{startedUtc=$start.ToString('o');endedUtc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=$timer.Elapsed.TotalSeconds;
    sampleCount=$Samples;ao3d=$ao3d;aoCpu=$cpu;engines=@($engines|Where-Object averageUtilization -gt .1|Sort-Object averageUtilization -Descending)}
$result|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $Output -Encoding utf8
$ao3d|ConvertTo-Json
