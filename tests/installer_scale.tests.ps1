param([string]$Output)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\tools\setup-safety.ps1')
$folder=Join-Path $Output ('metadata-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$token=[guid]::NewGuid().ToString('N')
$entries=New-Object 'Collections.Generic.List[object]'
for($i=0;$i -lt 26916;$i++) {
    $target=Join-Path $folder ("randy-vk/packs/ao-cyber-world/1010004_"+$i+"_d.png")
    $stage=Join-Path ([IO.Path]::GetDirectoryName($target)) ('.'+[IO.Path]::GetFileName($target)+'.installing-'+$token)
    $entries.Add(@{target=$target;stage=$stage;before='';after=('a'*64);backup=($target+'.upgrade-'+$token)})
}
$journal=[pscustomobject]@{version=1;client=$folder;loader='';token=$token;committed=$false;entries=$entries.ToArray()}
$path=Join-Path $folder '.ao-vk-setup-transaction.json'
$timer=[Diagnostics.Stopwatch]::StartNew()
Save-AoTransaction $path $journal
$bytes=(Get-Item -LiteralPath $path).Length
$journal=$null;$entries=$null
Recover-AoSetupTransaction $folder
if(Test-Path -LiteralPath $path){throw 'Large recovery journal remained'}
$process=[Diagnostics.Process]::GetCurrentProcess();$process.Refresh()
@{result='passed';entries=26916;journalBytes=$bytes;seconds=$timer.Elapsed.TotalSeconds;peakWorkingSetMiB=($process.PeakWorkingSet64/1MB);pointerBytes=[IntPtr]::Size;scope='Full-HD-sized ownership/recovery metadata with nonexistent dummy targets, not a texture throughput benchmark'}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Output 'RESULTS.json')
Write-Output 'PASS: 26,916-file recovery metadata in the 32-bit installer host'
