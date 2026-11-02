param(
    [ValidateSet('Install', 'Update', 'Uninstall')][string]$Action = 'Install',
    [string]$ClientDir = '',
    [string]$PackageDir = $PSScriptRoot,
    [string]$PreviewPath = '',
    [switch]$ListCandidates
)
$ErrorActionPreference = 'Stop'
# Prefer this PowerShell host's built-in modules when launched from another host.
$env:PSModulePath = (Join-Path $PSHOME 'Modules') + ';' + $env:PSModulePath

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

function Find-AoFolders {
    $candidates = @($ClientDir)
    foreach ($drive in @(Get-PSDrive -PSProvider FileSystem)) {
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
    $candidates | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'AnarchyOnline.exe')) } |
        ForEach-Object { (Resolve-Path -LiteralPath $_).Path } | Select-Object -Unique
}
$folders = @(Find-AoFolders)
if ($ListCandidates) { $folders; return }
$fullHdSetup = $false
$memoryOptionAvailable = $false
$setupManifest = Join-Path $PackageDir 'manifest.json'
if (Test-Path -LiteralPath $setupManifest) {
    $setupInfo = Get-Content -LiteralPath $setupManifest -Raw | ConvertFrom-Json
    $fullHdSetup = [bool]$setupInfo.materialDownload
    $memoryOptionAvailable = [bool]$setupInfo.largeAddressAware -and [Environment]::Is64BitOperatingSystem
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object Windows.Forms.Form
$form.Text = 'AO-VK Live Setup'
if ($setupInfo.setupRevision) { $form.Text += ' - Setup ' + [string]$setupInfo.setupRevision }
$licenseSpace = if ($fullHdSetup) { 60 } else { 0 }
$form.ClientSize = New-Object Drawing.Size(680, (400 + $licenseSpace))
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
    $dialog = New-Object Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Choose the classic Anarchy Online installation folder.'
    $dialog.ShowNewFolderButton = $false
    if (Test-Path -LiteralPath $path.Text) { $dialog.SelectedPath = $path.Text }
    if ($dialog.ShowDialog($form) -eq 'OK') { $path.Text = $dialog.SelectedPath }
    $dialog.Dispose()
})
$null = Add-Text 'Operation' 26 176 110 25
$operation = New-Object Windows.Forms.ComboBox
$operation.SetBounds(144, 173, 190, 30)
$operation.DropDownStyle = 'DropDownList'
$operation.Items.AddRange(@('Install', 'Update', 'Uninstall'))
$operation.SelectedItem = $Action
$form.Controls.Add($operation)
$memory = New-Object Windows.Forms.CheckBox
$memory.Text = 'Allow up to 4 GB memory (recommended)'
$memory.SetBounds(26, 211, 626, 28)
$memory.Checked = $memoryOptionAvailable
$memory.Enabled = $memoryOptionAvailable -and $Action -ne 'Uninstall'
$form.Controls.Add($memory)
$tip = New-Object Windows.Forms.ToolTip
$tip.SetToolTip($memory, 'For 64-bit Windows. Recognized executables are backed up before changes. Existing AOSharp/AOPP patches are preserved. Uninstall restores only changes made by this setup.')
$artNotice = $null
if ($fullHdSetup) {
    $artNotice = Add-Text "Free to use with any compatible launcher.`r`nRedistribution of the HD artwork is not permitted without authorization.`r`nSee LICENSE-TEXTURES.md; existing third-party permissions remain in force." 26 243 626 58
    $artNotice.Visible = $Action -ne 'Uninstall'
}
$null = Add-Text "Close AO and its launcher first. Settings are preserved; 4 GB changes are backed up.`r`nOnly the supported classic live build is accepted; New Engine is unsupported." 26 (243 + $licenseSpace) 626 46
$status = Add-Text '' 26 (296 + $licenseSpace) 440 62
$status.ForeColor = [Drawing.Color]::DarkSlateGray
$downloadProgress = New-Object Windows.Forms.ProgressBar
$downloadProgress.SetBounds(26, (384 + $licenseSpace), 440, 10)
$downloadProgress.Visible = $false
$form.Controls.Add($downloadProgress)
$script:setupBusy = $false
$form.Add_FormClosing({ if ($script:setupBusy) { $_.Cancel = $true } })
$apply = New-Object Windows.Forms.Button
$apply.Text = $Action
$apply.SetBounds(482, (336 + $licenseSpace), 170, 38)
$form.Controls.Add($apply)
$operation.Add_SelectedIndexChanged({
    $apply.Text = [string]$operation.SelectedItem
    $memory.Enabled = $memoryOptionAvailable -and $operation.SelectedItem -ne 'Uninstall'
    if ($artNotice) { $artNotice.Visible = $operation.SelectedItem -ne 'Uninstall' }
})
$apply.Add_Click({
    $apply.Enabled = $false
    $browse.Enabled = $false
    $path.Enabled = $false
    $operation.Enabled = $false
    $memory.Enabled = $false
    $script:setupBusy = $true
    try {
        if (!$path.Text -or !(Test-Path -LiteralPath (Join-Path $path.Text 'AnarchyOnline.exe'))) {
            throw 'Choose your AO folder containing AnarchyOnline.exe.'
        }
        $status.Text = 'Checking files...'
        $form.Refresh()
        $arguments = @{ ClientDir = $path.Text; PackageDir = $PackageDir }
        if (!$memory.Checked) { $arguments.DisableLargeAddressAware = $true }
        $arguments.ProgressCallback = {
            # Negative command arguments can arrive as strings in Windows
            # PowerShell. Bind numerically before comparing the marquee marker.
            param([string]$message, [int]$percent = -1)
            $status.Text = $message
            $downloadProgress.Visible = $true
            if ($percent -ge 0) { $downloadProgress.Style = 'Continuous'; $downloadProgress.Value = [Math]::Min(100, $percent) }
            else { $downloadProgress.Style = 'Marquee' }
            $form.Refresh()
            [Windows.Forms.Application]::DoEvents()
        }
        if ($operation.SelectedItem -eq 'Update') { $arguments.Upgrade = $true }
        if ($operation.SelectedItem -eq 'Uninstall') { $arguments.Uninstall = $true }
        $setupOutput = @(& (Join-Path $PSScriptRoot 'install-live.ps1') @arguments)
        $setupResult = $setupOutput | Where-Object { $_.AoVkSetupResult } | Select-Object -Last 1
        if ($operation.SelectedItem -eq 'Uninstall') {
            $message = 'AO-VK Live was removed. Your settings were preserved. Installer-owned 4 GB changes were restored where files still match.'
        } else {
            $message = "Ready. Start AO normally and select Primary Display Driver.`r`nIn-game options: Ctrl+Shift+F12. Grass is enabled by default."
        }
        if ($setupResult.memoryStatus) { $message += "`r`n`r`n" + $setupResult.memoryStatus }
        if ($setupResult.memoryWarnings) { $message += "`r`n`r`n" + ($setupResult.memoryWarnings -join "`r`n") }
        $status.Text = 'Finished.'
        $null = [Windows.Forms.MessageBox]::Show($form, $message, 'AO-VK Live', 'OK', 'Information')
    } catch {
        $status.Text = 'Setup did not finish. See the error message.'
        $message = $_.Exception.Message
        if ($_.Exception -is [UnauthorizedAccessException] -or $message -match 'denied|unauthorized') {
            $message += "`r`n`r`nWindows blocked access. Close this window, then right-click the setup .cmd file and choose Run as administrator."
        }
        $null = [Windows.Forms.MessageBox]::Show($form, $message, 'AO-VK Live setup', 'OK', 'Error')
    } finally {
        $apply.Enabled = $true
        $browse.Enabled = $true
        $path.Enabled = $true
        $operation.Enabled = $true
        $memory.Enabled = $memoryOptionAvailable -and $operation.SelectedItem -ne 'Uninstall'
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
