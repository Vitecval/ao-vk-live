param([string]$ClientDir = '', [scriptblock]$ProgressCallback)
$ErrorActionPreference = 'Stop'
if (!$ClientDir) {
    & (Join-Path $PSScriptRoot 'setup-live.ps1') -Action 'Reset settings'
    return
}
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $host32=Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    & $host32 -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File $PSCommandPath -ClientDir $ClientDir
    if ($LASTEXITCODE) { throw "Settings reset failed (exit $LASTEXITCODE)." }
    return
}
. (Join-Path $PSScriptRoot 'setup-safety.ps1')
$client = Get-AoSetupDirectory $ClientDir 'AO installation folder'
if (!(Test-Path -LiteralPath (Join-Path $client 'AnarchyOnline.exe') -PathType Leaf)) {
    throw 'Choose your AO folder containing AnarchyOnline.exe.'
}
$lock = Enter-AoSetupLock $client
try {
    if (Test-Path -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json')) {
        throw 'Finish or recover the interrupted install before resetting settings.'
    }
    foreach ($name in @('AnarchyOnline','Anarchy','AnarchyPatcher','randy_harness')) {
        foreach ($process in @(Get-Process -Name $name -ErrorAction SilentlyContinue)) {
            if ($process.HasExited) { continue }
            try {
                $processPath=$process.Path
                $modules=@($process.Modules | ForEach-Object { $_.FileName })
            } catch { if ($process.HasExited) { continue }; throw 'Close AO and its launcher first; a running client could not be inspected.' }
            $usesClient=!$processPath -or $processPath.StartsWith($client+'\',[StringComparison]::OrdinalIgnoreCase)
            foreach ($module in $modules) {
                if ($module -and $module.StartsWith($client+'\',[StringComparison]::OrdinalIgnoreCase)) { $usesClient=$true }
            }
            if ($usesClient) { throw 'Close this AO client and its launcher before resetting settings.' }
        }
    }
    $ini=Join-Path $client 'randy-vk.ini'
    Assert-AoSetupPath $ini
    if ([IO.Directory]::Exists($ini)) { throw 'The settings path is a folder; it was left unchanged.' }
    $existed=[IO.File]::Exists($ini)
    if ($existed) { Remove-Item -LiteralPath $ini -Force -ErrorAction Stop }
    if ([IO.File]::Exists($ini)) { throw 'Windows could not remove the settings file.' }
    Write-Output 'AO-VK settings reset. Defaults will be created when AO next starts.'
    Write-Output ([pscustomobject]@{AoVkSetupResult=$true;settingsReset=$true;removedSettings=$existed})
} finally { $lock.ReleaseMutex(); $lock.Dispose() }
