# Local-only executable changes. No game executable or backup is distributed.
function Get-AoByteHash([byte[]]$Bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Get-AoPeFlagsOffset([byte[]]$Bytes) {
    if ($Bytes.Length -lt 256 -or $Bytes[0] -ne 77 -or $Bytes[1] -ne 90) { throw 'Invalid game executable: missing DOS header.' }
    $pe = [BitConverter]::ToInt32($Bytes, 60)
    if ($pe -lt 64 -or $pe -gt $Bytes.Length - 248 -or [BitConverter]::ToUInt32($Bytes, $pe) -ne 17744 -or
        [BitConverter]::ToUInt16($Bytes, $pe + 4) -ne 332 -or [BitConverter]::ToUInt16($Bytes, $pe + 24) -ne 267 -or
        ([BitConverter]::ToUInt16($Bytes, $pe + 22) -band 8194) -ne 2) {
        throw 'The 4 GB memory option requires a valid classic 32-bit game executable.'
    }
    return $pe + 22
}
function Get-AoLaaTarget([string]$Scope, [string]$Name, [string]$Client, [string]$Loader) {
    if ($Scope -notin @('client', 'loader') -or $Name -notin @('AnarchyOnline.exe', 'Anarchy.exe') -or
        ($Scope -eq 'loader' -and (!$Loader -or $Name -ne 'AnarchyOnline.exe'))) { throw 'Invalid memory-patch ownership record.' }
    $folder = if ($Scope -eq 'client') { $Client } else { $Loader }
    $target = Join-Path $folder $Name
    if ((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing an executable link for the 4 GB memory option.' }
    return $target
}
function Get-AoLaaBackup([string]$Client, $Record) {
    if ($Record.before -notmatch '^[a-f0-9]{64}$' -or $Record.after -notmatch '^[a-f0-9]{64}$') { throw 'Invalid memory-patch hashes.' }
    $folder = Join-Path $Client '.ao-vk-laa'
    if ((Test-Path -LiteralPath $folder) -and ((Get-Item -LiteralPath $folder -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing a linked memory-patch backup folder.' }
    $backup = Join-Path $folder ($Record.scope + '-' + $Record.name + '.' + $Record.before + '.bak')
    if ((Test-Path -LiteralPath $backup) -and ((Get-Item -LiteralPath $backup -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing a linked memory-patch backup.' }
    return $backup
}
function Get-AoLaaPlan([string]$Client, [string]$Loader, $Config, $Receipt, [bool]$Enable, [bool]$Removing) {
    $records = @(); $changes = @(); $warnings = @(); $status = 'Left unchanged'
    $seen = @{}
    foreach ($record in @($Receipt.largeAddressAware)) {
        if (!$record) { continue }
        $null = Get-AoLaaTarget $record.scope $record.name $Client $Loader
        $null = Get-AoLaaBackup $Client $record
        $identity = $record.scope + '/' + $record.name
        if ($seen.ContainsKey($identity)) { throw 'Duplicate memory-patch ownership record.' }
        $seen[$identity] = $true
        $records += $record
    }
    if ($Removing) {
        foreach ($scope in @('client', 'loader')) {
            $group = @($records | Where-Object { $_.scope -eq $scope })
            if (!$group.Count) { continue }
            $canRestore = $true
            foreach ($record in $group) {
                $target = Get-AoLaaTarget $scope $record.name $Client $Loader
                $backup = Get-AoLaaBackup $Client $record
                if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -notin @($record.before, $record.after)) { $canRestore = $false }
                if (!(Test-Path -LiteralPath $backup) -or (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ine $record.before) { $canRestore = $false }
            }
            if (!$canRestore) {
                $warnings += "The $scope executable or its backup changed after installation. Its memory patch was left unchanged; backups remain in $Client\.ao-vk-laa."
                continue
            }
            foreach ($record in $group) {
                $target = Get-AoLaaTarget $scope $record.name $Client $Loader
                $backup = Get-AoLaaBackup $Client $record
                $changes += @{ target=$target; hash=[string]$record.before; oldHash=[string]$record.after; bytes=[IO.File]::ReadAllBytes($backup); restore=$true; special=$true; executable=$true }
                $changes += @{ target=$backup; hash=[string]$record.before; oldHash=[string]$record.before; special=$true }
            }
        }
        return @{ records=$records; changes=$changes; warnings=$warnings; status='Installer-owned memory changes restored when unchanged' }
    }
    if (!$Enable -or !$Config) { return @{ records=$records; changes=$changes; warnings=$warnings; status=$status } }
    if (![Environment]::Is64BitOperatingSystem) { return @{ records=$records; changes=$changes; warnings=$warnings; status='Requires 64-bit Windows; left unchanged' } }
    if (Test-Path -LiteralPath (Join-Path $Client 'Cheetah.dll')) { throw 'The 4 GB memory option is only for the classic client.' }
    $candidates = @(@{scope='client';name='AnarchyOnline.exe'})
    if ($Loader -and $Loader -ine $Client) { $candidates += @{scope='loader';name='AnarchyOnline.exe'} }
    $pending = @()
    foreach ($candidate in $candidates) {
        $target = Get-AoLaaTarget $candidate.scope $candidate.name $Client $Loader
        $bytes = [IO.File]::ReadAllBytes($target)
        $offset = Get-AoPeFlagsOffset $bytes
        if ($bytes[$offset] -band 32) { continue } # Preserve existing AOSharp/AOPP patches.
        if ((Get-AoByteHash $bytes) -cne [string]$Config.gameSha256) { throw 'Unrecognized game executable for the 4 GB memory option. Disable this option to install graphics without executable changes.' }
        $patched = [byte[]]$bytes.Clone(); $patched[$offset] = $patched[$offset] -bor 32
        $pending += @{ scope=$candidate.scope; name=$candidate.name; original=$bytes; patched=$patched }
        if ($candidate.scope -eq 'client') {
            $launcher = Get-AoLaaTarget 'client' 'Anarchy.exe' $Client $Loader
            $original = [IO.File]::ReadAllBytes($launcher); $hash = Get-AoByteHash $original
            if ($hash -ceq [string]$Config.launcherAfterSha256) { continue }
            if ($hash -cne [string]$Config.launcherBeforeSha256) { throw 'Unrecognized official launcher for the 4 GB memory option. Disable this option to preserve this launcher unchanged.' }
            $offset = [int]$Config.launcherOffset
            if ($offset -lt 0 -or $offset -gt $original.Length - 8) { throw 'Invalid launcher memory-patch descriptor.' }
            $patched = [byte[]]$original.Clone()
            [byte[]]$replacement = 144,144,144,144,144,144,144,235
            [Array]::Copy($replacement, 0, $patched, $offset, $replacement.Length)
            if ((Get-AoByteHash $patched) -cne [string]$Config.launcherAfterSha256) { throw 'Launcher compatibility patch verification failed.' }
            $pending += @{scope='client';name='Anarchy.exe';original=$original;patched=$patched}
        }
    }
    foreach ($item in $pending) {
        $record = [pscustomobject]@{scope=$item.scope;name=$item.name;before=(Get-AoByteHash $item.original);after=(Get-AoByteHash $item.patched)}
        $existing = @($records | Where-Object { $_.scope -eq $record.scope -and $_.name -eq $record.name })
        if ($existing.Count) {
            if ($existing.Count -ne 1 -or $existing[0].before -cne $record.before -or $existing[0].after -cne $record.after) { throw 'Conflicting memory-patch history; original backups were preserved.' }
        } else { $records += $record }
        $target = Get-AoLaaTarget $record.scope $record.name $Client $Loader
        $backup = Get-AoLaaBackup $Client $record
        if ((Test-Path -LiteralPath $backup) -and (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ine $record.before) { throw 'An existing memory-patch backup differs; refusing to overwrite it.' }
        $changes += @{ target=$backup; hash=[string]$record.before; oldHash=[string]$record.before; bytes=$item.original; special=$true }
        $changes += @{ target=$target; hash=[string]$record.after; oldHash=[string]$record.before; bytes=$item.patched; special=$true; executable=$true }
    }
    $status = if ($pending.Count) { '4 GB memory enabled; original executables backed up' } else { '4 GB memory already enabled; existing patches preserved' }
    return @{ records=$records; changes=$changes; warnings=$warnings; status=$status }
}
