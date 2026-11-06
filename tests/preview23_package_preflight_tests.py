"""Exercise actual packaged asset manifests through the x86 installer.

Only synthetic client folders are inputs. A mocked busy-client guard deliberately
stops execution after asset-path validation and before any installation work.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import uuid


def quote(path):
    return "'" + str(path).replace("'", "''") + "'"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, action='append', required=True)
    parser.add_argument('--installer', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-rejection', default='Close the game')
    cases = parser.add_mutually_exclusive_group()
    cases.add_argument('--upgrade-only', action='store_true')
    cases.add_argument('--fresh-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    assert output.is_relative_to(root.parent), 'Keep fixtures inside Preview23'
    run = output / ('run-' + uuid.uuid4().hex[:10])
    run.mkdir(parents=True)
    host = Path(os.environ['WINDIR']) / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    rows = []
    for number, package in enumerate(args.package):
        package = package.resolve()
        manifest = json.loads((package / 'manifest.json').read_text(encoding='utf-8-sig'))
        installer = (args.installer or package / 'install-live.ps1').resolve()
        for upgrade in ([True] if args.upgrade_only else [False] if args.fresh_only else [False, True]):
            client = run / f'client-{number}-{int(upgrade)}'
            client.mkdir()
            if upgrade:
                receipt = dict(product='AO-VK Live', build='synthetic-previous',
                               clientDirectory=str(client), loaderDirectory='',
                               addon=manifest['previousAddons'][0], assets=manifest['assets'])
                (client / 'ao-vk-live.install.json').write_text(json.dumps(receipt))
            before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in client.iterdir()}
            script = run / f'probe-{number}-{int(upgrade)}.ps1'
            script.write_text(f"""$ErrorActionPreference='Stop'
function global:Get-Process {{param($Name,$ErrorAction)
    if ($Name -eq 'AnarchyOnline') {{ [pscustomobject]@{{Id=123;HasExited=$false}} }}
}}
$caught=$false
try {{
    & {quote(installer)} -ClientDir {quote(client)} -PackageDir {quote(package)} -DisableLargeAddressAware {'-Upgrade' if upgrade else ''}
}} catch {{
    $caught=$true
    if ($_.Exception.Message -notlike ('*'+{quote(args.expected_rejection)}+'*')) {{throw}}
    Write-Output $_.Exception.Message
}}
if (!$caught) {{throw 'Expected the deliberately busy fixture to stop installation'}}
""", encoding='utf-8')
            started = time.monotonic()
            result = subprocess.run([str(host), '-NoProfile', '-ExecutionPolicy', 'Bypass',
                                     '-File', str(script)], capture_output=True, text=True, timeout=600)
            (run / f'probe-{number}-{int(upgrade)}.log').write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in client.iterdir()}
            assert before == after, 'Preflight changed the synthetic client'
            rows.append(dict(edition=manifest['edition'], upgrade=upgrade,
                             asset_entries=len(manifest['assets']), client_unchanged=True,
                             process_seconds=time.monotonic() - started,
                             installer_sha256=hashlib.sha256(installer.read_bytes()).hexdigest(),
                             sourceCommit=manifest['sourceCommit']))
    (output / 'RESULTS.json').write_text(json.dumps(dict(result='passed',
        expected_rejection=args.expected_rejection, actual_game_client_tested=False,
        cases=rows), indent=2) + '\n')
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()
