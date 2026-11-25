"""Preserve enhanced Shared+ snow against r69, without testing restored non-snow.

Uses real beauty/material/normal shader stages and original AO-submission dumps.
Writes RESULTS.json on success or failure; never builds or changes installed files.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import traceback

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/preview23/x86')
    parser.add_argument('--before', type=Path, default=ROOT / 'build/environment-078-r70/before')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/environment-078-r70/snow-preservation-check')
    parser.add_argument('--cache', type=Path)
    parser.add_argument('--validate-only', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    folders = {'before': args.before.resolve(), 'after': args.build.resolve()}
    result = dict(result='failed', binarySha256={}, executablePaths={}, cases={}, runs={},
                  livePlaytest=False, scope='enhanced snow only; original AO geometry and lightmap')
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    common = ['--size', '768', '512', '--frames', '160', '--fixed-clock', '.000001',
              '--ground-rocks', '0', '--ground-pebbles', '0', '--ground-tess', '0', '--cliff-depth', '0',
              '--grass-combined', '--terrain-blend', '2', '--ground-diagonal', '0',
              '--sky-zone', '4321', '--snow-ground-fixture']
    cache = out / 'pipelines.cache'

    def persist() -> None:
        temporary = out / 'RESULTS.json.tmp'
        temporary.write_text(json.dumps(result, indent=2) + '\n')
        temporary.replace(out / 'RESULTS.json')

    def stable() -> None:
        for label, folder in folders.items():
            assert result['binarySha256'][label] == {name: sha(folder / name) for name in ('rvk_demo.exe', 'ao-vk.dll')}, 'Build outputs changed during snow preservation test'

    def run(label: str, name: str, flags: list[str], diagnostic: int | None = None) -> np.ndarray:
        stable()
        directory = out / label / name; directory.mkdir(parents=True, exist_ok=True)
        command = [str(folders[label] / 'rvk_demo.exe'), *common, *flags, '--shot', 'result.bmp']
        row = dict(command=command, cwd=str(directory), diagnostic=diagnostic)
        result['runs'][label + '/' + name] = row
        persist()
        if not args.validate_only:
            with (directory / 'run.log').open('w') as log:
                subprocess.run(command, cwd=directory, env=dict(os.environ, RANDYVK_PIPELINE_CACHE=str(cache)),
                               stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
        log = (directory / 'run.log').read_text(errors='replace')
        assert 'rendered; screenshot result.bmp' in log, (label, name, 'No final image')
        assert not re.search(r'VUID-|VK_ERROR|Validation Error|device lost|allocation failed', log, re.I), (label, name)
        if diagnostic is not None:
            assert f'native ground diagnostic stage: {diagnostic}' in log, (label, name, 'Diagnostic inactive')
        expected_source = 'coarse-cells' if '--ground-coarse-normals' in flags else 'smooth-samples'
        assert f'AO normal source: {expected_source}' in log, (label, name, 'Normal fixture control inactive')
        dump = (directory / 'native-frame.txt').read_text(errors='replace')
        assert 'shared-plus 1' in dump and 'geometry=classic-submissions' in dump, (label, name, 'Not classic Shared+')
        assert 'lighting=classic-AO-baked-lightmap' in dump and 'original-terrain-consumed=0' in dump, (label, name, 'AO lightmap/submissions replaced')
        patches = re.search(r'patches/frame ([\d.]+)', dump)
        assert patches and float(patches[1]) == 0, (label, name, 'GroundOn patch substitution')
        assert re.search(r'# snow: tiles=1;', dump) and 'detail-masters=6' in dump, (label, name, 'Snow identity/material absent')
        if flags[flags.index('--hd-ground') + 1] == '1':
            assert 'hd-masters=6' in dump, (label, name, 'HD upload not ready')
        powder = float(flags[flags.index('--snow-powder') + 1])
        assert f'powder={powder:.2f}' in dump, (label, name, 'Powder setting inactive')
        image = directory / 'result.bmp'
        row.update(imageSha256=sha(image), dumpSha256=sha(directory / 'native-frame.txt'),
                   originalTerrainConsumed=False, geometry='classic AO submissions', powder=powder)
        stable()
        print(label, name, 'rendered', flush=True)
        pixels = np.asarray(Image.open(image).convert('RGB'), dtype=np.float64)
        assert pixels.shape == (512, 768, 3), (label, name, 'Wrong actual output size', pixels.shape)
        return pixels

    def same(left: np.ndarray, right: np.ndarray, row: dict, key: str) -> None:
        delta = np.abs(left - right)
        row[key] = dict(maximumDelta=float(delta.max()), meanDelta=float(delta.mean()),
                        changedPixels=int(np.count_nonzero(delta.max(axis=2))))
        persist()
        assert delta.max() <= 1, (key, row[key])

    def positive(images: dict[str, np.ndarray], row: dict, label: str, distance: str) -> None:
        control = {}
        for key in ('base-smooth', 'beauty-smooth', 'mapped', 'material-hd-bare'):
            image = images[key]
            crop = image[160:340, 240:528]
            foreground = np.abs(image - image[0, 0]).max(axis=2)[160:340, 240:528] > 2
            pixels = crop[foreground]
            assert len(pixels) > 1000, (label, distance, key, 'No terrain in probe crop')
            # Per-channel spatial variance cannot be satisfied merely by a
            # constant blue/green diagnostic, sky border or flat tinted face.
            spatial_std = float(np.std(pixels, axis=0).mean())
            control[key + 'SpatialStd'] = spatial_std
            row[label + 'PositiveControls'] = control
            persist()
            assert spatial_std > 1, (label, distance, key, 'Constant/blank shader output', spatial_std)
        mapped = np.abs(images['mapped'] - images['plain']).max(axis=2)
        hd = np.abs(images['material-hd-bare'] - images['material-legacy-bare']).max(axis=2)
        powder = np.abs(images['material-hd-powder'] - images['material-hd-bare']).max(axis=2)
        control.update(normalMapPerturbedPixels=int(np.count_nonzero(mapped > 2)),
                       hdMaterialChangedPixels=int(np.count_nonzero(hd > 2)), powderChangedPixels=int(np.count_nonzero(powder > 1)),
                       hdMeanDelta=float(hd.mean()), powderMeanDelta=float(powder.mean()))
        persist()
        assert control['normalMapPerturbedPixels'] > 100, (label, distance, 'Normal map perturbation missing', control)
        assert control['hdMaterialChangedPixels'] > 100, (label, distance, 'HD material missing', control)
        assert control['powderChangedPixels'] > 50, (label, distance, 'Powder coat missing', control)
        if distance == 'near':
            crop = images['material-hd-bare'][180:300, 280:488]
            low = np.asarray(Image.fromarray(crop.astype('uint8')).filter(ImageFilter.GaussianBlur(3)), dtype=float)
            grain = (crop - low).mean(axis=2)
            gx, gy = float(np.abs(np.diff(grain, axis=1)).mean()), float(np.abs(np.diff(grain, axis=0)).mean())
            control.update(grainGradientX=gx, grainGradientY=gy, grainAxisRatio=gy / max(gx, .0001))
            persist()
            assert gx > .03 and gy > .03 and .12 < control['grainAxisRatio'] < 8, (label, 'Cliff detail collapsed', control)

    try:
        for label, folder in folders.items():
            result['binarySha256'][label] = {name: sha(folder / name) for name in ('rvk_demo.exe', 'ao-vk.dll')}
            result['executablePaths'][label] = str(folder / 'rvk_demo.exe')
        if args.cache and not cache.exists():
            shutil.copyfile(args.cache, cache)
        persist()
        for view, distance in ((2, 'near'), (3, 'far')):
            cases = {
                'base-smooth': (['--pbr-debug', '25', '--hd-ground', '1', '--snow-powder', '0'], 25),
                'base-coarse': (['--pbr-debug', '25', '--hd-ground', '1', '--snow-powder', '0', '--ground-coarse-normals'], 25),
                'beauty-smooth': (['--hd-ground', '1', '--snow-powder', '.65'], None),
                'beauty-coarse': (['--hd-ground', '1', '--snow-powder', '.65', '--ground-coarse-normals'], None),
                'plain': (['--pbr-debug', '26', '--terrain-off', '--hd-ground', '1', '--snow-powder', '0'], 26),
                'mapped': (['--pbr-debug', '26', '--terrain-off', '--ground-known-normal', '--hd-ground', '1', '--snow-powder', '0'], 26),
                'mapped-coarse': (['--pbr-debug', '26', '--terrain-off', '--ground-known-normal', '--ground-coarse-normals', '--hd-ground', '1', '--snow-powder', '0'], 26),
                'material-hd-powder': (['--pbr-debug', '23', '--hd-ground', '1', '--snow-powder', '.65'], 23),
                'material-hd-bare': (['--pbr-debug', '23', '--hd-ground', '1', '--snow-powder', '0'], 23),
                'material-legacy-bare': (['--pbr-debug', '23', '--hd-ground', '0', '--snow-powder', '0'], 23),
            }
            row = result['cases'][distance] = {}
            captures = {}
            for label in folders:
                captures[label] = {name: run(label, distance + '-' + name, ['--ground-cliff-probe', str(view), *flags], diagnostic)
                                   for name, (flags, diagnostic) in cases.items()}
                positive(captures[label], row, label, distance)
                same(captures[label]['base-smooth'], captures[label]['base-coarse'], row, label + 'BaseCoarseSmooth')
                same(captures[label]['beauty-smooth'], captures[label]['beauty-coarse'], row, label + 'BeautyCoarseSmooth')
                same(captures[label]['mapped'], captures[label]['mapped-coarse'], row, label + 'PerturbedCoarseSmooth')
            for name in cases:
                same(captures['before'][name], captures['after'][name], row, name + 'R69Preserved')
        stable()
        result['result'] = 'passed'
    except Exception as error:
        result['error'] = str(error)
        result['traceback'] = traceback.format_exc()
        raise
    finally:
        persist()
        print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    main()
