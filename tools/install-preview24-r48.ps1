param([string]$Client='F:\AO_SInstall\Anarchy Online',[switch]$Fixture,[switch]$Revert)
$ErrorActionPreference='Stop'
if([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core'){
    $forward=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,'-Client',$Client)
    if($Fixture){$forward+='-Fixture'};if($Revert){$forward+='-Revert'}
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" @forward
    if($LASTEXITCODE){throw "Local update failed (exit $LASTEXITCODE)."};return
}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\setup-safety.ps1')
$work=Join-Path $root 'build\preview24\rocks-r48\production'
$clientPath=Get-AoSetupDirectory $Client 'AO test installation'
$expected=if($Fixture){Join-Path $work 'installer-fixture-client'}else{'F:\AO_SInstall\Anarchy Online'}
if($clientPath -ine $expected){throw 'This trial only targets its F: test client or the isolated fixture.'}
$recordPath=Join-Path $work $(if($Fixture){'INSTALL-FIXTURE.json'}else{'INSTALL-RESULTS.json'})
$verified=Read-AoSetupJson (Join-Path $work 'VERIFIED.json')
$candidate=Join-Path $work 'candidate\ao-vk.dll'
$candidateHash=Get-AoSetupHash $candidate
if($verified.result -ne 'passed' -or $candidateHash -cne $verified.rendererSha256){throw 'The candidate does not match its completed checks.'}
if(!$Fixture -and (Read-AoSetupJson (Join-Path $work 'INSTALLER-TESTS.json')).result -ne 'passed'){throw 'The isolated install and revert checks must pass first.'}
$names=@('rock_face_03_diff_2k.jpg','rock_face_03_nor_gl_2k.jpg','rock_face_03_arm_1k.jpg','rock-face-03-source.json')
$assets=@{}
foreach($name in $names){
    $rel='randy-vk/packs/EnhancedRocks/'+$name
    $pin=$verified.assets.$rel
    if(!$pin -or (Get-AoSetupHash (Join-Path $work ('candidate\'+$rel))) -cne $pin){throw "Surface asset differs from its checks: $name"}
    $assets[$rel]=$pin
}
$lock=Enter-AoSetupLock $clientPath
$stages=@()
try{
    Assert-AoSetupFolderIdle $clientPath
    if(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json')){Recover-AoSetupTransaction $clientPath}
    $renderer=Join-Path $clientPath 'ao-vk.dll';$receiptPath=Join-Path $clientPath 'ao-vk-live.install.json'
    $receipt=Read-AoSetupJson $receiptPath;$oldHash=Get-AoSetupHash $renderer;$receiptHash=Get-AoSetupHash $receiptPath
    if($receipt.product -cne 'AO-VK Live' -or $receipt.clientDirectory -ine $clientPath -or !$oldHash -or $oldHash -cne $receipt.addon.'ao-vk.dll'){
        throw 'The renderer and installation record differ. Both were preserved.'
    }
    $protected=@{}
    foreach($name in @('randy-vk.ini','Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')){
        $protected[$name]=Get-AoSetupHash (Join-Path $clientPath $name)
    }
    $beforeAssets=@{}
    foreach($rel in $assets.Keys){
        $target=Join-Path $clientPath $rel;Assert-AoSetupPath $target
        $current=Get-AoSetupHash $target
        if($current -and $current -cne $receipt.assets.$rel){throw "Existing surface file is not owned by this installation: $rel"}
        if(!$current -and $receipt.assets.$rel){throw "Owned surface file is missing: $rel"}
        $beforeAssets[$rel]=$current
    }
    $pairs=New-Object 'Collections.Generic.List[object]'
    if($Revert){
        $installed=Read-AoSetupJson $recordPath
        if($installed.client -ine $clientPath -or $installed.result -ne 'passed' -or $oldHash -cne $installed.rendererSha256 -or $receiptHash -cne $installed.receiptSha256){throw 'The installation changed since this trial. No automatic revert was applied.'}
        $backup=[IO.Path]::GetFullPath($installed.backup)
        if(!$backup.StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'The saved backup is outside this trial.'}
        foreach($rel in $assets.Keys){
            if($beforeAssets[$rel] -cne $installed.assets.$rel){throw 'A surface file changed since installation. Revert preserved it.'}
            $oldAsset=[string]$installed.previousAssets.$rel
            $source=if($oldAsset){Join-Path $backup $rel}else{''}
            if($source -and (Get-AoSetupHash $source) -cne $oldAsset){throw 'A saved surface backup changed.'}
            $pairs.Add(@{target=(Join-Path $clientPath $rel);source=$source;before=$beforeAssets[$rel];after=$oldAsset})
        }
        $sourceRenderer=Join-Path $backup 'ao-vk.dll';$sourceReceipt=Join-Path $backup 'ao-vk-live.install.json'
        $newHash=Get-AoSetupHash $sourceRenderer
        if($newHash -cne $installed.previousSha256 -or (Get-AoSetupHash $sourceReceipt) -cne $installed.previousReceiptSha256){throw 'The saved backup changed.'}
    }else{
        $complete=$oldHash -ceq $candidateHash
        foreach($rel in $assets.Keys){if($beforeAssets[$rel] -cne $assets[$rel]){$complete=$false}}
        if($complete){Write-Output 'The verified local rock trial is already installed.';return}
        $backup=Join-Path $work ('install-backup-'+[guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $backup | Out-Null
        Copy-Item -LiteralPath $renderer,$receiptPath -Destination $backup
        if((Get-AoSetupHash (Join-Path $backup 'ao-vk.dll')) -cne $oldHash -or (Get-AoSetupHash (Join-Path $backup 'ao-vk-live.install.json')) -cne $receiptHash){throw 'Backup verification failed.'}
        foreach($rel in $assets.Keys){
            if($beforeAssets[$rel]){
                $saved=Join-Path $backup $rel
                New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($saved)) | Out-Null
                Copy-Item -LiteralPath (Join-Path $clientPath $rel) -Destination $saved
                if((Get-AoSetupHash $saved) -cne $beforeAssets[$rel]){throw 'Surface backup verification failed.'}
            }
            $pairs.Add(@{target=(Join-Path $clientPath $rel);source=(Join-Path $work ('candidate\'+$rel));before=$beforeAssets[$rel];after=$assets[$rel]})
            $receipt.assets | Add-Member -MemberType NoteProperty -Name $rel -Value $assets[$rel] -Force
        }
        $sourceRenderer=$candidate;$newHash=$candidateHash
        $receipt.addon.'ao-vk.dll'=$newHash
        $sourceReceipt=Join-Path $backup 'updated-receipt.json'
        [IO.File]::WriteAllText($sourceReceipt,($receipt|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)))
    }
    $pairs.Add(@{target=$renderer;source=$sourceRenderer;before=$oldHash;after=$newHash})
    $pairs.Add(@{target=$receiptPath;source=$sourceReceipt;before=$receiptHash;after=(Get-AoSetupHash $sourceReceipt)})
    $entries=New-Object 'Collections.Generic.List[object]'
    foreach($pair in $pairs){
        $stage=''
        if($pair.source){
            New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($pair.target)) | Out-Null
            Assert-AoSetupPath $pair.target
            $stage=Join-Path ([IO.Path]::GetDirectoryName($pair.target)) ('.'+[IO.Path]::GetFileName($pair.target)+'.installing-'+[guid]::NewGuid().ToString('N'));$stages+=$stage
            Copy-Item -LiteralPath $pair.source -Destination $stage
            if((Get-AoSetupHash $stage) -cne $pair.after){throw 'Staged copy verification failed.'}
        }
        $entries.Add(@{target=$pair.target;stage=$stage;before=$pair.before;after=$pair.after})
    }
    Assert-AoSetupFolderIdle $clientPath
    Invoke-AoSetupTransaction $clientPath '' ($entries.ToArray())
    foreach($pair in $pairs){if([string](Get-AoSetupHash $pair.target) -cne [string]$pair.after){throw 'Installed file verification failed.'}}
    foreach($name in $protected.Keys){if((Get-AoSetupHash (Join-Path $clientPath $name)) -cne $protected[$name]){throw "Protected file changed: $name"}}
    $result=[ordered]@{result='passed';revision='48';client=$clientPath;rendererSha256=$newHash;receiptSha256=(Get-AoSetupHash $receiptPath);previousSha256=$oldHash;previousReceiptSha256=$receiptHash;assets=$assets;previousAssets=$beforeAssets;backup=$backup;reverted=[bool]$Revert;settingsPreserved=$true;textureCachePreserved=$true;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
    $output=if($Revert){$recordPath.Replace('.json','-REVERT.json')}else{$recordPath}
    $result|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $output -Encoding UTF8
    Write-Output $(if($Revert){'Previous renderer and surface files restored; settings and textures preserved.'}else{'Rigid rock clusters and Full surface pack installed; settings and prepared textures preserved.'})
}finally{
    if(!(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json'))){foreach($stage in $stages){if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -ErrorAction SilentlyContinue}}}
    if($lock){$lock.ReleaseMutex();$lock.Dispose()}
}
