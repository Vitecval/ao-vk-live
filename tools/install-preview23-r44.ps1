param([string]$Client='F:\AO_SInstall\Anarchy Online', [switch]$Fixture, [switch]$RendererOnly)
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $forward=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,'-Client',$Client)
    if($Fixture){$forward+='-Fixture'}
    if($RendererOnly){$forward+='-RendererOnly'}
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" @forward
    if($LASTEXITCODE){throw "Revision 44 installation failed (exit $LASTEXITCODE)."};return
}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\setup-safety.ps1')
$work=Join-Path $root 'build\preview23\atmosphere-r44'
if($Fixture){$work=Join-Path $work 'installer-fixture-package'}
$candidate=Join-Path $work 'candidate\ao-vk.dll'
$verified=Read-AoSetupJson (Join-Path $work 'VERIFIED.json')
$newHash=Get-AoSetupHash $candidate
if($verified.result -ne 'passed' -or $newHash -ine $verified.rendererSha256){throw 'Renderer validation or hash does not match.'}
$manifestPath=Join-Path $work 'MATERIAL-CACHE.json'
if((Get-AoSetupHash $manifestPath) -ine $verified.materialManifestSha256){throw 'Prepared texture manifest changed.'}
$manifest=Read-AoSetupJson $manifestPath
$clientPath=Get-AoSetupDirectory $Client 'AO test installation'
if($Fixture){
    $expected=Join-Path $root 'build\preview23\atmosphere-r44\installer-fixture-client'
    if($clientPath -ine $expected){throw 'Fixture mode only accepts the project-local fixture client.'}
} elseif($clientPath -ine 'F:\AO_SInstall\Anarchy Online'){throw 'This local trial targets the F: test installation only.'}
$lock=Enter-AoSetupLock $clientPath
$stages=@()
try {
    Assert-AoSetupFolderIdle $clientPath
    if(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json')){Recover-AoSetupTransaction $clientPath}
    $renderer=Join-Path $clientPath 'ao-vk.dll';$receiptPath=Join-Path $clientPath 'ao-vk-live.install.json'
    $receipt=Read-AoSetupJson $receiptPath;$oldHash=Get-AoSetupHash $renderer
    if($receipt.product -ne 'AO-VK Live' -or $receipt.clientDirectory -ine $clientPath -or !$oldHash -or $oldHash -ine $receipt.addon.'ao-vk.dll'){
        throw 'The installed renderer does not match its installation record; no renderer was changed.'
    }
    $protected=@{}
    foreach($name in @('Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')){
        $protected[$name]=Get-AoSetupHash (Join-Path $clientPath $name)
    }
    $ini=Join-Path $clientPath 'randy-vk.ini'
    $iniHash=Get-AoSetupHash $ini
    $encoding=[Text.Encoding]::GetEncoding(28591)
    $text=if($iniHash){$encoding.GetString([IO.File]::ReadAllBytes($ini))}else{''}
    $section=[regex]::Match($text,'(?ims)(^\[Renderer\][^\r\n]*\r?\n)(.*?)(?=^\[|\z)')
    if($section.Success){
        $body=$section.Groups[2].Value
        if($body -match '(?im)^RVK_SunDist='){$body=[regex]::Replace($body,'(?im)^RVK_SunDist=[^\r\n]*','RVK_SunDist=200')}
        else{if($body -and !$body.EndsWith("`n")){$body+="`r`n"};$body+="RVK_SunDist=200`r`n"}
        $text=$text.Substring(0,$section.Index)+$section.Groups[1].Value+$body+$text.Substring($section.Index+$section.Length)
    }else{$text+="`r`n[Renderer]`r`nRVK_SunDist=200`r`n"}
    $newIniBytes=$encoding.GetBytes($text)
    # Additive, disposable prepared files are published before changing the
    # renderer. Interruption leaves valid cache entries and the old renderer.
    $cache=Join-Path $clientPath 'randy-vk\cache\materials'
    Assert-AoSetupPath $cache
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $cacheCount=0
    $cacheEntries=@($manifest.files)
    if($RendererOnly){$cacheEntries=@()}
    foreach($entry in $cacheEntries){
        if($entry.name -cnotmatch '^[a-f0-9]{16}\.rvkm$' -or $entry.sha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Invalid cache entry in the manifest.'}
        $source=Join-Path (Join-Path $work 'prepared-materials-final') $entry.name
        $target=Join-Path $cache $entry.name
        Assert-AoSetupPath $target
        if((Get-AoSetupHash $source) -ine $entry.sha256){throw "Prepared texture changed: $($entry.name)"}
        $existing=Get-AoSetupHash $target
        if($existing -and $existing -ine $entry.sha256){throw "Existing prepared texture differs: $($entry.name). It was preserved."}
        if(!$existing){
            $stage=Join-Path $cache ('.'+$entry.name+'.installing-'+[guid]::NewGuid().ToString('N'));$stages+=$stage
            Copy-Item -LiteralPath $source -Destination $stage
            if((Get-AoSetupHash $stage) -ine $entry.sha256){throw 'Prepared texture copy verification failed.'}
            Move-Item -LiteralPath $stage -Destination $target
        }
        $cacheCount++
    }
    $backup=Join-Path $work ('install-backup-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach($file in @($renderer,$receiptPath,(Join-Path $clientPath 'randy-vk.ini'))){if(Test-Path -LiteralPath $file){Copy-Item -LiteralPath $file -Destination $backup}}
    $receiptHash=Get-AoSetupHash $receiptPath;$receipt.addon.'ao-vk.dll'=$newHash
    $entries=New-Object 'Collections.Generic.List[object]'
    foreach($target in @($renderer,$receiptPath,$ini)){
        $stage=Join-Path $clientPath ('.'+[IO.Path]::GetFileName($target)+'.installing-'+[guid]::NewGuid().ToString('N'));$stages+=$stage
        if($target -eq $renderer){Copy-Item -LiteralPath $candidate -Destination $stage;$before=$oldHash}
        elseif($target -eq $receiptPath){[IO.File]::WriteAllText($stage,($receipt|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)));$before=$receiptHash}
        else{[IO.File]::WriteAllBytes($stage,$newIniBytes);$before=$iniHash;$newIniHash=Get-AoSetupHash $stage}
        $entries.Add(@{target=$target;stage=$stage;before=$before;after=(Get-AoSetupHash $stage)})
    }
    Assert-AoSetupFolderIdle $clientPath
    Invoke-AoSetupTransaction $clientPath '' ($entries.ToArray())
    foreach($name in $protected.Keys){if((Get-AoSetupHash (Join-Path $clientPath $name)) -ine $protected[$name]){throw "Protected file changed: $name"}}
    if((Get-AoSetupHash $ini) -ine $newIniHash){throw 'The requested shadow range was not saved correctly.'}
    if((Get-AoSetupHash $renderer) -ine $newHash){throw 'Installed renderer verification failed.'}
    $result=[ordered]@{result='passed';revision=44;client=$clientPath;rendererSha256=$newHash;previousSha256=$oldHash;backup=$backup;
        preparedMaterials=$cacheCount;cacheDeliveryPending=[bool]$RendererOnly;otherSettingsPreserved=$true;sunShadowDistance=200;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
    $result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'INSTALL-RESULTS.json') -Encoding UTF8
    Write-Output "Revision 44 installed: $cacheCount prepared materials and shadow fixes. Shadow range 200 m; all other settings and artwork preserved."
} finally {
    if(!(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json'))){
        foreach($stage in $stages){if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -ErrorAction SilentlyContinue}}
    }
    if($lock){$lock.ReleaseMutex();$lock.Dispose()}
}
