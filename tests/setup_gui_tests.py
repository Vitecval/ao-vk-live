"""Run the shipped WinForms button handlers, using isolated dummy clients.

Only modal message boxes and the final ShowDialog call are replaced by probes.
Installer, progress callback and WinForms controls run unchanged in 32-bit PS.
No AO installation is modified and no process is closed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import uuid
import zipfile
from large_address_aware_tests import pe

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.6.0-preview.21'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tools', type=Path, default=ROOT / 'tools')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = output / ('run-' + uuid.uuid4().hex[:10])
    run.mkdir()
    source = (args.tools / 'setup-live.ps1').read_text(encoding='utf-8-sig')
    callback = re.search(r'\$arguments.ProgressCallback = (\{.*?\n        \})', source, re.S).group(1)
    assert source.count('[Windows.Forms.MessageBox]::Show(') == 2
    head = source.split('$form.AcceptButton = $apply')[0]
    head = head.replace('[Windows.Forms.MessageBox]::Show(', '[AoSetupDialogProbe]::Show(')
    host = Path(os.environ['WINDIR']) / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    env = {k: v for k, v in os.environ.items() if k.lower() != 'psmodulepath'}
    env['PSModulePath'] = str(host.parent / 'Modules')
    results = []
    for edition in ['lite', 'full']:
        folder = run / edition
        package, client = folder / 'package with spaces', folder / 'AO dummy client'
        package.mkdir(parents=True)
        client.mkdir()
        originals = {}
        for name in ['randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll']:
            data = ('stock dummy ' + name).encode()
            (client / name).write_bytes(data)
            originals[name] = sha(data)
        game = pe()
        launcher = bytearray(pe())
        launcher[400:408] = bytes.fromhex('e8022d000084c075')
        patched_launcher = bytearray(launcher)
        patched_launcher[400:408] = bytes.fromhex('90909090909090eb')
        (client / 'AnarchyOnline.exe').write_bytes(game)
        (client / 'Anarchy.exe').write_bytes(launcher)
        (client / 'randy-vk.ini').write_bytes(b'[Renderer]\r\nRVK_TerraBlend=1\r\n')
        personal = client / 'randy-vk/packs/personal/keep.txt'
        personal.parent.mkdir(parents=True)
        personal.write_bytes(b'personal pack retained')
        assets = {}
        # Exceed the exact 250-file preflight boundary which failed in Full HD.
        for index in range(260):
            name = f'randy-vk/packs/ao-cyber-world/1010004_{index}_d.png'
            target = package / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(f'dummy material {index}'.encode())
            assets[name] = sha(target.read_bytes())
        addon = {}
        for name in ['ao-vk.dll', 'ddraw.dll']:
            (package / name).write_bytes(('addon one ' + name).encode())
            addon[name] = sha((package / name).read_bytes())
        manifest = dict(product='AO-VK Live', build='gui-fixture-1', version=VERSION,
                        edition=edition, client=originals, addon=addon, assets=assets,
                        previousAddons=[], installBytes=1024 * 1024)
        manifest['largeAddressAware'] = dict(gameSha256=sha(game), launcherBeforeSha256=sha(launcher),
            launcherAfterSha256=sha(patched_launcher), launcherOffset=400)
        license_bytes=(ROOT/'LICENSE-TEXTURES.md').read_bytes()
        (package/'LICENSE-TEXTURES.md').write_bytes(license_bytes)
        manifest['artworkLicense']=dict(file='LICENSE-TEXTURES.md',sha256=sha(license_bytes))
        manifest['setupRevision']=3
        if edition == 'full':
            parts = []
            names = list(assets)
            for number, members in enumerate([names[:64], names[64:128], names[128:]], 1):
                name = f'ao-vk-live-{VERSION}-windows-x86-full-materials-{number}.zip'
                archive = package / name
                with zipfile.ZipFile(archive, 'w', zipfile.ZIP_STORED) as z:
                    for member in members:
                        z.write(package / member, member)
                    z.writestr('LICENSE-TEXTURES.md',license_bytes)
                parts.append(dict(name=name, bytes=archive.stat().st_size,
                                  sha256=sha(archive.read_bytes()), files=members,
                                  unpackedBytes=sum((package / m).stat().st_size for m in members)+len(license_bytes),
                                  notices={'LICENSE-TEXTURES.md':sha(license_bytes)}))
            manifest['materialParts'] = parts
            manifest['materialDownload'] = dict(repository='Vitecval/ao-vk-live', tag='v' + VERSION,
                                                baseUrl=f'https://github.com/Vitecval/ao-vk-live/releases/download/v{VERSION}/')
            for member in assets:
                (package / member).unlink()
        (package / 'manifest.json').write_text(json.dumps(manifest))
        for name in ['install-live.ps1', 'material-parts.ps1', 'material-download.ps1', 'large-address-aware.ps1']:
            shutil.copyfile(args.tools / name, package / name)
        prelude = '''Add-Type -TypeDefinition @'
public static class AoSetupDialogProbe {
    public static string Error = "";
    public static object Show(object owner, object message, object title, object buttons, object icon) {
        if (icon.ToString() == "Error") Error = message.ToString();
        return null;
    }
}
'@
'''
        # Keep the original param block first, so parameter binding is unchanged.
        start = head.index("$ErrorActionPreference = 'Stop'")
        instrumented = head[:start] + prelude + head[start:]
        tail = r'''
$ProgressPreference = 'SilentlyContinue'
function Assert($condition, $message) { if (!$condition) { throw $message } }
function Hash($p) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash }
$form.StartPosition = 'Manual'
$form.Location = New-Object Drawing.Point(-32000, -32000)
$form.ShowInTaskbar = $false
$form.Show()
$prefs = Hash (Join-Path $ClientDir 'randy-vk.ini')
$stock = @{}
$m = Get-Content (Join-Path $PackageDir 'manifest.json') -Raw | ConvertFrom-Json
foreach ($p in $m.client.PSObject.Properties) { $stock[$p.Name] = Hash (Join-Path $ClientDir $p.Name) }
try {
    if($fullHdSetup){Assert ($artNotice.Visible -and $artNotice.Text.Contains('Redistribution')) 'Full HD artwork notice not visible before installation'}
    $callback = CALLBACK
    foreach ($v in @('-1', [int]-1, '0', 0, '50', 50, '100', 100, '150')) {
        & $callback 'Testing actual GUI progress callback' $v
        if ([int]$v -lt 0) { Assert ($downloadProgress.Style -eq 'Marquee') 'Negative progress did not select marquee' }
        else { Assert ($downloadProgress.Value -eq [Math]::Min(100,[int]$v)) 'Incorrect bounded progress' }
    }
    foreach ($actionName in @('Install', 'Update', 'Uninstall', 'Uninstall')) {
        if ($actionName -eq 'Update') {
            [IO.File]::WriteAllText((Join-Path $PackageDir 'ao-vk.dll'), 'addon two renderer')
            $m.addon.'ao-vk.dll' = (Hash (Join-Path $PackageDir 'ao-vk.dll')).ToLowerInvariant()
            $m.build = 'gui-fixture-2'
            $m | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $PackageDir 'manifest.json')
        }
        [AoSetupDialogProbe]::Error = ''
        $operation.SelectedItem = $actionName
        $apply.PerformClick()
        Assert (![AoSetupDialogProbe]::Error) ($actionName + ': ' + [AoSetupDialogProbe]::Error)
        Assert ($status.Text -eq 'Finished.') ($actionName + ' GUI did not finish')
        if($fullHdSetup){Assert ($artNotice.Visible -eq ($actionName -ne 'Uninstall')) 'Artwork notice visibility is incorrect'}
        Assert ($apply.Enabled -and !$script:setupBusy -and !$downloadProgress.Visible) 'GUI did not reset after operation'
        Assert ((Hash (Join-Path $ClientDir 'randy-vk.ini')) -eq $prefs) 'Preferences changed'
        Assert (Test-Path (Join-Path $ClientDir 'randy-vk/packs/personal/keep.txt')) 'Personal pack removed'
        foreach ($name in $stock.Keys) { Assert ((Hash (Join-Path $ClientDir $name)) -eq $stock[$name]) ('Stock file changed: ' + $name) }
        foreach ($name in $m.assets.PSObject.Properties.Name) {
            $p = Join-Path $ClientDir $name
            if ($actionName -eq 'Uninstall') { Assert (!(Test-Path $p)) 'Owned material remains after uninstall' }
            else { Assert ((Hash $p) -eq $m.assets.$name) 'Installed material hash mismatch' }
        }
        if ($actionName -eq 'Uninstall') {
            Assert (!(Test-Path (Join-Path $ClientDir 'ao-vk.dll'))) 'Renderer remains after uninstall'
            Assert (!(Test-Path (Join-Path $ClientDir 'ddraw.dll'))) 'Loader remains after uninstall'
            Assert (!(Test-Path (Join-Path $ClientDir 'ao-vk-live.install.json'))) 'Receipt remains after uninstall'
        } else { Assert ((Hash (Join-Path $ClientDir 'ao-vk.dll')) -eq $m.addon.'ao-vk.dll') 'Renderer hash mismatch' }
        $gameBytes=[IO.File]::ReadAllBytes((Join-Path $ClientDir 'AnarchyOnline.exe'))
        if($actionName -eq 'Uninstall'){
            Assert ((Hash (Join-Path $ClientDir 'AnarchyOnline.exe')) -eq $m.largeAddressAware.gameSha256) 'Memory patch did not restore game'
            Assert ((Hash (Join-Path $ClientDir 'Anarchy.exe')) -eq $m.largeAddressAware.launcherBeforeSha256) 'Memory patch did not restore launcher'
        }else{
            Assert (($gameBytes[86] -band 32) -eq 32) 'Default GUI memory option did not enable LAA'
            Assert ((Hash (Join-Path $ClientDir 'Anarchy.exe')) -eq $m.largeAddressAware.launcherAfterSha256) 'Launcher compatibility not installed'
        }
        Write-Output ('PASS GUI: ' + $actionName)
    }
    $memory.Checked=$false
    $operation.SelectedItem='Install'
    [AoSetupDialogProbe]::Error='';$apply.PerformClick()
    Assert (![AoSetupDialogProbe]::Error) [AoSetupDialogProbe]::Error
    Assert ((Hash (Join-Path $ClientDir 'AnarchyOnline.exe')) -eq $m.largeAddressAware.gameSha256) 'Unchecked GUI memory option changed game'
    $operation.SelectedItem='Uninstall';$apply.PerformClick()
    Assert (![AoSetupDialogProbe]::Error) [AoSetupDialogProbe]::Error
    Write-Output 'PASS MEMORY GUI: unchecked option preserves executables'
} finally { $form.Dispose() }
'''.replace('CALLBACK', callback)
        script = package / 'setup-gui-probe.ps1'
        script.write_text(instrumented + tail, encoding='utf-8-sig')
        process = subprocess.run([str(host), '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass',
                                  '-File', str(script), '-ClientDir', str(client), '-PackageDir', str(package)],
                                 capture_output=True, text=True, env=env, timeout=240)
        (folder / 'run.log').write_text(process.stdout + process.stderr, encoding='utf-8')
        assert process.returncode == 0, (edition, process.stdout, process.stderr)
        assert process.stdout.count('PASS GUI:') == 4
        results.append(dict(edition=edition, result='passed', assets=260,
                            actual_winforms_button_handlers=True,
                            memory_option_default_on_and_disabled_tested=True,
                            progress_values=['string -1', 'integer -1', 0, 50, 100, 150],
                            actions=['Install', 'Update', 'Uninstall', 'Uninstall again'],
                            offline_companions=3 if edition == 'full' else 0))
    (output / 'RESULTS.json').write_text(json.dumps(dict(result='passed', cases=results,
        host=str(host), game_installations_modified=False, processes_closed=False), indent=2) + '\n')
    print('PASS: actual Lite/Full GUI install/update/uninstall, progress types and three offline packs.')

if __name__ == '__main__':
    main()
