# Built-in Windows/.NET transport. The setup's embedded manifest pins every byte.
function Write-AoMaterialProgress([string]$Message, [int]$Percent = -1, [scriptblock]$ProgressCallback) {
    Write-Progress -Activity 'AO-VK Full HD Setup' -Status $Message -PercentComplete $Percent
    if ($ProgressCallback) { & $ProgressCallback $Message $Percent }
}

function Assert-AoMaterialRelease($Manifest, $Part) {
    $sourceVersion = [string]$Manifest.version
    if ($Manifest.materialSourceVersion) {
        if ($Manifest.version -cnotin @('0.6.0-preview.22','0.6.0-preview.23') -or $Manifest.materialSourceVersion -cne '0.6.0-preview.21') {
            throw 'Invalid frozen material source release.'
        }
        $sourceVersion = [string]$Manifest.materialSourceVersion
    }
    if ($Manifest.version) {
        $prefix = '^ao-vk-live-' + [regex]::Escape($sourceVersion) + '-windows-x86-full-materials-[0-9]+\.zip$'
        if ($Part.name -cnotmatch $prefix) { throw 'Material archive belongs to a different release. Nothing was installed.' }
    }
    if (!$Manifest.materialDownload) { return }
    $version = $sourceVersion
    $expected = "https://github.com/Vitecval/ao-vk-live/releases/download/v$version/"
    if ($version -cne '0.6.0-preview.21' -or $Manifest.materialDownload.repository -cne 'Vitecval/ao-vk-live' -or
        $Manifest.materialDownload.tag -cne "v$version" -or $Manifest.materialDownload.baseUrl -cne $expected -or
        [long]$Part.bytes -le 0 -or [long]$Part.unpackedBytes -le 0) {
        throw 'Invalid pinned Preview 21 download manifest. Extract a fresh copy of Full HD Setup.'
    }
}

function Open-AoMaterialResponse([string]$Url, [long]$Offset) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($Url)
    $request.UserAgent = 'AO-VK-Live-Preview21-Setup'
    $request.Timeout = 30000; $request.ReadWriteTimeout = 30000
    if ($Offset -gt 0) { $request.AddRange($Offset) }
    $response = $request.GetResponse()
    if ($response.ResponseUri.Scheme -cne 'https') { $response.Dispose(); throw 'Refusing an insecure material download redirect.' }
    return $response
}

function Get-AoMaterialFreeBytes([string]$Path) {
    if (!('AoSetupDiskSpace' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AoSetupDiskSpace {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern bool GetDiskFreeSpaceEx(string path, out ulong available, out ulong total, out ulong free);
}
'@
    }
    [uint64]$available=0;[uint64]$total=0;[uint64]$free=0
    if (![AoSetupDiskSpace]::GetDiskFreeSpaceEx(([IO.Path]::GetFullPath($Path).TrimEnd('\')+'\'),[ref]$available,[ref]$total,[ref]$free)) {
        throw "Cannot check free space for $Path. Check that the drive or network share is available."
    }
    return [long][Math]::Min($available,[uint64][long]::MaxValue)
}

function Assert-AoMaterialSpace([string]$Package, [string]$Client, [long]$PackageBytes, [long]$ClientBytes) {
    $packageRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Package))
    $clientRoot = if ($Client) { [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Client)) } else { '' }
    $reserve = $PackageBytes + 128MB
    if ($Client -and $packageRoot -ieq $clientRoot) { $reserve += $ClientBytes + 128MB }
    if ((Get-AoMaterialFreeBytes $Package) -lt $reserve) {
        throw ('Not enough disk space on {0}. Full HD needs about {1:N2} GiB free for downloads and safe staging. Free space or move Setup to another drive, then retry. Your installation is unchanged.' -f $packageRoot, ($reserve / 1GB))
    }
    if ($Client -and $packageRoot -ine $clientRoot -and (Get-AoMaterialFreeBytes $Client) -lt ($ClientBytes + 128MB)) {
        throw ('Not enough disk space on {0}. Allow about {1:N2} GiB for the transactional installation. Your installation is unchanged.' -f $clientRoot, (($ClientBytes + 128MB) / 1GB))
    }
}

function Test-AoMaterialArchive([string]$Path, $Part) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    if ($Part.bytes -and (Get-Item -LiteralPath $Path).Length -ne [long]$Part.bytes) { return $false }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ieq $Part.sha256
}

function Receive-AoMaterialArchive([string]$Cache, $Manifest, $Part, [string]$Label, [scriptblock]$ProgressCallback) {
    Assert-AoMaterialRelease $Manifest $Part
    if (!$Manifest.materialDownload) { throw "Missing offline archive $($Part.name). Use the matching Full HD Setup for automatic downloads." }
    $url = [string]$Manifest.materialDownload.baseUrl + [string]$Part.name
    $final = Join-Path $Cache $Part.name
    $partial = $final + '.partial'; $metadata = $partial + '.json'
    foreach ($path in @($final, $partial, $metadata)) {
        if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Unsafe download cache link; extract Setup to a new folder.' }
    }
    $pin = "$url|$($Part.sha256)|$($Part.bytes)"
    if (!(Test-Path -LiteralPath $metadata) -or (Get-Content -LiteralPath $metadata -Raw) -cne $pin) {
        if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
        [IO.File]::WriteAllText($metadata, $pin)
    }
    $lastError = ''
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $response = $inputStream = $output = $null
        try {
            $offset = if (Test-Path -LiteralPath $partial) { [long](Get-Item -LiteralPath $partial).Length } else { [long]0 }
            if ($offset -ge [long]$Part.bytes) {
                if ($offset -eq [long]$Part.bytes -and (Test-AoMaterialArchive $partial $Part)) {
                    Move-Item -LiteralPath $partial -Destination $final -Force
                    return $final
                }
                Remove-Item -LiteralPath $partial -Force; $offset = [long]0
            }
            Write-Host ("{0}: {1} ({2:N1} MiB), attempt {3}/3, resuming at {4:N1} MiB" -f $Label, $Part.name, ($Part.bytes / 1MB), $attempt, ($offset / 1MB))
            Write-AoMaterialProgress "$Label - connecting to GitHub..." -1 $ProgressCallback
            $response = Open-AoMaterialResponse $url $offset
            if ($offset -gt 0 -and [int]$response.StatusCode -eq 206) {
                $range = [string]$response.Headers['Content-Range']
                if ($range -notmatch '^bytes ([0-9]+)-([0-9]+)/([0-9]+)$' -or [long]$Matches[1] -ne $offset -or [long]$Matches[3] -ne [long]$Part.bytes) { throw 'Invalid download resume range.' }
            } elseif ([int]$response.StatusCode -eq 200) {
                if ($offset -gt 0) { Write-Host 'Server restarted the download; keeping the same pinned release.' }
                $offset = [long]0
            } else { throw "Unexpected download response $([int]$response.StatusCode)." }
            $output = [IO.File]::Open($partial, $(if ($offset) { [IO.FileMode]::Append } else { [IO.FileMode]::Create }), [IO.FileAccess]::Write, [IO.FileShare]::None)
            $inputStream = $response.GetResponseStream(); $buffer = New-Object byte[] 262144
            $received = $offset; $timer = [Diagnostics.Stopwatch]::StartNew(); $last = -1000
            while (($count = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $received += $count
                if ($received -gt [long]$Part.bytes) { throw 'Download exceeded its pinned archive size.' }
                $output.Write($buffer, 0, $count)
                if ($timer.ElapsedMilliseconds - $last -ge 250) {
                    $percent = [int](100.0 * $received / [long]$Part.bytes)
                    Write-AoMaterialProgress ('{0}: {1:N1} / {2:N1} MiB ({3}%)' -f $Label, ($received / 1MB), ($Part.bytes / 1MB), $percent) $percent $ProgressCallback
                    $last = $timer.ElapsedMilliseconds
                }
            }
            $output.Dispose(); $output = $null
            if ($received -ne [long]$Part.bytes) { throw 'Download interrupted before the complete archive arrived.' }
            Write-AoMaterialProgress ('{0}: {1:N1} / {1:N1} MiB (100%)' -f $Label, ($received / 1MB)) 100 $ProgressCallback
            Write-AoMaterialProgress "$Label - verifying SHA256..." -1 $ProgressCallback
            if (!(Test-AoMaterialArchive $partial $Part)) {
                Remove-Item -LiteralPath $partial -Force
                throw 'Downloaded archive failed its pinned SHA256 checksum.'
            }
            Move-Item -LiteralPath $partial -Destination $final -Force
            Write-Host "Verified $($Part.name)."
            return $final
        } catch {
            $lastError = $_.Exception.Message
            $webError = $_.Exception
            while ($webError.InnerException -and $webError -isnot [Net.WebException]) { $webError = $webError.InnerException }
            if ($webError -is [Net.WebException] -and $webError.Response) {
                $status = [int]$webError.Response.StatusCode
                $webError.Response.Dispose()
                if ($status -eq 404) { throw 'Preview 21 material download is unavailable (HTTP 404). The release may not be published yet. Retry after publication or place all matching offline packs beside Install.cmd. Your installation is unchanged.' }
                if ($status -eq 416 -and (Test-Path -LiteralPath $partial)) { Remove-Item -LiteralPath $partial -Force }
            }
            Write-Host "Download attempt $attempt failed: $lastError"
            if ($attempt -lt 3) { Write-AoMaterialProgress "$Label - retrying; downloaded data is saved..." -1 $ProgressCallback; Start-Sleep -Seconds 1 }
        } finally {
            if ($output) { $output.Dispose() }
            if ($inputStream) { $inputStream.Dispose() }
            if ($response) { $response.Dispose() }
        }
    }
    throw "Cannot complete the Preview 21 download. Check internet/proxy access and retry Install.cmd; valid packs and resumable data were saved. $lastError Your existing installation is unchanged."
}
