param(
 [Parameter(Mandatory=$true)][string]$SourceDir,
 [Parameter(Mandatory=$true)][string]$Destination,
 [Parameter(Mandatory=$true)][string]$Manifest
)
$ErrorActionPreference='Stop'
function Get-SourceHash([string]$Path) {
 $stream=[IO.File]::OpenRead($Path);$algorithm=[Security.Cryptography.SHA256]::Create()
 try {return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}
 finally {$stream.Dispose();$algorithm.Dispose()}
}
$source=(Resolve-Path -LiteralPath $SourceDir).Path.TrimEnd('\','/')
$target=(Resolve-Path -LiteralPath $Destination).Path.TrimEnd('\','/')
if($source -ieq $target -or !(Test-Path -LiteralPath (Join-Path $target '.git'))){throw 'Choose a separate existing Git clone.'}
$state=& git -c "safe.directory=$target" -C $target status --porcelain
if($LASTEXITCODE -ne 0){throw 'Cannot inspect destination Git state.'}
if($state){throw 'Destination has local changes. Preserve/review them before synchronization.'}
$inventory=Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
if($inventory.format -ne 1 -or !$inventory.files){throw 'Unsupported or empty source inventory.'}
$copies=@()
foreach($entry in $inventory.files){
 $relative=([string]$entry.path).Replace('\','/')
 if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)' -or $relative -match '(^|/)(\.git|private|source-art|maps)(/|$)' -or $relative -match '\.(zip|7z|log|dll|exe|obj|pdb|cache|dmp)$'){throw "Excluded source path: $relative"}
 $from=[IO.Path]::GetFullPath((Join-Path $source $relative));$to=[IO.Path]::GetFullPath((Join-Path $target $relative))
 if(!$from.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase) -or !$to.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Source inventory path escapes its root.'}
 if(!(Test-Path -LiteralPath $from) -or (Get-SourceHash $from) -ine $entry.sha256){throw "Source changed after preparation: $relative"}
 $copies+=@{from=$from;to=$to}
}
foreach($item in $copies){
 $parent=Split-Path -Parent $item.to
 if(!(Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Path $parent -Force | Out-Null}
 Copy-Item -LiteralPath $item.from -Destination $item.to -Force
}
Write-Output "Copied $($copies.Count) reviewed source files. No files removed; nothing staged, committed, tagged or pushed."
