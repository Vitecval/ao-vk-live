param([string]$Client='F:\AO_SInstall\Anarchy Online')
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    & "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Client $Client
    if ($LASTEXITCODE) { throw "Revision 42 installation failed (exit $LASTEXITCODE)." }; return
}
$root=Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\setup-safety.ps1')
$work=Join-Path $root 'build\preview23\atmosphere-r42'
$candidate=Join-Path $work 'candidate\ao-vk.dll'
$verified=Read-AoSetupJson (Join-Path $work 'VERIFIED.json')
$newHash=Get-AoSetupHash $candidate
if ($verified.result -ne 'passed' -or $newHash -ine $verified.rendererSha256) { throw 'Revision 42 validation or renderer hash does not match.' }
$clientPath=Get-AoSetupDirectory $Client 'AO test installation'
if ($clientPath -ine 'F:\AO_SInstall\Anarchy Online') { throw 'This local update targets the F: test installation only.' }
$lock=Enter-AoSetupLock $clientPath
$stages=@()
try {
    if (Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json')) { Assert-AoSetupFolderIdle $clientPath; Recover-AoSetupTransaction $clientPath }
    function Assert-Closed {
        foreach($name in @('AnarchyOnline','Anarchy','AnarchyPatcher','randy_harness')) {
            foreach($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
                if ($process.HasExited) { continue }
                try { $processPath=$process.Path; $modules=@($process.Modules | ForEach-Object { $_.FileName }) }
                catch { if ($process.HasExited) { continue }; throw 'Close AO and its launcher first; a running client could not be inspected.' }
                if (!$processPath) { throw 'Close the starting AO client and launcher before installing.' }
                $usesClient=$processPath.StartsWith($clientPath+'\',[StringComparison]::OrdinalIgnoreCase)
                foreach($module in $modules) {
                    if ($module -and $module.StartsWith($clientPath+'\',[StringComparison]::OrdinalIgnoreCase)) { $usesClient=$true }
                }
                if ($usesClient) { throw 'Close the F: game and launcher before installing.' }
            }
        }
    }
    Assert-Closed
    $renderer=Join-Path $clientPath 'ao-vk.dll'
    $receiptPath=Join-Path $clientPath 'ao-vk-live.install.json'
    $ini=Join-Path $clientPath 'randy-vk.ini'
    $oldHash=Get-AoSetupHash $renderer
    $receiptHash=Get-AoSetupHash $receiptPath
    $iniHash=Get-AoSetupHash $ini
    $receipt=Read-AoSetupJson $receiptPath
    if ($receipt.product -ne 'AO-VK Live' -or $receipt.clientDirectory -ine $clientPath -or
        !$oldHash -or $oldHash -ine $receipt.addon.'ao-vk.dll') { throw 'The existing renderer does not match this installation record; it was left unchanged.' }
    $backup=Join-Path $work ('install-backup-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach($file in @($renderer,$receiptPath,$ini)) { if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $backup } }
    $protected=@{}
    foreach($name in @('Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')) {
        $protected[$name]=Get-AoSetupHash (Join-Path $clientPath $name)
    }
    $encoding=[Text.Encoding]::GetEncoding(28591)
    $text=if ($iniHash) { $encoding.GetString([IO.File]::ReadAllBytes($ini)) } else { '' }
    # Preserve every other user preference. Only the requested default is applied.
    $section=[regex]::Match($text,'(?ims)(^\[Renderer\][^\r\n]*\r?\n)(.*?)(?=^\[|\z)')
    if ($section.Success) {
        $body=$section.Groups[2].Value
        if ($body -match '(?m)^RVK_SkipHugeShadows=') { $body=[regex]::Replace($body,'(?im)^RVK_SkipHugeShadows=[^\r\n]*','RVK_SkipHugeShadows=0') }
        else { if ($body -and !$body.EndsWith("`n")) { $body+="`r`n" }; $body+="RVK_SkipHugeShadows=0`r`n" }
        $text=$text.Substring(0,$section.Index)+$section.Groups[1].Value+$body+$text.Substring($section.Index+$section.Length)
    } else { $text+="`r`n[Renderer]`r`nRVK_SkipHugeShadows=0`r`n" }
    $receipt.addon.'ao-vk.dll'=$newHash
    $entries=New-Object 'Collections.Generic.List[object]'
    foreach($target in @($renderer,$receiptPath,$ini)) {
        $stage=Join-Path $clientPath ('.'+[IO.Path]::GetFileName($target)+'.installing-'+[guid]::NewGuid().ToString('N'))
        $stages+=$stage
        if ($target -eq $renderer) { Copy-Item -LiteralPath $candidate -Destination $stage; $before=$oldHash }
        elseif ($target -eq $receiptPath) { [IO.File]::WriteAllText($stage,($receipt | ConvertTo-Json -Depth 30),(New-Object Text.UTF8Encoding($false))); $before=$receiptHash }
        else { [IO.File]::WriteAllBytes($stage,$encoding.GetBytes($text)); $before=$iniHash }
        $entries.Add(@{target=$target;stage=$stage;before=$before;after=(Get-AoSetupHash $stage)})
    }
    if ((Get-AoSetupHash $stages[0]) -ine $newHash) { throw 'Renderer staging verification failed.' }
    Assert-Closed
    Invoke-AoSetupTransaction $clientPath '' ($entries.ToArray())
    foreach($name in $protected.Keys) {
        if ((Get-AoSetupHash (Join-Path $clientPath $name)) -ine $protected[$name]) { throw "Protected game file changed: $name" }
    }
    $result=[ordered]@{result='passed';revision=42;client=$clientPath;rendererSha256=$newHash;previousSha256=$oldHash;backup=$backup;
        skipHugeShadows=0;otherPreferencesPreserved=$true;artworkReplaced=$false;loadingPhrases=50;gameLaunched=$false;verifiedUtc=[DateTime]::UtcNow.ToString('o')}
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'INSTALL-RESULTS.json') -Encoding UTF8
    Write-Output "Revision 42 installed and verified in $clientPath. Restored ships, full-image zoning text and stable scenery shadows; Skip HUGE shadows off."
} finally {
    if (!(Test-Path -LiteralPath (Join-Path $clientPath '.ao-vk-setup-transaction.json'))) {
        foreach($stage in $stages) { if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -ErrorAction SilentlyContinue } }
    }
    $lock.ReleaseMutex();$lock.Dispose()
}
