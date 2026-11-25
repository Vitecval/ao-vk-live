"""Shared+ snow uses continuous fragment lighting with original AO geometry.

Coarse AO normals are test inputs, not a production bypass. Stage25 shows the
lighting base; stage26 shows the normal after existing material perturbation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import numpy as np
from PIL import Image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--before', type=Path, required=True, help='Preserved renderer/demo before the snow lighting change')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cache', type=Path)
    p.add_argument('--validate-only', action='store_true')
    a = p.parse_args(); out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    build = a.build.resolve(); before = a.before.resolve()
    sha = lambda f: hashlib.sha256(f.read_bytes()).hexdigest()
    pinned = {str(folder): {n: sha(folder/n) for n in ['rvk_demo.exe', 'ao-vk.dll']} for folder in [build, before]}
    cache = out/'pipelines.cache'
    if a.cache and not cache.exists(): shutil.copyfile(a.cache, cache)
    env = dict(os.environ, RANDYVK_PIPELINE_CACHE=str(cache))
    common = ['--size', '768', '512', '--frames', '160', '--fixed-clock', '.000001',
              '--ground-rocks', '0', '--ground-pebbles', '0', '--ground-tess', '0', '--cliff-depth', '0',
              '--grass-combined', '--terrain-blend', '2', '--hd-ground', '1', '--snow-powder', '0']
    metrics = {}

    def run(name, flags, previous=False, diagnostic=None):
        folder = before if previous else build
        assert pinned[str(folder)] == {n: sha(folder/n) for n in pinned[str(folder)]}, 'Build changed during test'
        d = out/name; d.mkdir(exist_ok=True)
        if not a.validate_only:
            with (d/'run.log').open('w') as log:
                subprocess.run([str(folder/'rvk_demo.exe'), *common, *flags, '--shot', 'result.bmp'], cwd=d,
                               env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
        log = (d/'run.log').read_text(errors='replace')
        assert 'rendered; screenshot result.bmp' in log, name
        assert not re.search(r'VUID-|VK_ERROR|Validation Error|device lost|allocation failed', log, re.I), name
        dump = (d/'native-frame.txt').read_text(errors='replace')
        assert 'geometry=classic-submissions-HD-materials' in dump and 'original-terrain-consumed=0' in dump, name
        expected_hd = flags[flags.index('--hd-ground')+1] if '--hd-ground' in flags else '1'
        assert 'patches/frame 0.00' in dump and ('hd-masters=0' if expected_hd=='0' else 'hd-masters=6') in dump, name
        if diagnostic:
            assert f'native ground diagnostic stage: {diagnostic}' in log, name
        assert pinned[str(folder)] == {n: sha(folder/n) for n in pinned[str(folder)]}, 'Build changed while rendering'
        print(name, 'rendered', flush=True)
        return np.asarray(Image.open(d/'result.bmp').convert('RGB'), dtype=float)

    def same(a0, b0, description):
        delta = np.abs(a0-b0)
        maximum, mean = float(delta.max()), float(delta.mean())
        assert maximum <= 1, (description, 'AO input normals still leak into snow lighting', maximum, mean)
        return dict(maximumDelta=maximum, meanDelta=mean)

    for view, name in [(2, 'near'), (3, 'far')]:
        base = ['--ground-cliff-probe', str(view), '--ground-diagonal', '0', '--sky-zone', '4321']
        snow = base+['--snow-ground-fixture']
        normals = []
        for coarse in [False, True]:
            suffix = 'coarse' if coarse else 'smooth'
            normals.append(run(f'{name}-snow-normal-{suffix}', snow+['--pbr-debug', '25']+
                               (['--ground-coarse-normals'] if coarse else []), diagnostic=25))
        row = dict(baseNormal=same(*normals, name+' continuous snow base'))
        assert normals[0][160:340, 240:528].std() > 5, (name, 'Blank/constant normal diagnostic')

        # Same snow-coloured image, AO lightmap and geometry; only source role
        # changes to non-snow. This must expose the injected discontinuities.
        controls = [run(f'{name}-nonsnow-normal-'+('coarse' if coarse else 'smooth'),
                        snow+['--ground-snow-identity-control', '--pbr-debug', '25']+
                        (['--ground-coarse-normals'] if coarse else []), diagnostic=25) for coarse in [False, True]]
        changed = int(np.count_nonzero(np.abs(controls[0]-controls[1]).max(axis=2) > 5))
        assert changed > 500, (name, 'Coarse normal positive control inactive', changed)
        row['coarseNonSnowChangedPixels'] = changed

        beauty = [run(f'{name}-snow-beauty-'+('coarse' if coarse else 'smooth'),
                      snow+(['--ground-coarse-normals'] if coarse else [])) for coarse in [False, True]]
        row['displayedLighting'] = same(*beauty, name+' snow beauty')
        assert beauty[0][160:340, 240:528].std() > 5, (name, 'Blank beauty image')

        # The new field is a base for maps, not a replacement for painted
        # perturbations. Remove rock overlays to isolate the known tile map.
        plain = run(f'{name}-normal-map-off', snow+['--terrain-off', '--pbr-debug', '26'], diagnostic=26)
        mapped = run(f'{name}-normal-map-on', snow+['--terrain-off', '--ground-known-normal', '--pbr-debug', '26'], diagnostic=26)
        mapped_coarse = run(f'{name}-normal-map-coarse', snow+['--terrain-off', '--ground-known-normal',
                            '--ground-coarse-normals', '--pbr-debug', '26'], diagnostic=26)
        row['mappedNormal'] = same(mapped, mapped_coarse, name+' mapped snow normal')
        perturbed = int(np.count_nonzero(np.abs(mapped-plain).max(axis=2) > 2))
        assert perturbed > 100, (name, 'Snow override erased normal-map perturbation', perturbed)
        row['normalMapPerturbedPixels'] = perturbed

        # All flags here existed in the preserved pre-change executable. This
        # exact comparison protects the 0.7.7 non-snow lighting contract.
        for zone in [800, 4605]:
            # Isolate the lighting contract from separately evolving HD colour
            # sampling; ordinary Shared+ retains its original AO normal path.
            ordinary = ['--ground-cliff-probe', str(view), '--sky-zone', str(zone), '--hd-ground', '0']
            old = run(f'{name}-zone{zone}-before', ordinary, previous=True)
            current = run(f'{name}-zone{zone}-after', ordinary)
            row[f'nonSnowZone{zone}'] = same(old, current, f'{name} nonSnow zone{zone}')
        metrics[name] = row

    # Material-normal direction itself must not inherit AO's triangulation.
    diag = [run('snow-normal-diagonal'+str(d), ['--ground-cliff-probe', '1', '--ground-diagonal', str(d),
                '--snow-ground-fixture', '--ground-coarse-normals', '--sky-zone', '4321', '--pbr-debug', '25'], diagnostic=25)
            for d in [0, 1]]
    metrics['diagonals'] = same(*diag, 'Snow base normal diagonal independence')
    result = dict(result='passed', binarySha256=pinned, cases=metrics, livePlaytest=False,
                  geometryAndLightmap='original AO submissions; original lightmap sampled unchanged')
    (out/'RESULTS.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    main()
