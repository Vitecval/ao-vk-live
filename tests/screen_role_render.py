"""Render captured screen roles, native coverage and minification controls.

Original game pixels are not bundled. Pass --native-dir containing the three
RGBA exports 258411-native.png, 258412-native.png and 285825-native.png.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--native-dir', type=Path, required=True)
    parser.add_argument('--validation-layers', type=Path, required=True)
    parser.add_argument('--frames', type=int, default=120)
    args = parser.parse_args()
    exe, out, native = (p.resolve() for p in (args.exe, args.output, args.native_dir))
    assert args.frames >= 80, 'Exercise refreshes and multiple owner re-tags.'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'RESULTS.json').unlink(missing_ok=True)
    executable_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
    hashes = {}
    for identity in (258411, 258412, 285825):
        path = native / f'{identity}-native.png'
        hashes[str(identity)] = hashlib.sha256(path.read_bytes()).hexdigest()

    cases = {}

    def render(name, mode, edges=1, leaf=0, dither=0, native_pixels=False,
               update=0, retag=0, offset=0, distance=6, taa=False, extra_frame=False,
               lod=35, validation=False):
        directory = out / name
        directory.mkdir(exist_ok=True)
        command = [str(exe), '--screen-card-test', str(mode),
                   '--screen-card-edges', str(edges), '--screen-card-dither', str(dither),
                   '--screen-card-update', str(update), '--screen-card-retag', str(retag),
                   '--screen-card-offset', str(offset), '--screen-card-distance', str(distance),
                   '--hdr', '--bloom', '0', '--night-glow', '0', '--leaf', str(leaf),
                   '--foliage-lod', str(lod), '--fixed-clock', '16.6666667',
                   '--size', '640', '480', '--frames', str(args.frames + int(extra_frame)),
                   '--shot', str(directory / 'frame.bmp')]
        if native_pixels:
            command += ['--screen-card-native', str(native)]
        if taa:
            command += ['--taa']
        environment = os.environ.copy()
        environment['RANDYVK_PIPELINE_CACHE'] = str(out/'pipelines.cache')
        if validation:
            environment.update(VK_LAYER_PATH=str(args.validation_layers.resolve()),
                               VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation', VK_LOADER_DEBUG='layer',
                               VK_LAYER_VALIDATE_SYNC='1')
        with (directory / 'run.log').open('w') as log:
            subprocess.run(command, cwd=directory, env=environment, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        image = np.asarray(Image.open(directory / 'frame.bmp').convert('RGB'), dtype=np.float64)
        cases[name] = image
        print(name, 'rendered', flush=True)

    def difference(a, b, region=None):
        delta = np.abs(cases[a] - cases[b])
        if region:
            x0, y0, x1, y1 = region
            delta = delta[y0:y1, x0:x1]
        return dict(max=float(delta.max()), mean=float(delta.mean()))

    for name, mode, edges, leaf, dither, lod in [
        ('layer-on', 3, 1, 0, 0, 35), ('layer-off', 3, 0, 0, 0, 35),
        ('old-layer-on', 5, 1, 0, 0, 35), ('old-layer-off', 5, 0, 0, 0, 35),
        ('agency-base', 4, 0, 0, 0, 35), ('agency-leaf', 4, 1, 5, 0, 35),
        ('agency-dither', 4, 1, 5, 1, 35), ('agency-lod', 4, 1, 0, 0, 1),
        ('old-agency-base', 6, 0, 0, 0, 35), ('old-agency-leaf', 6, 1, 5, 0, 35),
        ('leaf-base', 2, 1, 0, 0, 35), ('leaf-light', 2, 1, 5, 0, 35),
    ]:
        render(name, mode, edges, leaf, dither, lod=lod)

    render('native-layer-on', 3, native_pixels=True)
    render('native-layer-off', 3, edges=0, native_pixels=True)
    render('native-agency-off', 4, edges=0, native_pixels=True)
    render('native-agency-on', 4, leaf=5, dither=1, native_pixels=True)
    render('native-layer-taa', 3, native_pixels=True, taa=True)
    render('native-layer-taa-next', 3, native_pixels=True, taa=True, extra_frame=True)

    # A red native reference and red replacement have the same final colour.
    # The replacement's deliberately opaque alpha must never replace the live
    # original's coverage, including full and partial animation uploads.
    for update, label in ((0, 'static'), (1, 'full-update'), (2, 'partial-update')):
        render('alpha-reference-' + label, 7, native_pixels=True, update=update)
        render('alpha-replacement-' + label, 8, native_pixels=True, update=update)
    render('alpha-owner-retag', 8, native_pixels=True, update=2, retag=1, validation=True)

    # Subpixel translation of a minified native scanline field: actual surface
    # detail changes smoothly. The unlisted control retains the old level0
    # sampler and deliberately demonstrates the alias that this fixes.
    for old in (False, True):
        prefix = 'old-' if old else ''
        for phase in (False, True):
            suffix = '-phase' if phase else ''
            render(prefix + 'minified-agency' + suffix, 6 if old else 4, edges=0,
                   native_pixels=True, offset=.004 if phase else 0)
            render(prefix + 'minified-lines' + suffix, 5 if old else 3, edges=0,
                   native_pixels=True, distance=24, offset=.016 if phase else 0)

    result = dict(
        result='passed', frames=args.frames, executableSha256=executable_hash,
        nativeInputSha256=hashes, retailPostInstallVerified=False,
        washOverlaySplit=difference('layer-on', 'layer-off'),
        oldWashOverlaySplit=difference('old-layer-on', 'old-layer-off'),
        agencyLeaf=difference('agency-base', 'agency-leaf'),
        agencyDither=difference('agency-leaf', 'agency-dither'),
        agencyLod=difference('agency-base', 'agency-lod'),
        oldAgencyLeaf=difference('old-agency-base', 'old-agency-leaf'),
        genuineLeaf=difference('leaf-base', 'leaf-light'),
        nativeOverlaySplit=difference('native-layer-on', 'native-layer-off'),
        nativeAgencyFoliage=difference('native-agency-off', 'native-agency-on'),
        nativeTaaInterior=difference('native-layer-taa', 'native-layer-taa-next', (130, 150, 510, 330)),
        alphaReference={label: difference('alpha-reference-' + label, 'alpha-replacement-' + label)
                        for label in ('static', 'full-update', 'partial-update')},
        nativeAnimationChange=difference('alpha-reference-static', 'alpha-reference-partial-update'),
        ownerRetag=difference('alpha-reference-partial-update', 'alpha-owner-retag'),
        # Native rows925..975 contain only the two-row scanline field. The
        # right margin also includes glyph-edge mip footprints, whose proper
        # subpixel movement is not scanline aliasing.
        minifiedAgency=difference('minified-agency', 'minified-agency-phase', (150, 325, 490, 335)),
        oldMinifiedAgency=difference('old-minified-agency', 'old-minified-agency-phase', (150, 325, 490, 335)),
        minifiedLines=difference('minified-lines', 'minified-lines-phase', (278, 222, 362, 258)),
        oldMinifiedLines=difference('old-minified-lines', 'old-minified-lines-phase', (278, 222, 362, 258)),
    )
    log = (out / 'alpha-owner-retag/run.log').read_text(errors='replace')
    result['validationLayerActive'] = 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in log
    result['synchronizationValidationRequested'] = True
    result['validationErrors'] = [line for line in log.splitlines()
                                  if 'VUID-' in line or 'Validation Error' in line or 'SYNC-HAZARD' in line]
    dump = (out / 'alpha-owner-retag/screen-card-frame.txt').read_text(errors='replace')
    result['nativeMipDiagnostics'] = re.findall(
        r'screen-native-levels:(\d+) filtered-levels:(\d+) refreshes:(\d+)', dump)
    try:
        assert hashlib.sha256(exe.read_bytes()).hexdigest() == executable_hash, 'Executable changed during render run.'
        assert result['washOverlaySplit']['max'] <= 1 and result['oldWashOverlaySplit']['max'] > 5
        for key in ('agencyLeaf', 'agencyDither', 'agencyLod', 'nativeOverlaySplit', 'nativeAgencyFoliage'):
            assert result[key]['max'] <= 1, key
        assert result['oldAgencyLeaf']['max'] > 5 and result['genuineLeaf']['max'] > 5
        for label, values in result['alphaReference'].items():
            assert values['max'] <= 2, label
        assert result['nativeAnimationChange']['max'] > 20
        assert result['ownerRetag']['max'] <= 2
        assert result['nativeTaaInterior']['mean'] < 1
        assert result['minifiedAgency']['max'] <= 2
        assert result['oldMinifiedAgency']['max'] > result['minifiedAgency']['max'] + 3
        assert result['minifiedLines']['mean'] < result['oldMinifiedLines']['mean']
        assert result['validationLayerActive'] and not result['validationErrors']
        assert any(int(original) == 1 and int(filtered) > 1 and int(refreshes) > 2
                   for original, filtered, refreshes in result['nativeMipDiagnostics'])
    except AssertionError:
        result['result'] = 'failed'
        raise
    finally:
        (out / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
