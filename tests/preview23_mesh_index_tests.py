"""Run the x86 installer on isolated clients with legacy model-index variants.

Optional existing-index input is read only and copied into private test fixtures;
no actual game executable is copied or run, and no real client is modified.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def index(named=False, different_identity=False, different_category=False):
    names = b'\0wall from an older index\0scenery_spaceship_legacy\0' if named else b'\0scenery_spaceship\0'
    offsets = [1, names.index(b'scenery_spaceship')] if named else [0, 1]
    if different_category:
        offsets[1] = 0
    entries = b''.join(struct.pack('<QQIIHHI', 11 + i + int(different_identity),
                                  21 + i, 101, 31 + i, 0, 1, offsets[i]) for i in range(2))
    return b'AOMESHID' + struct.pack('<III', 1, 2, len(names)) + entries + names


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--package-index', type=Path)
    p.add_argument('--existing-index', type=Path)
    p.add_argument('--tools', type=Path, default=ROOT / 'tools')
    args = p.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to(ROOT.parent), 'Keep fixtures inside Preview23'
    run = out / ('run-' + uuid.uuid4().hex[:10])
    run.mkdir(parents=True)
    cases = []

    def add(name, existing=None, owned=False, upgrade=False, reject='', removing=False,
            missing_source=False, missing_descriptor=False, fail_commit=False,
            source=None, stale_owned_hash=False):
        folder = run / name
        client, package = folder / 'AO dummy client', folder / 'package'
        client.mkdir(parents=True)
        package.mkdir()
        originals = {}
        for n in ['randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll']:
            data = ('stock dummy ' + n).encode()
            (client / n).write_bytes(data)
            originals[n] = sha(data)
        (client / 'AnarchyOnline.exe').write_bytes(b'dummy, never executable')
        (client / 'randy-vk.ini').write_bytes(b'[Renderer]\r\nRVK_LivePeople=0\r\n')
        addon = {}
        old_addon = {}
        for n in ['ao-vk.dll', 'ddraw.dll']:
            data = ('candidate dummy ' + n).encode()
            (package / n).write_bytes(data)
            addon[n] = sha(data)
            if upgrade or removing:
                data = ('previous dummy ' + n).encode()
                (client / n).write_bytes(data)
                old_addon[n] = sha(data)
        relative = 'randy-vk/mesh-ids.bin'
        source = index() if source is None else source
        target = client / relative
        if existing is not None:
            target.parent.mkdir()
            target.write_bytes(existing)
        if not missing_source:
            (package / relative).parent.mkdir()
            (package / relative).write_bytes(source)
        manifest = dict(product='AO-VK Live', build='model-index-fixture', setupRevision=4,
                        client=originals, addon=addon, previousAddons=[old_addon],
                        assets={relative: sha(source)})
        if not missing_descriptor:
            manifest['meshIndex'] = dict(file=relative, formatVersion=1, sha256=sha(source))
        (package / 'manifest.json').write_text(json.dumps(manifest))
        if upgrade or removing:
            receipt = dict(product='AO-VK Live', build='previous-fixture', clientDirectory=str(client),
                           loaderDirectory='', addon=old_addon, assets={})
            if owned:
                receipt['assets'][relative] = sha(source if stale_owned_hash or existing is None else existing)
            (client / 'ao-vk-live.install.json').write_text(json.dumps(receipt))
        cases.append(dict(name=name, client=str(client), package=str(package), upgrade=upgrade,
                          removing=removing, reject=reject, failCommit=fail_commit,
                          preserve=existing is not None and (not owned or existing != source),
                          existingHash=sha(existing) if existing is not None else '',
                          sourceHash=sha(source)))

    add('fresh-missing')
    add('fresh-identical-unowned', index())
    add('fresh-compatible-names-unowned', index(True))
    add('upgrade-compatible-names-unowned', index(True), upgrade=True)
    add('upgrade-compatible-names-owned', index(True), owned=True, upgrade=True)
    add('upgrade-compatible-names-stale-receipt', index(True), owned=True, upgrade=True, stale_owned_hash=True)
    add('upgrade-identical-owned', index(), owned=True, upgrade=True)
    add('upgrade-missing-owned', owned=True, upgrade=True)
    add('different-model-identities', index(True, different_identity=True), upgrade=True,
        reject='differs from this supported client build')
    add('different-renderer-category', index(True, different_category=True), upgrade=True,
        reject='differs from this supported client build')
    add('truncated-existing', b'AOMESHID', upgrade=True, reject='Invalid existing model-identification')
    bad_offset = bytearray(index(True))
    struct.pack_into('<I', bad_offset, 48, 99999)
    add('bad-name-offset', bytes(bad_offset), upgrade=True, reject='Invalid existing model-index name offset')
    add('missing-required-source', index(True), upgrade=True, missing_source=True,
        reject='Required model-identification file is missing or damaged')
    add('missing-required-descriptor', index(True), upgrade=True, missing_descriptor=True,
        reject='required model-identification index is missing')
    add('rollback-preserves-existing', index(True), upgrade=True, fail_commit=True,
        reject='simulated receipt replacement failure')
    add('uninstall-unowned-corrupt', b'unowned invalid metadata', removing=True)
    add('uninstall-owned-modified', b'changed owned metadata', owned=True, removing=True,
        stale_owned_hash=True, reject='unrecognized')
    if args.existing_index or args.package_index:
        assert args.existing_index and args.package_index
        add('actual-index-copy-upgrade', args.existing_index.read_bytes(), upgrade=True,
            source=args.package_index.read_bytes())

    (run / 'cases.json').write_text(json.dumps(cases))
    quote = lambda path: "'" + str(path).replace("'", "''") + "'"
    script = r'''$ErrorActionPreference='Stop'
$env:PSModulePath=(Join-Path $PSHOME 'Modules')+';'+$env:PSModulePath
function global:Get-Process {param($Name,$ErrorAction)}
function Snapshot($Folder) {
    $snapshot=[ordered]@{}
    foreach ($file in @(Get-ChildItem -LiteralPath $Folder -File -Recurse | Sort-Object FullName)) {
        $snapshot[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
    return ($snapshot | ConvertTo-Json -Compress)
}
function global:Move-Item {
    param($LiteralPath,$Destination)
    if ($global:failReceipt -and $Destination -like '*ao-vk-live.install.json' -and $LiteralPath -like '*.installing-*') {
        throw 'simulated receipt replacement failure'
    }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
$cases=Get-Content -LiteralPath CASES -Raw | ConvertFrom-Json
foreach ($case in $cases) {
    $relative='randy-vk/mesh-ids.bin'
    $target=Join-Path $case.client $relative
    $receipt=Join-Path $case.client 'ao-vk-live.install.json'
    $before=Snapshot $case.client
    $global:failReceipt=[bool]$case.failCommit
    $arguments=@{ClientDir=$case.client;PackageDir=$case.package;DisableLargeAddressAware=$true}
    if ($case.upgrade) {$arguments.Upgrade=$true}
    if ($case.removing) {$arguments.Uninstall=$true}
    $caught=$false
    try { & INSTALLER @arguments | Out-Null } catch {
        $caught=$true
        if (!$case.reject -or $_.Exception.Message -notlike ('*'+$case.reject+'*')) {throw}
    }
    $global:failReceipt=$false
    if ($case.reject) {
        if (!$caught) {throw ('Missing expected rejection: '+$case.name)}
        if ((Snapshot $case.client) -cne $before) {throw ('Rejected transaction changed files: '+$case.name)}
    } else {
        if ($caught) {throw ('Unexpected failure: '+$case.name)}
        if (!$case.removing) {
            $record=Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
            if ($case.preserve) {
                if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $case.existingHash) {throw 'Existing index was replaced'}
                if ($record.assets.$relative) {throw 'Preserved index was adopted into the receipt'}
            } elseif ($record.assets.$relative -cne $case.sourceHash) {throw 'New index ownership missing'}
            & INSTALLER -ClientDir $case.client -PackageDir $case.package -Uninstall | Out-Null
        }
        if ($case.preserve) {
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ine $case.existingHash) {throw 'Uninstall modified the preserved index'}
        } elseif (Test-Path -LiteralPath $target) {throw 'Uninstall left an installer-owned index'}
    }
    Write-Output ('PASS: '+$case.name)
}
'''
    script = script.replace('CASES', quote(run / 'cases.json')).replace('INSTALLER', quote(args.tools.resolve() / 'install-live.ps1'))
    runner = run / 'check.ps1'
    runner.write_text(script, encoding='utf-8')
    host = Path(os.environ['WINDIR']) / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    result = subprocess.run([str(host), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(runner)],
                            capture_output=True, text=True, timeout=180)
    (run / 'run.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    (out / 'RESULTS.json').write_text(json.dumps(dict(result='passed', cases=[c['name'] for c in cases],
        x86_installer=True, actual_index_copied_read_only=bool(args.existing_index),
        real_client_modified=False, installer_sha256=sha((args.tools / 'install-live.ps1').read_bytes())), indent=2) + '\n')
    print(result.stdout)


if __name__ == '__main__':
    main()
