"""Exercise the real installer with synthetic PE files; never launches AO."""
from pathlib import Path
import argparse, hashlib, json, os, struct, subprocess, uuid

ROOT = Path(__file__).resolve().parents[1]

def sha(b):
    return hashlib.sha256(b).hexdigest()

def pe(laa=False):
    b = bytearray(512)
    b[:2] = b'MZ'
    struct.pack_into('<I', b, 60, 64)
    b[64:68] = b'PE\0\0'
    struct.pack_into('<HH', b, 68, 0x14c, 1)
    struct.pack_into('<HHH', b, 84, 224, 0x102 | (32 if laa else 0), 0x10b)
    return bytes(b)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = output / ('run-' + uuid.uuid4().hex[:8])
    run.mkdir()
    host = Path(os.environ['WINDIR']) / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    env = {k:v for k,v in os.environ.items() if k.lower() != 'psmodulepath'}
    env['PSModulePath'] = str(host.parent / 'Modules')
    game = pe()
    launcher = bytearray(pe())
    launcher[400:408] = bytes.fromhex('e8022d000084c075')
    launcher = bytes(launcher)
    changed_launcher = bytearray(launcher)
    changed_launcher[400:408] = bytes.fromhex('90909090909090eb')
    changed_launcher = bytes(changed_launcher)
    config = dict(gameSha256=sha(game), launcherBeforeSha256=sha(launcher),
                  launcherAfterSha256=sha(changed_launcher), launcherOffset=400)
    cases = []
    for case in ['normal', 'preexisting', 'disabled', 'loader', 'unknown-game',
                 'unknown-launcher', 'invalid-pe', 'rollback-install', 'rollback-uninstall',
                 'external-change', 'backup-change', 'reapply', 'uninstall-without-helper-config']:
        folder = run / case
        client, package, loader = [folder / s for s in ['client with spaces', 'package', 'AOPP copy']]
        for p in [client, package, loader]:
            p.mkdir(parents=True)
        originals = {}
        for n in ['randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll']:
            b = ('stock fixture ' + n).encode()
            (client / n).write_bytes(b)
            originals[n] = sha(b)
        (client / 'AnarchyOnline.exe').write_bytes(pe(True) if case == 'preexisting' else game)
        (client / 'Anarchy.exe').write_bytes(changed_launcher if case == 'preexisting' else launcher)
        (loader / 'AnarchyOnline.exe').write_bytes(game)
        (client / 'randy-vk.ini').write_bytes(b'preferences preserved')
        addons = {}
        for n in ['ao-vk.dll', 'ddraw.dll']:
            (package / n).write_bytes(('addon ' + n).encode())
            addons[n] = sha((package / n).read_bytes())
        manifest = dict(product='AO-VK Live', build='laa-fixture', client=originals,
                        addon=addons, assets={}, previousAddons=[], largeAddressAware=config)
        (package / 'manifest.json').write_text(json.dumps(manifest))
        if case == 'unknown-game':
            b = bytearray(game);b[450] = 1;(client / 'AnarchyOnline.exe').write_bytes(b)
        if case == 'unknown-launcher':
            b = bytearray(launcher);b[450] = 1;(client / 'Anarchy.exe').write_bytes(b)
        if case == 'invalid-pe':
            (client / 'AnarchyOnline.exe').write_bytes(b'not PE data')
        before = {n: (client / n).read_bytes() for n in ['Anarchy.exe', 'AnarchyOnline.exe', 'randy-vk.ini']}
        script = folder / 'test.ps1'
        ps = r'''
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$env:PSModulePath=(Join-Path $PSHOME 'Modules')+';'+$env:PSModulePath
$installer='INSTALLER'
$client='CLIENT';$package='PACKAGE';$loader='LOADER';$case='CASE'
function Assert($v,$message){if(!$v){throw $message}}
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$global:FailTarget=''
function global:Move-Item {
 param($LiteralPath,$Destination)
 if($Destination -eq $global:FailTarget -and ($LiteralPath -like '*.installing-*' -or $LiteralPath -like '*.restoring-*')){throw 'injected transaction failure'}
 Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
$arguments=@{ClientDir=$client;PackageDir=$package}
if($case -eq 'loader'){$arguments.LoaderDir=$loader}
if($case -eq 'disabled'){$arguments.DisableLargeAddressAware=$true}
if($case -eq 'rollback-install'){$global:FailTarget=Join-Path $client 'AnarchyOnline.exe'}
$rejected=$false
try { & $installer @arguments | Out-Null } catch {
 $rejected=$true
 if($case -notin @('unknown-game','unknown-launcher','invalid-pe','rollback-install')){throw}
}
if($case -in @('unknown-game','unknown-launcher','invalid-pe','rollback-install')) {
 Assert $rejected 'Expected install rejection'
 Assert (!(Test-Path (Join-Path $client 'ao-vk.dll'))) 'Failed install left renderer'
 Assert (!(Test-Path (Join-Path $client 'ao-vk-live.install.json'))) 'Failed install left receipt'
} else {
 Assert (!$rejected) 'Unexpected rejection'
 $gameHash=Hash (Join-Path $client 'AnarchyOnline.exe')
 $launcherHash=Hash (Join-Path $client 'Anarchy.exe')
 $receipt=Get-Content (Join-Path $client 'ao-vk-live.install.json') -Raw|ConvertFrom-Json
 if($case -eq 'disabled'){Assert (@($receipt.largeAddressAware).Count -eq 0) 'Disabled option took ownership'}
 elseif($case -eq 'preexisting'){Assert (@($receipt.largeAddressAware).Count -eq 0) 'Preexisting patch took ownership'}
 else{Assert (@($receipt.largeAddressAware).Count -ge 2) 'Missing executable ownership'}
 if($case -eq 'reapply'){
  $backup=Get-ChildItem (Join-Path $client '.ao-vk-laa') -Filter 'client-AnarchyOnline.exe.*.bak'|Select-Object -First 1
  Copy-Item $backup.FullName (Join-Path $client 'AnarchyOnline.exe') -Force
 }
 $arguments.Upgrade=$true
 & $installer @arguments | Out-Null
 Assert ((Hash (Join-Path $client 'AnarchyOnline.exe')) -eq $gameHash) 'Update/reapply changed patched game'
 Assert ((Hash (Join-Path $client 'Anarchy.exe')) -eq $launcherHash) 'Update changed patched launcher'
 if($case -eq 'external-change'){
  $b=[IO.File]::ReadAllBytes((Join-Path $client 'AnarchyOnline.exe'));$b[450]=3
  [IO.File]::WriteAllBytes((Join-Path $client 'AnarchyOnline.exe'),$b)
 }
 if($case -eq 'backup-change'){
  $backup=Get-ChildItem (Join-Path $client '.ao-vk-laa')|Select-Object -First 1
  [IO.File]::WriteAllText($backup.FullName,'changed by another tool')
 }
 if($case -eq 'uninstall-without-helper-config'){
  $m=Get-Content (Join-Path $package 'manifest.json') -Raw|ConvertFrom-Json
  $m.PSObject.Properties.Remove('largeAddressAware')
  $m|ConvertTo-Json -Depth 12|Set-Content (Join-Path $package 'manifest.json')
 }
 if($case -eq 'rollback-uninstall'){
  $global:FailTarget=Join-Path $client 'AnarchyOnline.exe'
  $caught=$false
  try{& $installer @arguments -Uninstall|Out-Null}catch{$caught=$true}
  Assert $caught 'Expected uninstall transaction failure'
  Assert (Test-Path (Join-Path $client 'ao-vk.dll')) 'Failed uninstall lost renderer'
  Assert ((Hash (Join-Path $client 'AnarchyOnline.exe')) -eq $gameHash) 'Failed uninstall lost game patch'
  Assert ((Hash (Join-Path $client 'Anarchy.exe')) -eq $launcherHash) 'Failed uninstall lost launcher patch'
  Assert (Test-Path (Join-Path $client 'ao-vk-live.install.json')) 'Failed uninstall lost receipt'
  $global:FailTarget=''
 }
 & $installer @arguments -Uninstall|Out-Null
 Assert (!(Test-Path (Join-Path $client 'ao-vk.dll'))) 'Uninstall left renderer'
 Assert (!(Test-Path (Join-Path $client 'ao-vk-live.install.json'))) 'Uninstall left receipt'
 & $installer @arguments -Uninstall|Out-Null
}
'''
        for key, value in [('INSTALLER', ROOT / 'tools/install-live.ps1'), ('CLIENT', client),
                           ('PACKAGE', package), ('LOADER', loader), ('CASE', case)]:
            ps = ps.replace(key, str(value).replace("'", "''"))
        script.write_text(ps, encoding='utf-8-sig')
        proc = subprocess.run([str(host), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script)],
                              capture_output=True, text=True, timeout=90, env=env)
        (folder / 'run.log').write_text(proc.stdout + proc.stderr, encoding='utf-8')
        assert proc.returncode == 0, (case, proc.stdout, proc.stderr)
        assert (client / 'randy-vk.ini').read_bytes() == before['randy-vk.ini']
        for n, h in originals.items():
            assert sha((client / n).read_bytes()) == h, (case, n)
        if case in ['external-change', 'backup-change']:
            assert (client / 'Anarchy.exe').read_bytes() == changed_launcher
            expected = bytearray(pe(True))
            if case == 'external-change':
                expected[450] = 3
            assert (client / 'AnarchyOnline.exe').read_bytes() == expected
            assert list((client / '.ao-vk-laa').glob('*.bak'))
        else:
            assert (client / 'Anarchy.exe').read_bytes() == before['Anarchy.exe'], case
            assert (client / 'AnarchyOnline.exe').read_bytes() == before['AnarchyOnline.exe'], case
        if case == 'loader':
            assert (loader / 'AnarchyOnline.exe').read_bytes() == game
            assert not (loader / 'ddraw.dll').exists()
        assert not list(client.rglob('*.installing-*')) and not list(client.rglob('*.restoring-*'))
        assert not list(client.rglob('*.upgrade-*')) and not list(client.rglob('*.uninstalling-*'))
        cases.append(dict(case=case, result='passed'))
        print('PASS', case, flush=True)
    (output / 'RESULTS.json').write_text(json.dumps(dict(result='passed', cases=cases,
        actual_installer=True, synthetic_executables=True, game_installations_changed=False), indent=2) + '\n')

if __name__ == '__main__':
    main()
