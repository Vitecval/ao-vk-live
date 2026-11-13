param(
    [string]$RendererLog = 'randy-vk.log',
    [string]$LoaderLog = 'ao-vk-loader.log',
    [double]$PlayToResponsiveSeconds = -1
)
$ErrorActionPreference = 'Stop'
'AO-VK startup diagnostics (read-only)'
if ($PlayToResponsiveSeconds -ge 0) {
    'User-measured Play to responsive game window: {0:F3} seconds' -f $PlayToResponsiveSeconds
} else {
    'Play to responsive game window: NOT MEASURED. Renderer timing is a different interval.'
}
foreach ($entry in @(@{Kind='Loader';Path=$LoaderLog},@{Kind='Renderer';Path=$RendererLog})) {
    "`n$($entry.Kind) log: $($entry.Path)"
    if (!(Test-Path -LiteralPath $entry.Path -PathType Leaf)) { 'Log unavailable.'; continue }
    $lines = @(Get-Content -LiteralPath $entry.Path | Where-Object {
        $_ -match 'startup|pipeline cache|device creation failed|frame generation: off|FSR .*(failed|could not)|swapchain .*failed'
    } | Select-Object -Last 80)
    foreach ($line in $lines) {
        $line
        if ($line -match 'process-start-filetime-100ns=(\d+)') {
            '  Process creation UTC: ' + [DateTime]::FromFileTimeUtc([long]$Matches[1]).ToString('o')
        }
    }
    if (!$lines.Count) { 'No startup records found. Collect the log shortly after a slow launch.' }
}
'GPU driver cache resets and real client responsiveness must be recorded separately.'
