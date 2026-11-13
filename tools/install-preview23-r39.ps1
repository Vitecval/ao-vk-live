param([string]$Client='F:\AO_SInstall\Anarchy Online')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'build\preview23\atmosphere-r39\payload'
$work=Join-Path $root 'build\preview23\atmosphere-r39'
Start-Transcript -Path (Join-Path $work 'install-last.log') -Force | Out-Null
foreach($record in @('checks\RESULTS.json','stability-checks\RESULTS.json','ZONING-CHECK.json')) {
    $check=Get-Content -LiteralPath (Join-Path $work $record) -Raw | ConvertFrom-Json
    if($check.result -ne 'passed' -or $check.rendererSha256 -ine (Get-FileHash -LiteralPath (Join-Path $build 'ao-vk.dll') -Algorithm SHA256).Hash){throw 'Revision 39 validation is incomplete or payload changed.'}
}
$manifest=Get-Content -LiteralPath (Join-Path $work 'PAYLOAD.json') -Raw | ConvertFrom-Json
$artNames=@('01-frontier-bridge.png','02-the-ring.png','03-icc-grid.png')
foreach($name in @('ao-vk.dll')+@($artNames | ForEach-Object { 'randy-vk/loading/'+$_ })) {
    $entry=$manifest.files.PSObject.Properties[$name]
    if(!$entry -or (Get-FileHash -LiteralPath (Join-Path $build $name) -Algorithm SHA256).Hash -ine $entry.Value){throw "Payload hash mismatch: $name"}
}
$clientPath=(Resolve-Path -LiteralPath $Client).Path.TrimEnd('\')
if($clientPath -ine 'F:\AO_SInstall\Anarchy Online'){throw 'This local update targets the F: test installation only.'}
function Assert-Closed {
    foreach($process in @(Get-Process -Name Anarchy,AnarchyOnline,AnarchyPatcher,randy_harness -ErrorAction SilentlyContinue)) {
        if($process.HasExited){continue}
        $processPath=$process.Path
        if(!$processPath){throw 'A running AO process could not be inspected.'}
        if($processPath.StartsWith($clientPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Close the F: game and launcher before installing.'}
    }
}
Assert-Closed
$renderer=Join-Path $clientPath 'ao-vk.dll'
$receiptPath=Join-Path $clientPath 'ao-vk-live.install.json'
$ini=Join-Path $clientPath 'randy-vk.ini'
$receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if($receipt.product -ne 'AO-VK Live' -or $receipt.clientDirectory -ine $clientPath){throw 'Unexpected installation record.'}
$oldHash=(Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash
if($oldHash -ine $receipt.addon.'ao-vk.dll'){throw 'Renderer differs from its installation record.'}
$newHash=(Get-FileHash -LiteralPath (Join-Path $build 'ao-vk.dll') -Algorithm SHA256).Hash
if($newHash -ine (Get-Content -LiteralPath (Join-Path $work 'checks\RESULTS.json') -Raw | ConvertFrom-Json).rendererSha256){throw 'Renderer changed after validation.'}
$token=[guid]::NewGuid().ToString('N')
$backup=Join-Path $work ('install-backup-'+$token)
New-Item -ItemType Directory -Path $backup | Out-Null
foreach($path in @($renderer,$receiptPath,$ini)){Copy-Item -LiteralPath $path -Destination $backup}
$encoding=[Text.Encoding]::GetEncoding(28591)
$receiptText=$encoding.GetString([IO.File]::ReadAllBytes($receiptPath))
$pattern='(?i)("ao-vk\.dll"\s*:\s*")'+[regex]::Escape($oldHash)+'(")'
if([regex]::Matches($receiptText,$pattern).Count -ne 1){throw 'Unexpected renderer hash entries.'}
$receiptText=[regex]::Replace($receiptText,$pattern,('${1}'+$newHash.ToLowerInvariant()+'${2}'))
$iniText=$encoding.GetString([IO.File]::ReadAllBytes($ini))
function Set-IniValue([string]$Text,[string]$Section,[string]$Key,[string]$Value) {
    $sectionPattern='(?ims)(^\['+[regex]::Escape($Section)+'\][^\r\n]*\r?\n)(.*?)(?=^\[|\z)'
    $match=[regex]::Match($Text,$sectionPattern)
    if(!$match.Success){return $Text+"`r`n[$Section]`r`n$Key=$Value`r`n"}
    $body=$match.Groups[2].Value
    $keyPattern='(?m)^'+[regex]::Escape($Key)+'=[^\r\n]*'
    if([regex]::IsMatch($body,$keyPattern)){$body=[regex]::Replace($body,$keyPattern,($Key+'='+$Value))}
    else{$body+="$Key=$Value`r`n"}
    return $Text.Substring(0,$match.Index)+$match.Groups[1].Value+$body+$Text.Substring($match.Index+$match.Length)
}
foreach($pair in @(@('RVK_AmbientOn','1'),@('RVK_AmbientStyle','0'),@('RVK_AmbientMotes','2'),@('RVK_AmbientBright','1.5'),@('RVK_HDGround','1'),@('RVK_ImprovedTiles','0'),@('RVK_SkyClouds','1'),@('RVK_SkyTraffic','1'),@('RVK_CloudsOn','0'))) {
    $iniText=Set-IniValue $iniText 'Renderer' $pair[0] $pair[1]
}
$iniText=Set-IniValue $iniText 'Renderer' 'RVK_ZoneScreen' '1'
$iniText=Set-IniValue $iniText 'Migrations' 'AmbientBright34' '1'
$iniText=Set-IniValue $iniText 'Migrations' 'AmbientScale31' '1'
$loadingPath=Join-Path $clientPath 'randy-vk\loading'
$assetBackup=Join-Path $backup 'loading'
New-Item -ItemType Directory -Path $assetBackup | Out-Null
$existingArt=@{}
foreach($name in $artNames) {
    $path=Join-Path $loadingPath $name
    $existingArt[$name]=Test-Path -LiteralPath $path
    if($existingArt[$name]){Copy-Item -LiteralPath $path -Destination (Join-Path $assetBackup $name)}
}
$updatedReceipt=$receiptText | ConvertFrom-Json
if(!$updatedReceipt.assets){$updatedReceipt | Add-Member -NotePropertyName assets -NotePropertyValue ([pscustomobject]@{})}
foreach($name in $artNames) {
    $key='randy-vk/loading/'+$name
    $updatedReceipt.assets | Add-Member -NotePropertyName $key -NotePropertyValue $manifest.files.PSObject.Properties[$key].Value -Force
}
$receiptText=$updatedReceipt | ConvertTo-Json -Depth 30
$protected=@{}
foreach($name in @('Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')) {
    $protected[$name]=(Get-FileHash -LiteralPath (Join-Path $clientPath $name) -Algorithm SHA256).Hash
}
$staged=Join-Path $clientPath ('ao-vk-r39-'+$token+'.tmp')
# Stage before replacing any installed file; a denied write leaves the installation intact.
Copy-Item -LiteralPath (Join-Path $build 'ao-vk.dll') -Destination $staged
if((Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -ine $newHash){throw 'Staged renderer hash mismatch.'}
Assert-Closed
if((Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash -ine $oldHash){throw 'Installation changed during staging.'}
$replaced=$false
$changedArt=@()
try {
    New-Item -ItemType Directory -Path $loadingPath -Force | Out-Null
    foreach($name in $artNames) {
        $changedArt+=$name
        Copy-Item -LiteralPath (Join-Path $build ('randy-vk/loading/'+$name)) -Destination (Join-Path $loadingPath $name) -Force
        if((Get-FileHash -LiteralPath (Join-Path $loadingPath $name) -Algorithm SHA256).Hash -ine $manifest.files.PSObject.Properties['randy-vk/loading/'+$name].Value){throw "Installed artwork hash mismatch: $name"}
    }
    [IO.File]::Replace($staged,$renderer,[NullString]::Value);$replaced=$true
    [IO.File]::WriteAllBytes($receiptPath,$encoding.GetBytes($receiptText))
    [IO.File]::WriteAllBytes($ini,$encoding.GetBytes($iniText))
    if((Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash -ine $newHash){throw 'Installed renderer hash mismatch.'}
    if((Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json).addon.'ao-vk.dll' -ine $newHash){throw 'Receipt verification failed.'}
    if($encoding.GetString([IO.File]::ReadAllBytes($ini)) -cne $iniText){throw 'Settings verification failed.'}
    foreach($name in $protected.Keys) {
        if((Get-FileHash -LiteralPath (Join-Path $clientPath $name) -Algorithm SHA256).Hash -ine $protected[$name]){throw "Protected file changed: $name"}
    }
} catch {
    foreach($name in $changedArt) {
        $path=Join-Path $loadingPath $name
        if($existingArt[$name]){Copy-Item -LiteralPath (Join-Path $assetBackup $name) -Destination $path -Force}
        elseif(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
    }
    if($replaced){foreach($name in @('ao-vk.dll','ao-vk-live.install.json','randy-vk.ini')){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $clientPath $name) -Force}}
    throw
}
$result=[ordered]@{result='passed';revision=39;client=$clientPath;rendererSha256=$newHash;previousSha256=$oldHash;backup=$backup;ambient='Automatic';density=2;brightness=1.5;hdGround=1;iccTiles=0;skyClouds=1;skyTraffic=1;legacyClouds=0;zoningScreen=1;sharedGroundPlusPebbles=0.5;loadingArtwork=$artNames;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'INSTALL-RESULTS.json') -Encoding utf8
Write-Output "Preview 23 revision 39 installed and verified in $clientPath; zoning artwork on, Frontier Bridge for login loading, Shared Ground+ pebbles at half density."
Stop-Transcript | Out-Null
