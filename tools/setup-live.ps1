param(
    [ValidateSet('Install', 'Update', 'Uninstall', 'Reset settings')][string]$Action = 'Install',
    [string]$ClientDir = '',
    [string]$PackageDir = $PSScriptRoot,
    [string]$PreviewPath = '',
    [switch]$ListCandidates
)
$ErrorActionPreference = 'Stop'
# Prefer this PowerShell host's built-in modules when launched from another host.
$setupModules = Join-Path $PSHOME 'Modules'
$env:PSModulePath = (@($setupModules) + @($env:PSModulePath -split ';' | Where-Object { $_ -and $_ -ine $setupModules })) -join ';'

# Windows PowerShell's 64-bit Process.Modules can silently omit WOW64 modules.
# Use the game's bitness before inspecting clients, including direct script use.
if ([IntPtr]::Size -eq 8 -and $PSVersionTable.PSEdition -ne 'Core') {
    $host32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    if (!(Test-Path -LiteralPath $host32)) { throw 'Cannot find 32-bit Windows PowerShell for safe AO process inspection.' }
    $hostArgs = @('-NoLogo', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath)
    foreach ($key in $PSBoundParameters.Keys) {
        $value = $PSBoundParameters[$key]
        if ($value -is [Management.Automation.SwitchParameter]) {
            if ($value.IsPresent) { $hostArgs += "-$key" }
        } else { $hostArgs += @("-$key", [string]$value) }
    }
    & $host32 @hostArgs
    if ($LASTEXITCODE -ne 0) { throw "AO-VK setup failed in 32-bit PowerShell (exit $LASTEXITCODE)." }
    return
}

. (Join-Path $PSScriptRoot 'setup-safety.ps1')

function Find-AoFolders {
    $candidates = @($ClientDir)
    foreach ($drive in @(Get-PSDrive -PSProvider FileSystem)) {
        if (!$drive.Root) { continue }
        try { $info=New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($drive.Root)); if ($info.DriveType -eq 'Network' -or !$info.IsReady) { continue } } catch { continue }
        foreach ($relative in @('Funcom\Anarchy Online', 'Games\Anarchy Online', 'Anarchy Online',
                'Program Files (x86)\Funcom\Anarchy Online', 'Program Files\Funcom\Anarchy Online',
                'SteamLibrary\steamapps\common\Anarchy Online', 'Program Files (x86)\Steam\steamapps\common\Anarchy Online')) {
            $candidates += Join-Path $drive.Root $relative
        }
    }
    foreach ($key in @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
            'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
            'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*')) {
        Get-ItemProperty -Path $key -ErrorAction SilentlyContinue | Where-Object {
            $_.DisplayName -like '*Anarchy Online*' -and $_.InstallLocation
        } | ForEach-Object { $candidates += $_.InstallLocation }
    }
    $found=@()
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if ($candidate.StartsWith('\\') -and $candidate -ine $ClientDir) { continue }
        try {
            if (Test-Path -LiteralPath (Join-Path $candidate 'AnarchyOnline.exe') -PathType Leaf) { $found += (Resolve-Path -LiteralPath $candidate).Path }
        } catch { } # An obsolete registry entry or disconnected drive is not a setup failure.
    }
    $found | Select-Object -Unique
}
$folders = @(Find-AoFolders)
if ($ListCandidates) { $folders; return }
$fullHdSetup = $false
$memoryOptionAvailable = $false
$PackageDir = Get-AoSetupDirectory $PackageDir 'Setup package folder'
$setupManifest = Join-Path $PackageDir 'manifest.json'
$setupReadError=''
if (Test-Path -LiteralPath $setupManifest) {
    try {
        $setupInfo = Read-AoSetupJson $setupManifest
        $fullHdSetup = [bool]$setupInfo.materialDownload
        $memoryOptionAvailable = [bool]$setupInfo.largeAddressAware -and [Environment]::Is64BitOperatingSystem
    } catch {
        $setupReadError='Setup files could not be read. Extract a fresh setup ZIP.'
        $setupReadLog=Write-AoSetupFailure $_
        if ($setupReadLog) { $setupReadError += " Details: $setupReadLog" }
    }
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object Windows.Forms.Form
$form.Text = 'AO-VK Live Setup'
if ($setupInfo.setupRevision) { $form.Text += ' - Setup ' + [string]$setupInfo.setupRevision }
$licenseSpace = 0
$form.ClientSize = New-Object Drawing.Size(680, (520 + $licenseSpace))
$form.StartPosition = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.Font = New-Object Drawing.Font('Segoe UI', 10)

function Add-Text([string]$Text, [int]$X, [int]$Y, [int]$W, [int]$H) {
    $label = New-Object Windows.Forms.Label
    $label.Text = $Text
    $label.SetBounds($X, $Y, $W, $H)
    $form.Controls.Add($label)
    return $label
}
$title = Add-Text 'AO-VK Live' 24 18 620 32
if ($fullHdSetup) { $title.Text = 'AO-VK Live - Full HD Setup' }
$title.Font = New-Object Drawing.Font('Segoe UI', 18, [Drawing.FontStyle]::Bold)
$subtitle = Add-Text 'Vulkan graphics for the classic Anarchy Online live client.' 26 56 620 26
if ($fullHdSetup) { $subtitle.Text = 'HD textures download and verify automatically. One setup, all materials.' }
if ($setupInfo.version -eq '0.6.0-preview.24') {
    if ($setupInfo.edition -eq 'lite') {
        $title.Text = 'AO-VK Live - Lite'
        $subtitle.Text = 'Renderer, loading screens and ambient life. No texture downloads.'
    } else {
        $subtitle.Text = 'All HD textures and prepared cache. Downloads verify automatically.'
    }
}
$null = Add-Text 'AO installation folder (contains Anarchy.exe and AnarchyOnline.exe)' 26 98 620 24
$path = New-Object Windows.Forms.ComboBox
$path.SetBounds(26, 126, 514, 30)
$path.DropDownStyle = 'DropDown'
foreach ($folder in $folders) { $null = $path.Items.Add($folder) }
if ($ClientDir) { $path.Text = $ClientDir } elseif ($folders.Count) { $path.SelectedIndex = 0 }
$form.Controls.Add($path)
$browse = New-Object Windows.Forms.Button
$browse.Text = 'Browse...'
$browse.SetBounds(552, 124, 100, 32)
$form.Controls.Add($browse)
$browse.Add_Click({
    $dialog=$null
    try {
        $dialog = New-Object Windows.Forms.FolderBrowserDialog
        $dialog.Description = 'Choose the classic Anarchy Online installation folder.'
        $dialog.ShowNewFolderButton = $false
        if (![string]::IsNullOrWhiteSpace($path.Text) -and (Test-Path -LiteralPath $path.Text -PathType Container)) { $dialog.SelectedPath = $path.Text }
        if ($dialog.ShowDialog($form) -eq 'OK') { $path.Text = $dialog.SelectedPath }
    } catch {
        $message='Windows could not open the folder picker. Type your AO folder into the field instead.'
        $failureLog=Write-AoSetupFailure $_
        if ($failureLog) { $message += "`r`n`r`nDetails: $failureLog" }
        $null=[Windows.Forms.MessageBox]::Show($form,$message,'AO-VK Live setup','OK','Error')
    } finally { if ($dialog) { $dialog.Dispose() } }
})
$null = Add-Text 'Operation' 26 176 110 25
$operation = New-Object Windows.Forms.ComboBox
$operation.SetBounds(144, 173, 190, 30)
$operation.DropDownStyle = 'DropDownList'
$operation.Items.AddRange(@('Install', 'Update', 'Uninstall', 'Reset settings'))
$operation.SelectedItem = $Action
$form.Controls.Add($operation)
$memory = New-Object Windows.Forms.CheckBox
$memory.Text = 'Allow up to 4 GB memory (recommended)'
$memory.SetBounds(26, 211, 626, 28)
$memory.Checked = $memoryOptionAvailable
$memory.Enabled = $memoryOptionAvailable -and $Action -in @('Install','Update')
$form.Controls.Add($memory)
$tip = New-Object Windows.Forms.ToolTip
$tip.SetToolTip($memory, 'For 64-bit Windows. Recognized executables are backed up before changes. Existing AOSharp/AOPP patches are preserved. Uninstall restores only changes made by this setup.')
$operationNotes = Add-Text '' 26 (243 + $licenseSpace) 626 46
$status = Add-Text '' 26 (296 + $licenseSpace) 626 40
$status.ForeColor = [Drawing.Color]::DarkSlateGray
$activity = Add-Text '' 26 (340 + $licenseSpace) 626 22
$details = New-Object Windows.Forms.TextBox
$details.SetBounds(26, (386 + $licenseSpace), 626, 62)
$details.Multiline = $true
$details.ReadOnly = $true
$details.ScrollBars = 'Vertical'
if ($setupReadError) { $status.Text='Setup files need to be extracted again.';$details.Text=$setupReadError }
$form.Controls.Add($details)
$downloadProgress = New-Object Windows.Forms.ProgressBar
$downloadProgress.SetBounds(26, (366 + $licenseSpace), 626, 12)
$downloadProgress.Visible = $false
$form.Controls.Add($downloadProgress)
$script:setupBusy = $false
$form.Add_FormClosing({ if ($script:setupBusy) { $_.Cancel = $true } })
$apply = New-Object Windows.Forms.Button
$apply.Text = $Action
$apply.SetBounds(482, (466 + $licenseSpace), 170, 38)
$form.Controls.Add($apply)
function Update-AoOperationText {
    $apply.Text = [string]$operation.SelectedItem
    $memory.Enabled = $memoryOptionAvailable -and $operation.SelectedItem -in @('Install','Update')
    if ($operation.SelectedItem -eq 'Reset settings') {
        $operationNotes.Text = "Deletes AO-VK settings only. Defaults return when AO next starts.`r`nThe renderer and artwork stay installed. Close AO and its launcher first."
    } elseif ($operation.SelectedItem -eq 'Uninstall') {
        $operationNotes.Text = "Removes recognized AO-VK files; keeps unrecognized files and your settings.`r`nClose this AO client and its launcher first."
    } else {
        $operationNotes.Text = "Close AO and its launcher first. Settings are preserved; 4 GB changes are backed up.`r`nOnly the supported classic live build is accepted; New Engine is unsupported."
    }
}
$operation.Add_SelectedIndexChanged({ Update-AoOperationText })
Update-AoOperationText
$apply.Add_Click({
    $apply.Enabled = $false
    $browse.Enabled = $false
    $path.Enabled = $false
    $operation.Enabled = $false
    $memory.Enabled = $false
    $script:setupBusy = $true
    $worker = $null
    try {
        if (!$path.Text -or !(Test-Path -LiteralPath (Join-Path $path.Text 'AnarchyOnline.exe'))) {
            throw 'Choose your AO folder containing AnarchyOnline.exe.'
        }
        $status.Text = 'Checking files...'
        $details.Clear()
        $activity.Text = 'Starting setup...'
        $downloadProgress.Visible = $true
        $downloadProgress.Style = 'Marquee'
        $form.Refresh()
        $arguments = @{ ClientDir = $path.Text; PackageDir = $PackageDir }
        if (!$memory.Checked) { $arguments.DisableLargeAddressAware = $true }
        if ($operation.SelectedItem -eq 'Update') { $arguments.Upgrade = $true }
        if ($operation.SelectedItem -eq 'Uninstall') { $arguments.Uninstall = $true }
        $workerScript=Join-Path $PSScriptRoot 'install-live.ps1'
        if ($operation.SelectedItem -eq 'Reset settings') {
            $arguments=@{ClientDir=$path.Text}
            $workerScript=Join-Path $PSScriptRoot 'reset-settings.ps1'
        }
        # A worker runspace prevents a single long hash, download, driver call or
        # converter step from freezing the UI. Only this thread touches controls.
        $queue = New-Object 'Collections.Concurrent.ConcurrentQueue[object]'
        $worker = [PowerShell]::Create()
        $null = $worker.AddScript({
            param($installer, $arguments, $queue)
            $ErrorActionPreference = 'Stop'
            $arguments.ProgressCallback = {
                param([string]$message, [int]$percent = -1, [object]$detail = $null)
                $queue.Enqueue([pscustomobject]@{message=$message;percent=$percent;detail=$detail})
            }
            & $installer @arguments
        }).AddArgument($workerScript).AddArgument($arguments).AddArgument($queue)
        $pending = $worker.BeginInvoke()
        $elapsed = [Diagnostics.Stopwatch]::StartNew()
        $history = New-Object 'Collections.Generic.List[string]'
        $lastMessage = ''
        $nativeDetail = $null
        $nativeDetailAt = 0.0
        do {
            [object]$item = $null
            while ($queue.TryDequeue([ref]$item)) {
                $status.Text = [string]$item.message
                [int]$percent = $item.percent
                if ($percent -ge 0) { $downloadProgress.Style = 'Continuous'; $downloadProgress.Value = [Math]::Min(100, $percent) }
                else { $downloadProgress.Style = 'Marquee' }
                if ($lastMessage -ne $status.Text) {
                    $history.Add($status.Text)
                    if ($history.Count -gt 30) { $history.RemoveAt(0) }
                    $details.Lines = $history.ToArray()
                    $details.SelectionStart = $details.TextLength
                    $details.ScrollToCaret()
                    $lastMessage = $status.Text
                }
                $nativeDetail = $item.detail
                $nativeDetailAt = $elapsed.Elapsed.TotalSeconds
            }
            $activity.Text = 'Elapsed ' + $elapsed.Elapsed.ToString('h\:mm\:ss') + ' | Current step is running'
            if ($nativeDetail) {
                $idle = [int]($nativeDetail.idleSeconds + $elapsed.Elapsed.TotalSeconds - $nativeDetailAt)
                $activity.Text += ' | Last helper activity ' + $idle + 's ago'
            }
            $form.Refresh()
            [Windows.Forms.Application]::DoEvents()
            if (!$pending.IsCompleted) { Start-Sleep -Milliseconds 80 }
        } while (!$pending.IsCompleted -or !$queue.IsEmpty)
        $setupOutput = @($worker.EndInvoke($pending))
        if ($worker.HadErrors) { throw $worker.Streams.Error[0].Exception }
        $setupResult = $setupOutput | Where-Object { $_.AoVkSetupResult } | Select-Object -Last 1
        if ($operation.SelectedItem -eq 'Reset settings') {
            $message = 'AO-VK settings reset. Defaults will be created when AO next starts. The renderer remains installed.'
        } elseif ($operation.SelectedItem -eq 'Uninstall') {
            $message = 'Recognized AO-VK files were removed. Unrecognized files and your settings were preserved. Installer-owned 4 GB changes were restored where files still match.'
        } else {
            $message = "Renderer ready. Start AO normally and select Primary Display Driver.`r`nIn-game options: Ctrl+Shift+F12. Grass is enabled by default."
        }
        if ($setupResult.memoryStatus) { $message += "`r`n`r`n" + $setupResult.memoryStatus }
        if ($setupResult.memoryWarnings) { $message += "`r`n`r`n" + ($setupResult.memoryWarnings -join "`r`n") }
        $setupWarnings=@($worker.Streams.Warning | ForEach-Object { $_.Message })
        if ($setupWarnings.Count) { $message += "`r`n`r`n" + ($setupWarnings -join "`r`n") }
        $status.Text = 'Finished.'
        $activity.Text = 'Completed in ' + $elapsed.Elapsed.ToString('h\:mm\:ss')
        $null = [Windows.Forms.MessageBox]::Show($form, $message, 'AO-VK Live', 'OK', 'Information')
    } catch {
        $status.Text = 'Setup did not finish. See the error message.'
        $message = $_.Exception.Message
        $failureLog = Write-AoSetupFailure $_
        if ($worker) { foreach ($workerError in $worker.Streams.Error) { $workerLog = Write-AoSetupFailure $workerError; if ($workerLog) { $failureLog = $workerLog } } }
        if ($failureLog) { $message += "`r`n`r`nDetails: $failureLog" }
        if ($_.Exception -is [UnauthorizedAccessException] -or $message -match 'denied|unauthorized') {
            $message += "`r`n`r`nWindows blocked access. Close this window, then right-click the setup .cmd file and choose Run as administrator."
        }
        $null = [Windows.Forms.MessageBox]::Show($form, $message, 'AO-VK Live setup', 'OK', 'Error')
    } finally {
        if ($worker) { $worker.Dispose() }
        $apply.Enabled = $true
        $browse.Enabled = $true
        $path.Enabled = $true
        $operation.Enabled = $true
        $memory.Enabled = $memoryOptionAvailable -and $operation.SelectedItem -in @('Install','Update')
        $script:setupBusy = $false
        $downloadProgress.Visible = $false
    }
})
$form.AcceptButton = $apply
if ($PreviewPath) {
    $form.StartPosition = 'Manual'
    $form.Location = New-Object Drawing.Point(-32000, -32000)
    $form.ShowInTaskbar = $false
    $form.Show()
    [Windows.Forms.Application]::DoEvents()
    $bitmap = New-Object Drawing.Bitmap($form.Width, $form.Height)
    $form.DrawToBitmap($bitmap, (New-Object Drawing.Rectangle(0, 0, $form.Width, $form.Height)))
    $bitmap.Save($PreviewPath, [Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
} else { $null = $form.ShowDialog() }
$form.Dispose()
$tip.Dispose()
