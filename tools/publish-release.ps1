param([Parameter(Mandatory=$true)][string]$Version, [switch]$Publish)
$ErrorActionPreference='Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+(-[A-Za-z0-9.-]+)?$') {throw 'Invalid version'}
$root=Split-Path -Parent $PSScriptRoot
$release=Join-Path $root "dist/releases/$Version"
$prefix="ao-vk-live-$Version"
$names=@("$prefix-windows-x86-full.zip","$prefix-windows-x86-lite.zip","$prefix-source.zip",'SHA256SUMS.txt')
$checks=Get-Content -LiteralPath (Join-Path $release 'SHA256SUMS.txt')
foreach ($name in $names[0..2]) {
    if ((Get-Item -LiteralPath (Join-Path $release $name)).Length -ge 2GB) {throw "Release asset exceeds GitHub limit: $name"}
    $line=@($checks | Where-Object {$_ -match ('^[a-f0-9]{64}  '+[regex]::Escape($name)+'$')})
    if ($line.Count -ne 1 -or (Get-FileHash -LiteralPath (Join-Path $release $name) -Algorithm SHA256).Hash -ine $line[0].Substring(0,64)) {throw "Checksum failed: $name"}
}
$arguments=@('release','create',"v$Version",'--repo','Vitecval/ao-vk-live','--verify-tag','--title',"AO-VK Live $Version",'--notes-file',(Join-Path $root 'RELEASE-NOTES.md'))
if ($Version -match '-preview\.') {$arguments+='--prerelease'}
if (!$Publish) {$arguments+='--draft'}
$arguments+=@($names | ForEach-Object {Join-Path $release $_})
& gh @arguments
if ($LASTEXITCODE) {throw 'GitHub release creation failed'}
