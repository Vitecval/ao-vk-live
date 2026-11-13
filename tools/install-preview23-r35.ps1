param([string]$Client='F:\AO_SInstall\Anarchy Online')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'build\preview23\x86'
$work=Join-Path $root 'build\preview23\atmosphere-r35'
Start-Transcript -Path (Join-Path $work 'install-last.log') -Force | Out-Null
if((Get-Content -LiteralPath (Join-Path $work 'checks\RESULTS.json') -Raw | ConvertFrom-Json).result -ne 'passed'){throw 'Revision 35 validation is incomplete.'}
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
foreach($pair in @(@('RVK_AmbientOn','1'),@('RVK_AmbientStyle','0'),@('RVK_AmbientMotes','1.5'),@('RVK_AmbientBright','1'),@('RVK_HDGround','1'))) {
    $iniText=Set-IniValue $iniText 'Renderer' $pair[0] $pair[1]
}
$iniText=Set-IniValue $iniText 'Migrations' 'AmbientBright34' '1'
$iniText=Set-IniValue $iniText 'Migrations' 'AmbientScale31' '1'
$protected=@{}
foreach($name in @('Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')) {
    $protected[$name]=(Get-FileHash -LiteralPath (Join-Path $clientPath $name) -Algorithm SHA256).Hash
}
$staged=Join-Path $clientPath ('ao-vk-r35-'+$token+'.tmp')
# Stage before replacing any installed file; a denied write leaves the installation intact.
Copy-Item -LiteralPath (Join-Path $build 'ao-vk.dll') -Destination $staged
if((Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash -ine $newHash){throw 'Staged renderer hash mismatch.'}
Assert-Closed
if((Get-FileHash -LiteralPath $renderer -Algorithm SHA256).Hash -ine $oldHash){throw 'Installation changed during staging.'}
$replaced=$false
try {
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
    if($replaced){foreach($name in @('ao-vk.dll','ao-vk-live.install.json','randy-vk.ini')){Copy-Item -LiteralPath (Join-Path $backup $name) -Destination (Join-Path $clientPath $name) -Force}}
    throw
}
$result=[ordered]@{result='passed';revision=35;client=$clientPath;rendererSha256=$newHash;previousSha256=$oldHash;backup=$backup;ambient='Automatic';density=1.5;brightness=1;hdGround=1;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'INSTALL-RESULTS.json') -Encoding utf8
Write-Output 'Preview 23 revision 35 installed and verified on F:; Automatic ambient density 1.5 and brightness 1.'
Stop-Transcript | Out-Null
