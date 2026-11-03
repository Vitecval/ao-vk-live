"""Real installer checks for its required model index, in isolated client folders."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
def sha(b): return hashlib.sha256(b).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--tools', type=Path, default=ROOT / 'tools')
    args = p.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    run = out / ('run-' + uuid.uuid4().hex[:8]); run.mkdir()
    index = (ROOT / 'release/model-index/mesh-ids.bin').read_bytes()
    count = struct.unpack_from('<I', index, 12)[0]
    end = 20 + 32 * count
    assert index[:8] == b'AOMESHID' and index[end:] == b'\0scenery_spaceship\0'
    rows = list(struct.iter_unpack('<QQIIHHI', index[20:end]))
    assert {x[6] for x in rows} == {0, 1} and sum(x[6] == 1 for x in rows) == 13
    models = {(x[2], x[3]) for x in rows}
    assert all((1010001, x) in models for x in [296208, 296252, 296253, 296254])
    source = (ROOT / 'rvk/window_global.h').read_text()
    witnesses = re.findall(r'\{(\d+),(1010001|1010026),(\d+)\}', source.split('windowModelWitnesses[]={', 1)[1].split('};', 1)[0])
    coverage = {pair for _, t, m in witnesses if (pair := (int(t), int(m))) in models}
    assert len(coverage) > 200
    name_table = index[end:]
    named = bytearray(index + b'local debug model\0')
    struct.pack_into('<I', named, 16, len(name_table) + len(b'local debug model\0'))
    for i, row in enumerate(rows):
        if row[6] == 0: struct.pack_into('<I', named, 48 + i * 32, len(name_table))
    host = Path(os.environ['WINDIR']) / 'SysWOW64/WindowsPowerShell/v1.0/powershell.exe'
    env = {k:v for k,v in os.environ.items() if k.lower() != 'psmodulepath'}
    env['PSModulePath'] = str(host.parent / 'Modules')
    cases = []
    for edition in ['lite', 'full']:
        for case in ['clean', 'update3', 'preexisting', 'unowned-uninstall', 'corrupt-existing',
                     'missing-package', 'corrupt-package', 'missing-descriptor', 'quarantine', 'replace-failure']:
            folder = run / (edition + '-' + case); package = folder / 'package'; client = folder / 'client with spaces'
            package.mkdir(parents=True); client.mkdir()
            originals = {}
            for n in ['randy31.dll', 'DisplaySystem.dll', 'Gamecode.dll', 'GUI.dll', 'N3.dll', 'FXS.dll']:
                b = ('stock fixture ' + n).encode(); (client / n).write_bytes(b); originals[n] = sha(b)
            for n in ['AnarchyOnline.exe', 'Anarchy.exe']: (client / n).write_bytes(b'unchanged fixture executable')
            (client / 'randy-vk.ini').write_bytes(b'[Renderer]\r\nRVK_WindowDepth=2\r\n')
            personal = client / 'randy-vk/packs/personal/keep.txt'; personal.parent.mkdir(parents=True); personal.write_text('keep')
            addons = {}
            for n in ['ao-vk.dll', 'ddraw.dll']:
                b = ('renderer ' + n).encode(); (package / n).write_bytes(b); addons[n] = sha(b)
            relative = 'randy-vk/mesh-ids.bin'
            target = package / relative; target.parent.mkdir(); target.write_bytes(index)
            m = dict(product='AO-VK Live', build='mesh-index-fixture', client=originals, addon=addons,
                     assets={relative:sha(index)}, previousAddons=[], edition=edition, setupRevision=4,
                     meshIndex=dict(file=relative, formatVersion=1, records=count, sha256=sha(index)))
            (package / 'manifest.json').write_text(json.dumps(m))
            legacy = dict(m, assets={}, setupRevision=3); legacy.pop('meshIndex')
            (package / 'legacy.json').write_text(json.dumps(legacy))
            (folder / 'named.bin').write_bytes(named)
            script = r'''
$ErrorActionPreference='Stop'
$installer='INSTALLER';$client='CLIENT';$package='PACKAGE';$case='CASE';$named='NAMED'
function Assert($x,$m){if(!$x){throw $m}}
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Snapshot {
 $r=@{};Get-ChildItem -LiteralPath $client -File -Recurse|ForEach-Object {$r[$_.FullName]=Hash $_.FullName};return $r
}
$idx=Join-Path $client 'randy-vk/mesh-ids.bin';$receipt=Join-Path $client 'ao-vk-live.install.json'
$config=Join-Path $package 'manifest.json';$manifest=Get-Content $config -Raw|ConvertFrom-Json
$argsInstall=@{ClientDir=$client;PackageDir=$package;DisableLargeAddressAware=$true}
$legacyCase=$case -in @('update3','unowned-uninstall','quarantine','replace-failure')
if($legacyCase){
 Copy-Item $config (Join-Path $package 'new.json')
 Copy-Item (Join-Path $package 'legacy.json') $config -Force
 & $installer @argsInstall|Out-Null
 Copy-Item (Join-Path $package 'new.json') $config -Force
}
if($case -eq 'preexisting'){Copy-Item $named $idx}
if($case -eq 'unowned-uninstall'){Copy-Item (Join-Path $package 'randy-vk/mesh-ids.bin') $idx}
if($case -eq 'corrupt-existing'){[IO.File]::WriteAllText($idx,'user supplied invalid index')}
if($case -eq 'missing-package'){Remove-Item (Join-Path $package 'randy-vk/mesh-ids.bin')}
if($case -eq 'corrupt-package'){[IO.File]::WriteAllText((Join-Path $package 'randy-vk/mesh-ids.bin'),'corrupt')}
if($case -eq 'missing-descriptor'){$manifest.PSObject.Properties.Remove('meshIndex');$manifest|ConvertTo-Json -Depth 8|Set-Content $config}
$before=Snapshot
$global:MeshInject=$case
$global:MeshTarget=$idx
function global:Move-Item {
 param($LiteralPath,$Destination)
 if($Destination -eq $global:MeshTarget -and $LiteralPath -like '*.installing-*' -and $global:MeshInject -eq 'replace-failure') {throw 'injected index replacement failure'}
 Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
 if($Destination -eq $global:MeshTarget -and $LiteralPath -like '*.installing-*' -and $global:MeshInject -eq 'quarantine'){
  Microsoft.PowerShell.Management\Remove-Item -LiteralPath $Destination
 }
}
if($case -eq 'unowned-uninstall'){
 & $installer @argsInstall -Uninstall|Out-Null
 Assert (Test-Path -LiteralPath $idx) 'Uninstall removed an unowned index'
 Assert (!(Test-Path -LiteralPath $receipt)) 'Uninstall left receipt'
} elseif($case -in @('corrupt-existing','missing-package','corrupt-package','missing-descriptor','quarantine','replace-failure')) {
 $failed=$false
 try {& $installer @argsInstall -Upgrade|Out-Null} catch {$failed=$true;if($case -eq 'quarantine'){Assert ($_.Exception.Message -match 'quarantin') 'Quarantine did not report clearly'}}
 Assert $failed ('Expected failure: '+$case)
 $after=Snapshot;Assert ($before.Count -eq $after.Count) 'Failure changed file count'
 foreach($k in $before.Keys){Assert ($before[$k] -eq $after[$k]) ('Failure changed '+$k)}
} else {
 & $installer @argsInstall -Upgrade:$legacyCase|Out-Null
 Assert (Test-Path -LiteralPath $idx) 'Required index not installed'
 $record=Get-Content $receipt -Raw|ConvertFrom-Json
 if($case -eq 'preexisting'){
  Assert ((Hash $idx) -eq (Hash $named)) 'Existing names/index changed'
  Assert (!$record.assets.'randy-vk/mesh-ids.bin') 'Pre-existing index was adopted'
 } else {Assert ($record.assets.'randy-vk/mesh-ids.bin' -eq $manifest.meshIndex.sha256) 'Index not owned/pinned'}
 & $installer @argsInstall -Upgrade|Out-Null
 & $installer @argsInstall -Uninstall|Out-Null
 Assert ((Test-Path -LiteralPath $idx) -eq ($case -eq 'preexisting')) 'Wrong uninstall ownership'
 & $installer @argsInstall -Uninstall|Out-Null
}
Assert ((Hash (Join-Path $client 'randy-vk.ini')) -eq 'PREFHASH') 'Preferences changed'
Assert (Test-Path (Join-Path $client 'randy-vk/packs/personal/keep.txt')) 'Personal pack removed'
Get-ChildItem -LiteralPath $client -File -Recurse|Where-Object {$_.Name -match '\.(installing|upgrade)-'}|ForEach-Object {throw 'Transaction residue'}
Write-Output ('PASS '+$case)
'''
            replacements = dict(INSTALLER=str((args.tools / 'install-live.ps1').resolve()), CLIENT=str(client),
                                PACKAGE=str(package), CASE=case, NAMED=str(folder / 'named.bin'),
                                PREFHASH=sha((client / 'randy-vk.ini').read_bytes()))
            for k,v in replacements.items(): script = script.replace(k, v.replace("'", "''"))
            ps = folder / 'test.ps1'; ps.write_text(script)
            result = subprocess.run([str(host), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(ps)],
                                    capture_output=True, text=True, env=env, timeout=180)
            (folder / 'run.log').write_text(result.stdout + result.stderr)
            assert result.returncode == 0, (edition, case, result.stdout[-1000:], result.stderr[-2000:])
            cases.append(dict(edition=edition, case=case, result='passed'))
            print('PASS', edition, case, flush=True)
    receipt = dict(result='passed', index_records=count, static_models=len(models),
                   covered_global_model_witnesses=len(coverage), no_names_or_geometry_shipped=True, cases=cases)
    (out / 'RESULTS.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__': main()
