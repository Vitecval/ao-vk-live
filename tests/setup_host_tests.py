"""Exercise setup launchers and real installer host forwarding in dummy folders.

Uses the actual process guard, so unrelated live AO clients can stay open.
No game installation is written and no process is terminated.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    fixture = out / ('fixtures-' + uuid.uuid4().hex[:8])
    fixture.mkdir()
    windir = Path(os.environ['WINDIR'])
    hosts = {'desktop64': windir / 'System32/WindowsPowerShell/v1.0/powershell.exe',
             'desktop32': windir / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'}
    if shutil.which('pwsh'):
        hosts['core64'] = Path(shutil.which('pwsh'))
    env = dict(os.environ)
    env['PSModulePath'] = str(windir / 'System32/WindowsPowerShell/v1.0/Modules')
    results = {}
    for name, host in hosts.items():
        folder = fixture / name
        client, package, loader = [folder / p for p in ['AO client with spaces', 'package with spaces', 'shared loader with spaces']]
        for p in [client, package, loader]:
            p.mkdir(parents=True)
        originals = {}
        for filename in ['randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll']:
            p = client / filename
            p.write_text('dummy stock ' + filename)
            originals[filename] = sha(p)
        for p in [client, loader]:
            (p / 'AnarchyOnline.exe').write_text('dummy fixture, never executed')
        settings = client / 'randy-vk.ini'
        settings.write_text('[Renderer]\nRVK_TerraBlend=1\n')
        settings_hash = sha(settings)
        manifest = {'product': 'AO-VK Live', 'build': 'host-fixture-1', 'edition': 'lite',
                    'client': originals, 'addon': {}, 'assets': {}, 'previousAddons': []}

        def payload(version):
            manifest['build'] = 'host-fixture-' + str(version)
            for fn in ['ao-vk.dll', 'ddraw.dll']:
                p = package / fn
                p.write_text(f'dummy addon {version} {fn}')
                manifest['addon'][fn] = sha(p)
            (package / 'manifest.json').write_text(json.dumps(manifest))

        def run(label, script, extra, success=True):
            command = [str(host), '-NoLogo', '-NoProfile', '-STA', '-ExecutionPolicy', 'Bypass', '-File', str(script), *map(str, extra)]
            p = subprocess.run(command, cwd=folder, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
            (out / f'{name}-{label}.log').write_text(p.stdout, encoding='utf-8')
            assert (p.returncode == 0) == success, (name, label, p.returncode, p.stdout)
            return p.stdout

        listed = run('candidates', ROOT / 'tools/setup-live.ps1', ['-ClientDir', client, '-ListCandidates'])
        assert str(client).lower() in listed.lower()
        preview = out / (name + '-setup.png')
        run('preview', ROOT / 'tools/setup-live.ps1', ['-Action', 'Update', '-ClientDir', client, '-PackageDir', package, '-PreviewPath', preview])
        assert preview.is_file() and preview.stat().st_size > 1000
        payload(1)
        common = ['-ClientDir', client, '-PackageDir', package, '-LoaderDir', loader]
        run('install', ROOT / 'tools/install-live.ps1', common)
        assert (client / 'ao-vk-live.install.json').is_file()
        payload(2)
        run('update', ROOT / 'tools/install-live.ps1', [*common, '-Upgrade'])
        assert sha(client / 'ao-vk.dll') == manifest['addon']['ao-vk.dll']
        assert sha(loader / 'ddraw.dll') == manifest['addon']['ddraw.dll']
        run('uninstall', ROOT / 'tools/install-live.ps1', [*common, '-Uninstall'])
        assert not (client / 'ao-vk.dll').exists() and not (loader / 'ddraw.dll').exists()
        assert sha(settings) == settings_hash
        assert all(sha(client / fn) == digest for fn, digest in originals.items())
        (client / 'N3.dll').write_text('unsupported fixture')
        run('failure-exit', ROOT / 'tools/install-live.ps1', common, success=False)
        assert not (client / 'ao-vk.dll').exists()
        results[name] = {'setup_parameter_forwarding': 'passed', 'install_update_uninstall_with_real_process_guard': 'passed',
                         'client_package_loader_paths_with_spaces': 'passed', 'upgrade_and_uninstall_switch_forwarding': 'passed',
                         'failure_exit_propagation': 'passed', 'stock_and_settings_preserved': True}

    # Execute the exact generated .cmd body against a harmless setup stub. This
    # checks host selection, action forwarding, quoting and exit propagation.
    spec = importlib.util.spec_from_file_location('packager', ROOT / 'tools/package-release.py')
    packager = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packager)
    launch = fixture / 'launcher with spaces'
    launch.mkdir()
    (launch / 'setup-live.ps1').write_text('param([string]$Action)\n"$([IntPtr]::Size):$Action" | Set-Content -LiteralPath (Join-Path $PSScriptRoot "host.txt")\n')
    for action in ['Install', 'Update', 'Uninstall']:
        path = launch / (action + '.cmd')
        path.write_bytes(packager.launcher(action))
        p = subprocess.run(['cmd.exe', '/d', '/c', str(path)], cwd=launch, env=env, capture_output=True, text=True, timeout=30)
        assert p.returncode == 0, (action, p.stdout, p.stderr)
        assert (launch / 'host.txt').read_text().strip() == '4:' + action
    results['cmd_launchers'] = {'actions': ['Install', 'Update', 'Uninstall'], 'actual_host_pointer_bytes': 4, 'result': 'passed'}
    (out / 'validation.json').write_text(json.dumps({'result': 'passed', 'hosts': results, 'game_installations_modified': False, 'processes_closed': False}, indent=2) + '\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
