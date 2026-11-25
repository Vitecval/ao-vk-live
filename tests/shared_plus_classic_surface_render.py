"""Check original Shared+ coverage, diagonal-independent cliff mixing and distant snow.

Uses actual renderer material/debug stages. No live-game appearance claim.
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
from PIL import Image, ImageFilter


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--sections', nargs='+', choices=['topology', 'cliffs', 'snow'], default=['topology', 'cliffs', 'snow'])
    p.add_argument('--cache', type=Path)
    p.add_argument('--validate-only', action='store_true')
    a = p.parse_args()
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    build = a.build.resolve(); exe = build/'rvk_demo.exe'
    digest = lambda f: hashlib.sha256(f.read_bytes()).hexdigest()
    binaries = {n: digest(build/n) for n in ['rvk_demo.exe', 'ao-vk.dll']}
    cache = out/'pipelines.cache'
    if a.cache and not cache.exists():
        shutil.copyfile(a.cache, cache)
    env = dict(os.environ, RANDYVK_PIPELINE_CACHE=str(cache))
    rows = {}
    common = ['--size', '768', '512', '--frames', '160', '--fixed-clock', '.000001',
              '--ground-rocks', '0', '--ground-pebbles', '0', '--ground-tess', '0', '--cliff-depth', '0']

    def run(name, flags, shared=False, native=False, masters=False):
        assert binaries == {n: digest(build/n) for n in binaries}, 'Build outputs changed during the run'
        d = out/name; d.mkdir(exist_ok=True)
        if not a.validate_only:
            with (d/'run.log').open('w') as log:
                subprocess.run([str(exe), *common, *flags, '--shot', 'result.bmp'], cwd=d,
                               env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
        log = (d/'run.log').read_text(errors='replace')
        assert 'rendered; screenshot result.bmp' in log, (name, 'No final screenshot')
        assert not re.search(r'VUID-|VK_ERROR|Validation Error|device lost|allocation failed', log, re.I), name
        dump = (d/'native-frame.txt').read_text(errors='replace')
        if shared:
            assert 'geometry=classic-submissions-HD-materials' in dump and 'original-terrain-consumed=0' in dump, name
            m = re.search(r'patches/frame ([\d.]+)', dump)
            assert m and float(m[1]) == 0, (name, 'Shared+ submitted replacement terrain')
        if native:
            assert 'geometry=native-patches' in dump and 'original-terrain-consumed=1' in dump, name
        if masters:
            assert 'hd-masters=6' in dump, (name, 'HD source upload not ready')
        assert binaries == {n: digest(build/n) for n in binaries}, 'Build outputs changed during rendering'
        print(name, 'rendered', flush=True)
        return np.asarray(Image.open(d/'result.bmp').convert('RGB'), dtype=float), log, dump

    if 'topology' in a.sections:
        for zone, region in [(800, 'icc'), (4605, 'inferno'), (4321, 'snow')]:
            for combined in [True, False]:
                stem = f'{region}-'+('combined' if combined else 'two-pass')
                base = ['--retail-cliff-fixture', '--sky-zone', str(zone)]
                if region == 'snow': base += ['--snow-ground-fixture']
                if combined: base += ['--grass-combined']
                original, _, _ = run(stem+'-original', base+['--terrain-blend', '0'])
                plus, _, _ = run(stem+'-plus', base+['--terrain-blend', '2'], shared=True)
                enhanced, _, _ = run(stem+'-explicit-groundon', base+['--terrain-blend', '2', '--native-on'], native=True)
                bg = original[0, 0]
                assert bg[2] > bg[0]+20, (stem, 'Missing clear-sky coverage reference')
                mask = lambda im: np.max(np.abs(im-bg), axis=2) > 1
                source = mask(original)
                delta = int(np.count_nonzero(source != mask(plus)))
                control = int(np.count_nonzero(source != mask(enhanced)))
                assert source.sum() > 5000 and delta == 0, (stem, 'Shared+ changed original coverage', delta)
                assert control > 500, (stem, 'Explicit GroundOn did not exercise reconstruction', control)
                rows[stem] = dict(changedCoveragePixels=delta, explicitGroundOnCoverageDifference=control)

    if 'cliffs' in a.sections:
        for zone in [800, 4321]:
            images = []
            # Looking straight down with an orthographic projection makes world
            # X/Z identical at every pixel. Flipping a nonplanar cell diagonal
            # changes actual surface Y, but cannot move its material mask in X/Z.
            for diagonal in [0, 1]:
                flags = ['--ground-cliff-probe', '1', '--ground-diagonal', str(diagonal), '--sky-zone', str(zone),
                         '--grass-combined', '--terrain-blend', '2', '--hd-ground', '1', '--cliff-join', '0', '--pbr-debug', '22']
                if zone == 4321: flags += ['--snow-ground-fixture']
                im, log, _ = run(f'cliff-{zone}-diagonal{diagonal}', flags, shared=True, masters=True)
                saddle = re.search(r'max saddle ([\d.]+)', log)
                assert saddle and float(saddle[1]) > .5, 'Fixture accidentally planar'
                assert 'native ground diagnostic stage: 22' in log, 'Cliff diagnostic never enabled'
                assert float(np.abs(im[:, :, 0]-im[:, :, 1]).max()) <= 1 and float(np.abs(im[:, :, 1]-im[:, :, 2]).max()) <= 1, 'Cliff weight image is not grayscale; material/debug stage inactive'
                images.append(im)
            # Central cliff/toe strip, excluding the texture and camera borders.
            left, right = [im[192:320, 128:640] for im in images]
            intermediate = int(np.count_nonzero((left[:, :, 0] > 8) & (left[:, :, 0] < 247)))
            delta = np.abs(left-right)
            mean, p99 = float(delta.mean()), float(np.quantile(delta, .99))
            assert intermediate > 500, ('Cliff blend is not exercised', zone, intermediate)
            assert mean < .35 and p99 <= 2, ('Triangle diagonal changes material weights', zone, mean, p99)
            rows[f'cliff-{zone}'] = dict(nonplanar=True, intermediateBlendPixels=intermediate,
                                        diagonalMeanDelta=mean, diagonalP99Delta=p99)

    if 'snow' in a.sections:
        for view, distance in [(2, 'near'), (3, 'far')]:
            base = ['--ground-cliff-probe', str(view), '--ground-diagonal', '0', '--snow-ground-fixture',
                    '--sky-zone', '4321', '--grass-combined', '--terrain-blend', '2', '--pbr-debug', '23']
            painted, log, dump = run('snow-'+distance+'-hd', base+['--hd-ground', '1'], shared=True, masters=True)
            assert 'native ground diagnostic stage: 23' in log, 'Snow material diagnostic never enabled'
            bare, _, _ = run('snow-'+distance+'-no-powder', base+['--hd-ground', '1', '--snow-powder', '0'], shared=True, masters=True)
            legacy, _, _ = run('snow-'+distance+'-legacy-detail', base+['--hd-ground', '0', '--snow-powder', '0'], shared=True)
            # Raw material stage avoids mist/daylight accidentally satisfying a
            # detail check. Compare the same view with working positive controls.
            hd_delta = np.abs(bare-legacy).max(axis=2)
            coat_delta = np.abs(painted-bare).max(axis=2)
            hd_pixels = int(np.count_nonzero(hd_delta > 2))
            coat_pixels = int(np.count_nonzero(coat_delta > 1))
            assert hd_pixels > 100, (distance, 'HD material detail missing', hd_pixels)
            assert coat_pixels > 50, (distance, 'Powder coating missing', coat_pixels)
            rows['snow-'+distance] = dict(hdMaterialChangedPixels=hd_pixels, powderChangedPixels=coat_pixels,
                                          hdMeanDelta=float(hd_delta.mean()), powderMeanDelta=float(coat_delta.mean()))
            if distance == 'near':
                # Camera targets the cliff centre. Remove broad material tint
                # before testing grain in both screen directions; vertical
                # stripes alone must not pass as resolved cliff texture.
                crop = bare[180:300, 280:488]
                blurred = np.asarray(Image.fromarray(crop.astype('uint8')).filter(ImageFilter.GaussianBlur(3)), dtype=float)
                fine = (crop-blurred).mean(axis=2)
                gx = float(np.abs(np.diff(fine, axis=1)).mean())
                gy = float(np.abs(np.diff(fine, axis=0)).mean())
                ratio = gy/max(gx, .0001)
                assert gx > .03 and gy > .03 and .12 < ratio < 8, ('Cliff grain collapsed to stripes', gx, gy, ratio)
                rows['snow-near'].update(grainGradientX=gx, grainGradientY=gy, grainVerticalHorizontalRatio=ratio)

    result = dict(result='passed', binarySha256=binaries, cases=rows, livePlaytest=False,
                  scope='original coverage, nonplanar diagonal blend and near/far material positive controls; inspect face grain visually')
    (out/'RESULTS.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    main()
