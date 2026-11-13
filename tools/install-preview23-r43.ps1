param([string]$Client='F:\AO_SInstall\Anarchy Online', [switch]$Fixture)
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $forward=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,'-Client',$Client)
    if($Fixture){$forward+='-Fixture'}
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" @forward
    if($LASTEXITCODE){throw "Revision 43 installation failed (exit $LASTEXITCODE)."};return
}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\setup-safety.ps1')
$work=Join-Path $root 'build\preview23\atmosphere-r43'
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
    $expected=Join-Path $root 'build\preview23\atmosphere-r43\installer-fixture-client'
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
    foreach($name in @('randy-vk.ini','Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')){
        $protected[$name]=Get-AoSetupHash (Join-Path $clientPath $name)
    }
    # Additive, disposable prepared files are published before changing the
    # renderer. Interruption leaves valid cache entries and the old renderer.
    $cache=Join-Path $clientPath 'randy-vk\cache\materials'
    Assert-AoSetupPath $cache
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    $cacheCount=0
    foreach($entry in $manifest.files){
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
    foreach($target in @($renderer,$receiptPath)){
        $stage=Join-Path $clientPath ('.'+[IO.Path]::GetFileName($target)+'.installing-'+[guid]::NewGuid().ToString('N'));$stages+=$stage
        if($target -eq $renderer){Copy-Item -LiteralPath $candidate -Destination $stage;$before=$oldHash}
        else{[IO.File]::WriteAllText($stage,($receipt|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)));$before=$receiptHash}
        $entries.Add(@{target=$target;stage=$stage;before=$before;after=(Get-AoSetupHash $stage)})
    }
    Assert-AoSetupFolderIdle $clientPath
    Invoke-AoSetupTransaction $clientPath '' ($entries.ToArray())
    foreach($name in $protected.Keys){if((Get-AoSetupHash (Join-Path $clientPath $name)) -ine $protected[$name]){throw "Protected file changed: $name"}}
    if((Get-AoSetupHash $renderer) -ine $newHash){throw 'Installed renderer verification failed.'}
    $result=[ordered]@{result='passed';revision=43;client=$clientPath;rendererSha256=$newHash;previousSha256=$oldHash;backup=$backup;
        preparedMaterials=$cacheCount;settingsPreserved=$true;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
    $result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $work 'INSTALL-RESULTS.json') -Encoding UTF8
    Write-Output "Revision 43 installed: $cacheCount prepared materials and shadow fixes. Settings and artwork preserved."
} finally {
    if(!(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json'))){
        foreach($stage in $stages){if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -ErrorAction SilentlyContinue}}
    }
    if($lock){$lock.ReleaseMutex();$lock.Dispose()}
}
