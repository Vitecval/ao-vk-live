param(
    [ValidateSet('Install', 'Update', 'Uninstall')][string]$Action = 'Install',
    [string]$ClientDir = '',
    [string]$PackageDir = $PSScriptRoot,
    [string]$PreviewPath = '',
    [switch]$ListCandidates
)
$ErrorActionPreference = 'Stop'
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

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object Windows.Forms.Form
$form.Text = 'AO-VK Live Setup'
$form.ClientSize = New-Object Drawing.Size(680, 360)
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
$title.Font = New-Object Drawing.Font('Segoe UI', 18, [Drawing.FontStyle]::Bold)
$null = Add-Text 'Vulkan graphics for the classic Anarchy Online live client.' 26 56 620 26
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
$null = Add-Text "Close AO and its launcher first. Game files and preferences are preserved.`r`nOnly the supported classic live build is accepted; New Engine is unsupported." 26 220 626 50
$status = Add-Text '' 26 282 440 55
$status.ForeColor = [Drawing.Color]::DarkSlateGray
$apply = New-Object Windows.Forms.Button
$apply.Text = $Action
$apply.SetBounds(482, 296, 170, 38)
$form.Controls.Add($apply)
$operation.Add_SelectedIndexChanged({ $apply.Text = [string]$operation.SelectedItem })
$apply.Add_Click({
    $apply.Enabled = $false
    $browse.Enabled = $false
    try {
        if (!$path.Text -or !(Test-Path -LiteralPath (Join-Path $path.Text 'AnarchyOnline.exe'))) {
            throw 'Choose your AO folder containing AnarchyOnline.exe.'
        }
        $status.Text = 'Checking files...'
        $form.Refresh()
        $arguments = @{ ClientDir = $path.Text; PackageDir = $PackageDir }
        if ($operation.SelectedItem -eq 'Update') { $arguments.Upgrade = $true }
        if ($operation.SelectedItem -eq 'Uninstall') { $arguments.Uninstall = $true }
        & (Join-Path $PSScriptRoot 'install-live.ps1') @arguments | Out-Null
        if ($operation.SelectedItem -eq 'Uninstall') {
            $message = 'AO-VK Live was removed. Your settings and game files were preserved.'
        } else {
            $message = "Ready. Start AO normally and select Primary Display Driver.`r`nIn-game options: Ctrl+Shift+F12. Grass is enabled by default."
        }
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
