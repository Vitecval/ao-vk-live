"""Controlled sky pixels: SL traffic forbidden; RK traffic and SL clouds retained.

Complements ship_environment_check's complete theme/policy coverage. This uses
the existing sky fixture and never starts or edits a retail client.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess

from preview23_sky_life_tests import pixels, different


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cache', type=Path)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    exe = build / 'rvk_demo.exe'
    initial_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
    env = dict(os.environ, RANDYVK_PIPELINE_CACHE=str(args.cache.resolve() if args.cache else out / 'pipelines'))
    result = dict(result='failed', binarySha256=initial_hash, scope='Isolated Vulkan sky fixture; no retail client changed', cases=[])

    def run(name, zone, traffic=0, clouds=0):
        folder = out / name
        folder.mkdir(exist_ok=True)
        command = [str(exe), '--sky-enhance-fixture', '--soft-sky', '--water-lake', '--water-off',
                   '--sky-dome', '8FAACA', '--water-pitch', '-.18', '--sky-zone', str(zone),
                   '--stars', '0', '--meteors', '0', '--aurora', '0', '--sky-clouds', str(clouds),
                   '--sky-traffic', str(traffic), '--sun-dir', '.6', '-.7', '.3', '--size', '768', '512',
                   '--fixed-clock', '250', '--frames', '40', '--dump', 'frame.txt', '--shot', 'result.bmp']
        row = dict(case=name, zone=zone, requestedTraffic=traffic, requestedClouds=clouds, args=command[1:])
        result['cases'].append(row)
        with (folder / 'run.log').open('w') as log:
            subprocess.run(command, cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
        log = (folder / 'run.log').read_text(errors='replace')
        if re.search(r'VK_ERROR|VUID-|device lost|allocation failed', log, re.I):
            raise AssertionError('Vulkan error in ' + name)
        dump = (folder / 'sky-frame.txt').read_text(errors='replace')
        row['diagnostics'] = [line for line in dump.splitlines() if line.startswith('# Sky enhancements:')]
        if not row['diagnostics']:
            raise AssertionError('Sky enhancement fixture was not exercised: ' + name)
        if hashlib.sha256(exe.read_bytes()).hexdigest() != initial_hash:
            raise AssertionError('Executable changed during fixture')
        print(name + ' rendered', flush=True)
        return pixels(folder / 'result.bmp'), dump, row

    try:
        for zone in (4310, 4873, 4320, 4005, 4676):
            off, _, _ = run(f'sl-{zone}-off', zone)
            on, dump, row = run(f'sl-{zone}-busy', zone, traffic=2)
            row['pixelsChanged'] = different(off, on)
            if row['pixelsChanged'] != 0 or 'clouds 0; traffic 0' not in dump:
                raise AssertionError('Shadowlands traffic must be absent, including unknown/forest/garden biomes: ' + str(zone))
        off, _, _ = run('rk-655-off', 655)
        on, dump, row = run('rk-655-busy', 655, traffic=2)
        row['skyPixelsChanged'] = different(off[:256], on[:256])
        row['foregroundPixelsChanged'] = different(off[400:], on[400:])
        if row['skyPixelsChanged'] <= 30 or row['foregroundPixelsChanged'] != 0 or 'traffic 2' not in dump:
            raise AssertionError('Rubi-Ka positive control must retain actual sky traffic, without affecting ground')
        off = pixels(out / 'sl-4310-off' / 'result.bmp')
        clouds, dump, row = run('sl-4310-clouds-retained', 4310, traffic=2, clouds=1)
        row['skyPixelsChanged'] = different(off[:256], clouds[:256])
        row['foregroundPixelsChanged'] = different(off[400:], clouds[400:])
        if row['skyPixelsChanged'] <= 1000 or row['foregroundPixelsChanged'] != 0 or 'clouds 1; traffic 0' not in dump:
            raise AssertionError('Shadowlands exclusion must preserve clouds and ground')
        result['result'] = 'passed'
    except Exception as exc:
        result['error'] = str(exc)
    finally:
        (out / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
    return 0 if result['result'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
