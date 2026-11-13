param(
 [Parameter(Mandatory=$true)][string]$Version,
 [string]$SourceRepo='',
 [switch]$CheckOnly,
 [switch]$PrepareSource,
 [switch]$CommitSource,
 [switch]$Publish
)
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+(-[A-Za-z0-9.-]+)?$'){throw 'Invalid version'}
$steps=0;foreach($step in @($PrepareSource,$CommitSource,$Publish)){if($step){$steps++}}
if($steps -gt 1 -or ($CheckOnly -and $steps)){throw 'Choose one explicit mutation step, or CheckOnly.'}
$project=Split-Path -Parent $PSScriptRoot
$release=Join-Path $project "dist/releases/$Version"
if(!$SourceRepo){$SourceRepo=Join-Path (Split-Path -Parent $project) 'ao-vk-live-update'}
$tag="v$Version";$branch="codex/release-$Version";$repo='Vitecval/ao-vk-live'
$names=@("ao-vk-live-$Version-windows-x86-lite.zip","ao-vk-live-$Version-windows-x86-full.zip",'SHA256SUMS.txt')
$full=$null
$setupName="ao-vk-live-$Version-windows-x86-full-setup.zip"
if(Test-Path -LiteralPath (Join-Path $release $setupName)){
 $full=Get-Content -LiteralPath (Join-Path $release "ao-vk-live-$Version-windows-x86-full/manifest.json") -Raw | ConvertFrom-Json
 . (Join-Path $PSScriptRoot 'material-download.ps1')
 $names=@("ao-vk-live-$Version-windows-x86-lite.zip",$setupName)
 foreach($part in $full.materialParts){
  Assert-AoMaterialRelease $full $part
  if($part.sha256 -notmatch '^[a-f0-9]{64}$'){throw 'Invalid pinned release asset hash.'}
  $names+=[string]$part.name
 }
 $names+='SHA256SUMS.txt'
}
function Hash([string]$Path){return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Git {
 $result=& git -c "safe.directory=$SourceRepo" -C $SourceRepo @args
 if($LASTEXITCODE -ne 0){throw "Git failed: $args"};return $result
}
function Gh {
 $result=& gh @args
 if($LASTEXITCODE -ne 0){throw "GitHub operation failed: $args"};return $result
}
function Check-Paths([string]$Root){
 foreach($entry in $inventory.files){
  $relative=([string]$entry.path).Replace('\','/')
  if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|/)\.\.(/|$)' -or $relative -match '(^|/)(\.git|private|maps|source-art|build|dist)(/|$)' -or $relative -match '\.(dll|exe|obj|pdb|lib|zip|7z|log|cache|dmp|bak)$'){throw "Excluded source path: $relative"}
  $path=Join-Path $Root $relative
  if(!(Test-Path -LiteralPath $path) -or (Hash $path) -ine $entry.sha256){throw "Source differs from frozen tested snapshot: $relative"}
 }
}
function Check-Clone {
 if(!(Test-Path -LiteralPath (Join-Path $SourceRepo '.git'))){throw 'Choose the separate publishing Git clone.'}
 $origin=Git remote get-url origin
 if($origin -notmatch '^(https://github.com/|git@github.com:)Vitecval/ao-vk-live(?:\.git)?$'){throw "Unexpected publishing origin: $origin"}
}
$validation=Get-Content -LiteralPath (Join-Path $release 'validation.json') -Raw | ConvertFrom-Json
if($validation.version -ne $Version -or @($validation.critical_failures).Count -ne 0 -or $validation.release_state -ne 'ready_for_experimental_prerelease_publication'){throw 'Release validation is incomplete or failed.'}
$manifest=Join-Path $release 'SOURCE-FILES.json'
if((Hash $manifest) -ine $validation.source.source_manifest_sha256){throw 'Source manifest fingerprint changed.'}
$inventory=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
if($inventory.format -ne 1 -or !$inventory.files){throw 'Unsupported source inventory.'}
$snapshot=[string]$validation.source.snapshot
Check-Paths $snapshot
$sums=Get-Content -LiteralPath (Join-Path $release 'SHA256SUMS.txt')
if(@($sums).Count -ne ($names.Count-1)){throw 'Release checksums do not cover the complete installer and material asset set.'}
foreach($name in @($names | Where-Object {$_ -like '*.zip'})){
 $path=Join-Path $release $name
 if((Get-Item -LiteralPath $path).Length -ge 2GB){throw "Release asset exceeds 2 GiB: $name"}
 $line=@($sums | Where-Object {$_ -match ('^[a-f0-9]{64}  '+[regex]::Escape($name)+'$')})
 $actual=Hash $path
 if($line.Count -ne 1 -or $actual -ine $line[0].Substring(0,64)){throw "Archive checksum failed: $name"}
 if($full){
  $record=@($validation.archives.downloads | Where-Object {$_.name -ceq $name})
  if($record.Count -ne 1 -or $actual -ine $record[0].sha256){throw "Archive differs from validation receipt: $name"}
  $part=@($full.materialParts | Where-Object {$_.name -ceq $name})
  if($part.Count -and $actual -ine $part[0].sha256){throw "Download pin differs from release asset: $name"}
 } else {
  $edition=if($name -like '*-lite.zip'){'lite'}else{'full'}
  if($actual -ine $validation.archives.editions.$edition.sha256){throw "Archive differs from validation receipt: $name"}
 }
}
$notes=Join-Path $release 'RELEASE-NOTES.md'
if((Hash $notes) -ine $validation.publication.release_notes_sha256){throw 'Public release notes changed after preparation.'}
if(!$steps){
 Write-Output 'Frozen source, release notes, archive hashes and GitHub size limits passed. No Git/GitHub mutation or network command executed.'
 return
}
Check-Clone
if($PrepareSource){
 if(Git status --porcelain){throw 'Publishing clone has local changes; preserve and review them first.'}
 $review=Get-Content -LiteralPath (Join-Path $release 'SOURCE-CHANGES.json') -Raw | ConvertFrom-Json
 if((Git rev-parse HEAD).Trim() -ne $review.clone_commit){throw 'Publishing clone moved after review; refresh SOURCE-CHANGES before syncing.'}
 & git -c "safe.directory=$SourceRepo" -C $SourceRepo show-ref --verify --quiet "refs/heads/$branch"
 if($LASTEXITCODE -eq 0){throw 'Release branch already exists; inspect it instead of overwriting it.'}
 if($LASTEXITCODE -ne 1){throw 'Cannot inspect release branch.'}
 Git switch -c $branch | Write-Output
 & (Join-Path $PSScriptRoot 'sync-release-source.ps1') -SourceDir $snapshot -Destination $SourceRepo -Manifest $manifest
 Check-Paths $SourceRepo
 Git rm --cached --ignore-unmatch -- randy-vk.log | Write-Output
 Git --literal-pathspecs add "--pathspec-from-file=$(Join-Path $release 'SOURCE-PATHS.txt')" | Write-Output
 Git diff --cached --check | Write-Output
 Git diff --cached --stat | Write-Output
 Git diff --cached --name-status | Write-Output
 Write-Output 'Approved source staged for review. Review git diff --cached and SOURCE-CHANGES.json. Nothing committed, tagged, pushed or published. Then use -CommitSource.'
 return
}
if($CommitSource){
 if((Git branch --show-current).Trim() -ne $branch){throw "Expected prepared branch $branch"}
 Check-Paths $SourceRepo
 $approved=@{};foreach($entry in $inventory.files){$approved[[string]$entry.path]=$true}
 # The historical tracked runtime log is the only approved removal. Keep its local file.
 Git rm --cached --ignore-unmatch -- randy-vk.log | Write-Output
 Git --literal-pathspecs add "--pathspec-from-file=$(Join-Path $release 'SOURCE-PATHS.txt')" | Write-Output
 foreach($path in @(Git diff --cached --name-only --diff-filter=ACMR)){
  if(!$approved.ContainsKey($path)){throw "Unapproved staged file: $path"}
 }
 foreach($path in @(Git diff --cached --name-only --diff-filter=D)){
  if($path -ne 'randy-vk.log'){throw "Unapproved staged deletion: $path"}
 }
 foreach($path in @(Git ls-files)){
  if($path -match '(^|/)(maps|source-art|build|dist)(/|$)|\.(dll|exe|obj|pdb|lib|zip|log|cache)$'){throw "Generated/private file tracked in publishing clone: $path"}
 }
 Git diff --cached --check | Write-Output
 Git diff --cached --stat | Write-Output
 Git commit -m "Prepare AO-VK Live $tag" | Write-Output
 Check-Paths $SourceRepo
 if(Git status --porcelain){throw 'Unexpected working-tree changes remain after commit.'}
 $commit=(Git rev-parse HEAD).Trim()
 Set-Content -LiteralPath (Join-Path $release 'PUBLISH-COMMIT.txt') -Value $commit -Encoding ascii
 Write-Output "Prepared verified commit $commit. Nothing tagged, pushed or published. Use -Publish only after explicit publication authorization."
 return
}
# Only this explicitly requested step contacts GitHub or pushes a branch/tag.
Check-Paths $SourceRepo
if(Git status --porcelain){throw 'Publishing clone must be clean.'}
$commit=(Get-Content -LiteralPath (Join-Path $release 'PUBLISH-COMMIT.txt') -Raw).Trim()
if((Git rev-parse HEAD).Trim() -ne $commit -or (Git branch --show-current).Trim() -ne $branch){throw 'Publishing HEAD/branch differs from the verified release commit.'}
& git -c "safe.directory=$SourceRepo" -C $SourceRepo show-ref --verify --quiet "refs/tags/$tag"
if($LASTEXITCODE -eq 0){
 if((Git rev-parse "$tag^{}").Trim() -ne $commit){throw 'Existing local tag targets another commit; never overwrite it.'}
}elseif($LASTEXITCODE -eq 1){Git tag -a $tag $commit -m "AO-VK Live $tag" | Write-Output}else{throw 'Cannot inspect local tag.'}
$remoteTag=@(Git ls-remote --tags origin "refs/tags/$tag^{}")
if($remoteTag.Count -and $remoteTag[0].Split("`t")[0] -ne $commit){throw 'Remote tag differs; refusing overwrite.'}
Git push origin "${branch}:refs/heads/$branch" "refs/tags/${tag}:refs/tags/$tag" | Write-Output
$remote=& gh api "repos/$repo/releases/tags/$tag" 2>&1
if($LASTEXITCODE -ne 0){
 if(($remote -join "`n") -notmatch '404'){throw 'Cannot inspect GitHub release; verify authentication/network before retrying.'}
 $create=@('release','create',$tag,'--repo',$repo,'--verify-tag','--draft','--title',"AO-VK Live $tag",'--notes-file',$notes)
 if($Version -match '-preview\.'){$create+='--prerelease'}
 Gh @create | Write-Output
 $remote=Gh api "repos/$repo/releases/tags/$tag"
}
$state=$remote | ConvertFrom-Json
foreach($name in $names){
 $path=Join-Path $release $name;$expected=Hash $path
 $assets=@($state.assets | Where-Object {$_.name -ceq $name})
 if($assets.Count -gt 1){throw "Ambiguous remote asset: $name"}
 if($assets.Count -eq 1){
  # Never use --clobber. Matching already uploaded assets are safe to resume.
  $details=Gh api "repos/$repo/releases/assets/$($assets[0].id)" | ConvertFrom-Json
  if($details.digest -ne "sha256:$expected" -or $details.size -ne (Get-Item -LiteralPath $path).Length){throw "Existing asset cannot be verified or differs: $name. Preserve it and inspect before retrying."}
  Write-Output "Already uploaded and verified: $name"
 }else{
  if(!$state.draft){throw 'Published release is missing an asset; inspect before changing a public release.'}
  Gh release upload $tag $path --repo $repo | Write-Output
  $fresh=Gh api "repos/$repo/releases/tags/$tag" | ConvertFrom-Json
  $asset=@($fresh.assets | Where-Object {$_.name -ceq $name})
  if($asset.Count -ne 1){throw "Upload verification missing: $name"}
  $details=Gh api "repos/$repo/releases/assets/$($asset[0].id)" | ConvertFrom-Json
  if($details.digest -ne "sha256:$expected"){throw "Uploaded asset hash differs: $name"}
 }
}
if($state.draft){Gh release edit $tag --repo $repo --draft=false --notes-file $notes | Write-Output}
Write-Output "Published $tag with verified assets; no force push or asset overwrite used."
