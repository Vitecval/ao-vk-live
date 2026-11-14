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
$work=Join-Path $root 'build\preview24\lightning-r47c'
$clientPath=Get-AoSetupDirectory $Client 'AO test installation'
$expected=if($Fixture){Join-Path $work 'installer-fixture-client'}else{'F:\AO_SInstall\Anarchy Online'}
if($clientPath -ine $expected){throw 'This trial only targets its F: test client or the isolated fixture.'}
$recordPath=Join-Path $work $(if($Fixture){'INSTALL-FIXTURE.json'}else{'INSTALL-RESULTS.json'})
$verified=Read-AoSetupJson (Join-Path $work 'VERIFIED.json')
$candidate=Join-Path $work 'candidate\ao-vk.dll'
$candidateHash=Get-AoSetupHash $candidate
if($verified.result -ne 'passed' -or $candidateHash -cne $verified.rendererSha256){throw 'The candidate does not match its completed checks.'}
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
    if($Revert){
        $installed=Read-AoSetupJson $recordPath
        if($installed.client -ine $clientPath -or $installed.result -ne 'passed' -or $oldHash -cne $installed.rendererSha256 -or $receiptHash -cne $installed.receiptSha256){throw 'The installation changed since this trial. No automatic revert was applied.'}
        $backup=[IO.Path]::GetFullPath($installed.backup)
        if(!$backup.StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'The saved backup is outside this trial.'}
        $sourceRenderer=Join-Path $backup 'ao-vk.dll';$sourceReceipt=Join-Path $backup 'ao-vk-live.install.json'
        $newHash=Get-AoSetupHash $sourceRenderer
        if($newHash -cne $installed.previousSha256 -or (Get-AoSetupHash $sourceReceipt) -cne $installed.previousReceiptSha256){throw 'The saved backup changed.'}
    }else{
        if($oldHash -ceq $candidateHash){Write-Output 'The verified local trial is already installed.';return}
        $backup=Join-Path $work ('install-backup-'+[guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $backup | Out-Null
        Copy-Item -LiteralPath $renderer,$receiptPath -Destination $backup
        if((Get-AoSetupHash (Join-Path $backup 'ao-vk.dll')) -cne $oldHash -or (Get-AoSetupHash (Join-Path $backup 'ao-vk-live.install.json')) -cne $receiptHash){throw 'Backup verification failed.'}
        $sourceRenderer=$candidate;$newHash=$candidateHash
        $receipt.addon.'ao-vk.dll'=$newHash
        $sourceReceipt=Join-Path $backup 'updated-receipt.json'
        [IO.File]::WriteAllText($sourceReceipt,($receipt|ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false)))
    }
    $entries=New-Object 'Collections.Generic.List[object]'
    foreach($pair in @(@($renderer,$sourceRenderer,$oldHash),@($receiptPath,$sourceReceipt,$receiptHash))){
        $stage=Join-Path $clientPath ('.'+[IO.Path]::GetFileName($pair[0])+'.installing-'+[guid]::NewGuid().ToString('N'));$stages+=$stage
        Copy-Item -LiteralPath $pair[1] -Destination $stage
        $hash=Get-AoSetupHash $pair[1]
        if((Get-AoSetupHash $stage) -cne $hash){throw 'Staged copy verification failed.'}
        $entries.Add(@{target=$pair[0];stage=$stage;before=$pair[2];after=$hash})
    }
    Assert-AoSetupFolderIdle $clientPath
    Invoke-AoSetupTransaction $clientPath '' ($entries.ToArray())
    if((Get-AoSetupHash $renderer) -cne $newHash -or (Get-AoSetupHash $receiptPath) -cne (Get-AoSetupHash $sourceReceipt)){throw 'Installed copy verification failed.'}
    foreach($name in $protected.Keys){if((Get-AoSetupHash (Join-Path $clientPath $name)) -cne $protected[$name]){throw "Protected file changed: $name"}}
    $result=[ordered]@{result='passed';revision='47c';client=$clientPath;rendererSha256=$newHash;receiptSha256=(Get-AoSetupHash $receiptPath);previousSha256=$oldHash;previousReceiptSha256=$receiptHash;backup=$backup;reverted=[bool]$Revert;settingsPreserved=$true;textureCachePreserved=$true;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
    $output=if($Revert){$recordPath.Replace('.json','-REVERT.json')}else{$recordPath}
    $result|ConvertTo-Json|Set-Content -LiteralPath $output -Encoding UTF8
    Write-Output $(if($Revert){'Previous renderer restored; settings and textures preserved.'}else{'Zoning and storm update installed; settings and prepared textures preserved.'})
}finally{
    if(!(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json'))){foreach($stage in $stages){if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -ErrorAction SilentlyContinue}}}
    if($lock){$lock.ReleaseMutex();$lock.Dispose()}
}
