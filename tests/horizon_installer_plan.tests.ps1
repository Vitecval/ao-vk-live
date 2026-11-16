param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$Output=[IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Path $Output -Force | Out-Null
. (Join-Path $root 'tools/setup-safety.ps1')
# Exercise the production planning code with the actual Full manifest and an
# existing matching receipt. No game directory is read, written or started.
$input=[IO.File]::OpenRead((Join-Path $root 'apps/horizon/Assets/full-manifest.json.gz'))
try {
 $gzip=New-Object IO.Compression.GZipStream($input,[IO.Compression.CompressionMode]::Decompress)
 $reader=New-Object IO.StreamReader($gzip)
 try {$manifest=$reader.ReadToEnd() | ConvertFrom-Json} finally {$reader.Dispose()}
} finally {$input.Dispose()}
$receipt=$manifest
$client=Join-Path $Output 'dummy-client'
$targets=New-Object 'Collections.Generic.List[string]'
$targets.Add((Join-Path $client 'ao-vk.dll'))
$targets.Add((Join-Path $client 'ddraw.dll'))
$fileSpecs=@{}
$preserveMeshIndex=$false
$meshRelative='randy-vk/mesh-ids.bin'
$source=[IO.File]::ReadAllText((Join-Path $root 'tools/install-live.ps1'))
$start=$source.IndexOf('$assetHashes =')
$end=$source.IndexOf("`$bindingsRelative =",$start)
if($start -lt 0 -or $end -lt $start){throw 'Cannot locate the production file-plan code.'}
$plan=[scriptblock]::Create($source.Substring($start,$end-$start))
$timer=[Diagnostics.Stopwatch]::StartNew()
. $plan
$seconds=$timer.Elapsed.TotalSeconds
$count=@($manifest.assets.PSObject.Properties).Count
if($count -lt 58000 -or $assetHashes.Count -ne $count -or $fileSpecs.Count -ne $count -or $targets.Count -ne $count+2){throw 'Full HD planning lost or duplicated files.'}
foreach($p in $manifest.assets.PSObject.Properties){if($newAssetHashes[$p.Name] -cne $p.Value -or $fileSpecs[(Join-Path $client $p.Name)].hash -cne $p.Value){throw 'A planned integrity hash changed.'}}
# Distinct old/new hashes prove that the current manifest wins, while retiring
# assets remain in the ownership plan for the backend to preserve/remove safely.
$manifest=[pscustomobject]@{assets=[pscustomobject]@{'randy-vk/loading/current.png'=('b'*64)}}
$receipt=[pscustomobject]@{assets=[pscustomobject]@{'randy-vk/loading/current.png'=('a'*64);'randy-vk/loading/retired.png'=('c'*64)}}
$targets.Clear();$fileSpecs=@{}
. $plan
if($fileSpecs[(Join-Path $client 'randy-vk/loading/current.png')].hash -cne ('b'*64) -or !$fileSpecs[(Join-Path $client 'randy-vk/loading/retired.png')].retire){throw 'Update or retired-file ownership changed.'}
@{result='passed';assets=$count;existingReceipt=$true;planningSeconds=$seconds;hashesPreserved=$true;newHashWins=$true;retiredAssetPreservedInPlan=$true;retailClientsChanged=$false;pointerBytes=[IntPtr]::Size}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Output 'RESULTS.json')
Write-Output "PASS: planned $count Full HD entries in $seconds seconds; all hashes and retirement rules preserved."
