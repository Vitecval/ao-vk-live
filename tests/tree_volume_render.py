"""Render tree volume with real AO leaf masks on a controlled crown fixture.

Original textures are supplied locally and never published with the test.
The fixture is not a claim of in-game visual verification.
"""
import argparse, json, math, os, re, struct, subprocess
from pathlib import Path
import numpy as np
from PIL import Image


def fixture(path, texture):
    pixels = np.array(Image.open(texture).convert('RGBA'))[:, :, [2, 1, 0, 3]]
    h, w = pixels.shape[:2]
    vertices, indices = [], []
    # Several intersecting cards with different branch heights, like AO crowns.
    for angle, y, radius in [(a, y, r) for a in [0, 1.05, 2.1] for y, r in [(4.6, 2.8), (6.8, 3.5), (8.8, 2.5)]]:
        x, z = math.cos(angle) * radius, math.sin(angle) * radius
        base = len(vertices)
        for px, py, pz, u, v in [(-x, y-1.8, -z, 0, 1), (x, y-1.8, z, 1, 1),
                                  (x, y+1.8, z, 1, 0), (-x, y+1.8, -z, 0, 0)]:
            vertices.append((px, py, pz, -math.sin(angle), 0, math.cos(angle), u, v))
        indices += [base, base+1, base+2, base, base+2, base+3]
    def part(flags, pix, vs, ix):
        ph, pw = pix.shape[:2]
        data = struct.pack('<4I', flags, 128, pw, ph) + pix.tobytes()
        data += struct.pack('<I', len(vs)) + np.array(vs, dtype='<f4').tobytes()
        data += struct.pack('<I', len(ix)) + np.array(ix, dtype='<u2').tobytes()
        return data + bytes((-len(data)) % 4)
    trunk, ti = [], []
    for k in range(8):
        a, b = k*math.tau/8, (k+1)*math.tau/8
        base = len(trunk)
        for t, y, r, u, v in [(a, 0, .42, 0, 1), (b, 0, .42, 1, 1), (b, 9, .1, 1, 0), (a, 9, .1, 0, 0)]:
            trunk.append((r*math.cos(t), y, r*math.sin(t), math.cos(t), 0, math.sin(t), u, v))
        ti += [base, base+1, base+2, base, base+2, base+3]
    bark = np.array([[[48, 65, 79, 255]]], dtype=np.uint8)
    path.write_bytes(b'RVKT' + struct.pack('<2I', 1, 2) + part(0, bark, trunk, ti) + part(1, pixels, vertices, indices))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--originals', type=Path, required=True)
    p.add_argument('--build', type=Path, default=Path('build/windows-live'))
    p.add_argument('--output', type=Path, default=Path('build/tree-volume'))
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    exe = (args.build / 'rvk_demo.exe').resolve()
    trees = []
    for identity in [42116, 6269, 42065, 26302]:
        path = out / f'{identity}.tree'
        fixture(path, args.originals / f'1010004_{identity}.png')
        trees.append(str(path))
    env = dict(os.environ, RANDYVK_PIPELINE_CACHE=str(root / 'build/shared-terrain-check/pipeline.cache'))
    common = ['--leaf-tree', ','.join(trees), '--hdr', '--pixel-lighting', '--fixed-clock', '1000', '--size', '960', '540',
              '--leaf-wind', '0', '--leaf-flutter', '0', '--leaf-fall', '0', '--leaf-no-own', '--leaf-core', '.3']
    results = {}
    for name, extra, frames in [('cards', ['--leaf-off'], 240), ('old', ['--leaf-no-volume'], 240),
                                 ('volume', [], 240), ('repeat', [], 240), ('cycle', ['--leaf-volume-cycle'], 241),
                                 ('forest', ['--leaf-forest', '8','--profile-window'], 600),
                                 ('forest-old', ['--leaf-forest', '8','--leaf-no-volume','--profile-window'], 600)]:
        with (out / f'{name}.log').open('w') as log:
            subprocess.run([str(exe), *common, '--frames', str(frames), '--shot', str(out / f'{name}.bmp'), *extra],
                           cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (out / f'{name}.log').read_text(errors='replace')
        assert not any(x in text for x in ['VUID-', 'VK_ERROR', 'AddressSanitizer', 'device lost']), name
        results[name] = np.array(Image.open(out / f'{name}.bmp').convert('RGB'))
    assert np.array_equal(results['volume'], results['repeat']), 'unstable repeated bake'
    assert np.array_equal(results['volume'], results['cycle']), 'mode cycling changed the final crown'
    changed = np.any(results['volume'] != results['old'], axis=2)
    assert changed.sum() > 1000, 'no visible tree-volume effect'
    # Sky and ground outside the crowns must be unchanged.
    assert np.array_equal(results['volume'][:20], results['old'][:20])
    report = {'tree_masks': [42116, 6269, 42065, 26302], 'changed_pixels': int(changed.sum()),
              'repeat_and_toggle_stable': True, 'forest_instances': 64, 'in_game_visual_verification': False}
    timings={}
    for name in ['forest','forest-old']:
        line=re.search(r'tree window: gpu ms:([^\n]+)',(out/f'{name}.log').read_text())
        assert line,(name,'missing GPU profile')
        timings[name]=float(re.search(r'\| total ([\d.]+)',line[1])[1])
    assert timings['forest']<timings['forest-old']*1.5+.2,('unbounded forest cost',timings)
    report['fixture_gpu_total_ms']=timings
    (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
