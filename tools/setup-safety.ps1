$setupModules = Join-Path $PSHOME 'Modules'
$env:PSModulePath = (@($setupModules) + @($env:PSModulePath -split ';' | Where-Object { $_ -and $_ -ine $setupModules })) -join ';'
$script:AoSetupCheckedFolders = @{}
# Shared installer filesystem and ownership guards. No game process is started.
function Get-AoSetupDirectory([string]$Path, [string]$Label) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw "$Label is empty. Choose an extracted setup folder and an existing AO folder." }
    $item = Get-Item -LiteralPath $Path -Force
    if (!$item.PSIsContainer -or $item.PSProvider.Name -ne 'FileSystem') { throw "$Label must be a filesystem folder." }
    $full = [IO.Path]::GetFullPath($item.FullName)
    if ($full.Length -gt [IO.Path]::GetPathRoot($full).Length) { $full = $full.TrimEnd('\') }
    Assert-AoSetupPath $full
    return $full
}
function Assert-AoSetupPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'An installer path is empty.' }
    $full=[IO.Path]::GetFullPath($Path)
    $current=$full
    $checkedParents=@()
    while ($current) {
        if ($script:AoSetupCheckedFolders.ContainsKey($current)) { break }
        if ($current -ine $full) { $checkedParents += $current }
        if ([IO.File]::Exists($current) -or [IO.Directory]::Exists($current)) {
            $attributes=[IO.File]::GetAttributes($current)
            if ($attributes -band [IO.FileAttributes]::ReparsePoint) { throw "An installer path is a link or junction: $current. Choose its real folder; nothing was changed." }
            $directory=($attributes -band [IO.FileAttributes]::Directory) -ne 0
            if ($directory) { $script:AoSetupCheckedFolders[$current]=$true }
            if ($current -ine $full -and !$directory) { throw "A required folder is occupied by a file: $current." }
        }
        $parent=[IO.Directory]::GetParent($current)
        $current=if ($parent) { $parent.FullName } else { $null }
    }
    foreach ($checked in $checkedParents) { $script:AoSetupCheckedFolders[$checked]=$true }
}
function Get-AoSetupHash([string]$Path) {
    Assert-AoSetupPath $Path
    if ([IO.Directory]::Exists($Path)) { throw "An installer file path is occupied by a folder: $Path." }
    if (![IO.File]::Exists($Path)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Get-AoMaterialSearchFolders([string]$Package, [string]$Cache) {
    $current = [IO.Path]::GetFullPath($Package)
    $seen = @{}
    for ($i=0; $current -and $i -lt 3; $i++) {
        if (!$seen.ContainsKey($current)) { $seen[$current]=$true; $current }
        $parent = [IO.Directory]::GetParent($current)
        $current = if ($parent) { $parent.FullName } else { $null }
    }
    if ($Cache -and !$seen.ContainsKey($Cache)) { $Cache }
}
function Enter-AoSetupLock([string]$Directory) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $key = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Directory.TrimEnd('\').ToUpperInvariant())))).Replace('-','') }
    finally { $sha.Dispose() }
    $mutex = New-Object Threading.Mutex($false, ('Global\AO-VK-Setup-' + $key))
    $owned = $false
    try {
        try { $owned = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $owned = $true }
        if (!$owned) { throw 'Another AO-VK setup is using this game or shared loader folder. Wait for it to finish, then retry.' }
        return $mutex
    } finally { if (!$owned) { $mutex.Dispose() } }
}
function Write-AoSetupFailure($ErrorRecord) {
    try {
        $folder = Join-Path ([IO.Path]::GetTempPath()) 'AO-VK-Setup-Logs'
        New-Item -ItemType Directory -Path $folder -Force | Out-Null
        $log = Join-Path $folder ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N') + '.log')
        $text = $ErrorRecord.ToString() + "`r`n" + $ErrorRecord.InvocationInfo.PositionMessage + "`r`n" + $ErrorRecord.ScriptStackTrace + "`r`n" + $ErrorRecord.Exception.ToString()
        [IO.File]::WriteAllText($log, $text, (New-Object Text.UTF8Encoding($false)))
        return $log
    } catch { return '' } # Diagnostic logging must never replace the original error.
}

function Test-AoSetupAssetPath([string]$Relative) {
    return $Relative -cmatch '^randy-vk/(mesh-ids\.bin|packs/ao-((modern|cyber)-icc|icc-enhanced-original|cyber-characters|cyber-world)/(source-art/)?[A-Za-z0-9_-]+\.(png|json|md)|living/[A-Za-z0-9_.-]+\.(lrs|lrm|lrg|ini|txt|json))$'
}
function Assert-AoTransaction($Journal, [string]$Client, [scriptblock]$ProgressCallback) {
    if ($Journal.version -ne 1 -or $Journal.client -ine $Client -or $Journal.token -cnotmatch '^[a-f0-9]{32}$' -or !@($Journal.entries).Count) { throw 'Invalid interrupted setup record. Preserve it for diagnosis.' }
    $seen=@{}
    $checked=0
    foreach ($entry in $Journal.entries) {
        if ($ProgressCallback -and $checked % 128 -eq 0) { & $ProgressCallback "Checking recovery file records: $checked / $($Journal.entries.Count)" -1 };$checked++
        $target=[IO.Path]::GetFullPath([string]$entry.target)
        $prefix=$Client.TrimEnd('\')+'\'
        if ($target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
            $relative=$target.Substring($prefix.Length).Replace('\','/')
            $valid=$relative -cin @('ao-vk.dll','ddraw.dll','ao-vk-live.install.json','randy-vk.ini','AnarchyOnline.exe','Anarchy.exe') -or
                (Test-AoSetupAssetPath $relative) -or $relative -cmatch '^\.ao-vk-laa/(client|loader)-Anarchy(Online)?\.exe\.[a-f0-9]{64}\.bak$'
        } else {
            $valid=$Journal.loader -and ([IO.Path]::GetDirectoryName($target) -ieq [string]$Journal.loader) -and
                ([IO.Path]::GetFileName($target) -cin @('ddraw.dll','AnarchyOnline.exe'))
        }
        if (!$valid -or $seen.ContainsKey($target) -or $entry.backup -cne ($target+'.upgrade-'+$Journal.token) -or
            ($entry.before -and $entry.before -cnotmatch '^[a-f0-9]{64}$') -or ($entry.after -and $entry.after -cnotmatch '^[a-f0-9]{64}$')) { throw 'Unsafe interrupted setup record; no recovery files were changed.' }
        if ($entry.stage) {
            $leaf=[IO.Path]::GetFileName($target)
            $stageLeaf=[IO.Path]::GetFileName([string]$entry.stage)
            if ([IO.Path]::GetDirectoryName([string]$entry.stage) -ine [IO.Path]::GetDirectoryName($target) -or
                ($stageLeaf -cnotmatch ('^\.'+[regex]::Escape($leaf)+'\.installing-[a-f0-9]{32}$') -and
                 $stageLeaf -cnotmatch ('^'+[regex]::Escape($leaf)+'\.restoring-[a-f0-9]{32}$')) -or !$entry.after) { throw 'Unsafe setup staging record.' }
            Assert-AoSetupPath $entry.stage
        } elseif ($entry.after) { throw 'Missing setup staging record.' }
        Assert-AoSetupPath $target
        Assert-AoSetupPath $entry.backup
        $seen[$target]=$true
    }
}
function Save-AoTransaction([string]$Path, $Journal) {
    $temp=$Path+'.writing-'+$Journal.token
    try {
        [IO.File]::WriteAllText($temp,($Journal|ConvertTo-Json -Depth 6),(New-Object Text.UTF8Encoding($false)))
        if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temp,$Path,[System.Management.Automation.Language.NullString]::Value) }
        else { Move-Item -LiteralPath $temp -Destination $Path }
    } finally { if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp } }
}
function Repair-AoTransaction($Journal, [string]$Path, [scriptblock]$ProgressCallback) {
    $errors=New-Object 'Collections.Generic.List[string]'
    $entries=@($Journal.entries)
    for ($i=$entries.Count-1;$i -ge 0;$i--) {
        $entry=$entries[$i]
        if ($ProgressCallback -and $i % 128 -eq 0) { $label=if ($Journal.committed) { 'Finishing setup safely' } else { 'Restoring previous installation' }; & $ProgressCallback ($label+": $($entries.Count-$i) / $($entries.Count)") -1 }
        try {
            $actual=Get-AoSetupHash $entry.target
            $backup=Get-AoSetupHash $entry.backup
            if ($Journal.committed) {
                if ([string]$actual -cne [string]$entry.after) { throw "Completed setup file changed: $($entry.target)" }
                if ($backup) {
                    if ($backup -cne $entry.before) { throw "Backup changed: $($entry.backup)" }
                    Remove-Item -LiteralPath $entry.backup
                }
            } elseif ($backup) {
                if ($backup -cne $entry.before) { throw "Backup changed: $($entry.backup)" }
                if ($actual) {
                    if ($actual -cne $entry.after) { throw "File changed during interrupted setup: $($entry.target)" }
                    Remove-Item -LiteralPath $entry.target
                }
                Move-Item -LiteralPath $entry.backup -Destination $entry.target
            } elseif ([string]$actual -cne [string]$entry.before) {
                if (!$entry.before -and $actual -ceq $entry.after) { Remove-Item -LiteralPath $entry.target }
                else { throw "Original file or backup missing/changed: $($entry.target)" }
            }
            if ($entry.stage -and (Test-Path -LiteralPath $entry.stage)) {
                if ((Get-AoSetupHash $entry.stage) -cne $entry.after) { throw "Staged file changed: $($entry.stage)" }
                Remove-Item -LiteralPath $entry.stage
            }
        } catch { $errors.Add($_.Exception.Message) }
    }
    if ($errors.Count) { throw ("Setup recovery files were preserved at $Path. " + ($errors -join '; ')) }
    Remove-Item -LiteralPath $Path
}
function Recover-AoSetupTransaction([string]$Client, [scriptblock]$ProgressCallback) {
    $path=Join-Path $Client '.ao-vk-setup-transaction.json'
    Assert-AoSetupPath $path
    if (!(Test-Path -LiteralPath $path)) { return }
    $journal=Read-AoSetupJson $path 134217728
    Assert-AoTransaction $journal $Client $ProgressCallback
    $loaderLock=$null
    try {
        if ($journal.loader -and $journal.loader -ine $Client) { $loaderLock=Enter-AoSetupLock $journal.loader }
        Repair-AoTransaction $journal $path $ProgressCallback
    } finally { if ($loaderLock) { $loaderLock.ReleaseMutex();$loaderLock.Dispose() } }
}
function Invoke-AoSetupTransaction([string]$Client,[string]$Loader,$Entries,[scriptblock]$ProgressCallback) {
    $path=Join-Path $Client '.ao-vk-setup-transaction.json'
    $journal=[pscustomobject]@{version=1;client=$Client;loader=$Loader;token=[guid]::NewGuid().ToString('N');committed=$false;entries=@($Entries)}
    foreach ($entry in $journal.entries) { $entry.backup=$entry.target+'.upgrade-'+$journal.token; $entry.before=([string]$entry.before).ToLowerInvariant(); $entry.after=([string]$entry.after).ToLowerInvariant() }
    $script:AoSetupCheckedFolders=@{}
    Assert-AoTransaction $journal $Client $ProgressCallback
    if (Test-Path -LiteralPath $path) { throw 'An interrupted setup needs recovery before continuing.' }
    Save-AoTransaction $path $journal
    try {
        $i=0
        foreach ($entry in $journal.entries) {
            if ($ProgressCallback -and $i % 128 -eq 0) { & $ProgressCallback "Installing verified files: $i / $($journal.entries.Count)" -1 };$i++
            if ([string](Get-AoSetupHash $entry.target) -cne [string]$entry.before) { throw 'An installation file changed after preflight; the previous installation was preserved.' }
            if ($entry.before) { Move-Item -LiteralPath $entry.target -Destination $entry.backup }
            if ($entry.stage) { Move-Item -LiteralPath $entry.stage -Destination $entry.target }
        }
        $journal.committed=$true
        Save-AoTransaction $path $journal
    } catch {
        $original=$_
        $journal.committed=$false
        try { Repair-AoTransaction $journal $path $ProgressCallback } catch { throw ($original.Exception.Message + "`r`n" + $_.Exception.Message) }
        throw $original
    }
    try { Repair-AoTransaction $journal $path $ProgressCallback } catch { Write-Warning ("Setup finished. Temporary backup cleanup will be retried next time. " + $_.Exception.Message) }
}

function Assert-AoSetupManifest($Manifest) {
    if (!$Manifest -or !$Manifest.client -or !$Manifest.addon -or
        ($Manifest.product -and $Manifest.product -cne 'AO-VK Live')) { throw 'Incomplete or unrecognized setup manifest. Extract a fresh setup ZIP.' }
    foreach ($name in @('ao-vk.dll','ddraw.dll')) {
        if ([string]$Manifest.addon.$name -cnotmatch '^[a-fA-F0-9]{64}$') { throw "Invalid setup file hash: $name." }
    }
    $count=0
    foreach ($item in $Manifest.client.PSObject.Properties) {
        $count++
        if ($item.Name -cnotin @('randy31.dll','DisplaySystem.dll','Gamecode.dll','GUI.dll','N3.dll','FXS.dll') -or
            [string]$item.Value -cnotmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid classic client validation manifest.' }
    }
    if (!$count) { throw 'The setup manifest has no classic client validation files.' }
}

function Read-AoSetupJson([string]$Path,[long]$MaximumBytes=33554432) {
    Assert-AoSetupPath $Path
    $file=Get-Item -LiteralPath $Path -Force
    if ($file.PSIsContainer -or !$file.Length -or $file.Length -gt $MaximumBytes) { throw "Invalid or oversized setup metadata: $Path. Extract a fresh setup ZIP; existing game files were preserved." }
    return ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($Path,[Text.Encoding]::UTF8))
}
