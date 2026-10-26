"""Artwork package integrity, response separation, and rendered alpha equivalence.

Run with Pillow/NumPy: python tests/character_art_tests.py [--render alpha.bmp]
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--render', type=Path)
    args = parser.parse_args()
    pack = ROOT / 'material-packs/ao-cyber-characters'
    manifest = json.loads((pack / 'manifest.json').read_text())
    completion=json.loads((pack/'completion-catalog.json').read_text())
    count=14+len(completion)
    assert manifest['new_art_surfaces'] == count
    assert len(manifest['files']) == count*4
    total = runtime_total = 0
    for name, expected in manifest['files'].items():
        assert hashlib.sha256((pack / 'maps' / name).read_bytes()).hexdigest() == expected, name
    for item in manifest['materials']:
        stem = f"{item['type']}_{item['id']}"
        assert item['preserve_original_alpha']
        maps = {suffix: np.asarray(Image.open(pack / 'maps' / (stem + '_' + suffix + '.png')).convert('RGB'))
                for suffix in ['d', 'n', 'orm', 'e']}
        w, h = item['size']
        assert maps['d'].shape == (h, w, 3)
        assert all(maps[suffix].shape == (h//2, w//2, 3) for suffix in ['n', 'orm', 'e'])
        # New response must separate metal and fabric rather than making every pixel shiny.
        orm = maps['orm'] / 255
        if item.get('profile')=='coated-armour':
            assert orm[:,:,2].max()<=.26 and orm[:,:,1].min()>=.56,stem
            assert np.mean((orm[:,:,2]<.1)&(orm[:,:,1]>.7))>.02,stem
            assert maps['e'].max()==0,stem
        else:
            assert np.mean(orm[:, :, 2] > .6) > .02, stem
            assert np.mean((orm[:, :, 2] < .1) & (orm[:, :, 1] > .7)) > .15, stem
        assert np.mean(np.max(maps['e'], axis=2) > 0) < .04, stem
        normals = maps['n'] / 127.5 - 1
        assert np.min(normals[:, :, 2]) > .98, stem
        # Four full-resolution map mip chains, per independently created AO surface.
        total += sum(value.shape[0] * value.shape[1] * 4 * 4 / 3 for value in maps.values())
        for value in maps.values():
            h,w=value.shape[:2];scale=min(1,512/max(w,h))
            runtime_total+=max(1,int(w*scale))*max(1,int(h*scale))*16/3
    assert total < 80 * 1024 * 1024 and runtime_total < 48 * 1024 * 1024
    assert all(max(x['size'])<=512 and x['group']=='preserved-equipment' for x in completion)
    results = {'actual_repaints': count, 'baked_maps': count*4, 'library_full_size_mip_mb': round(total / 1024**2, 1),'library_runtime_capped_mip_mb':round(runtime_total/1024**2,1)}
    if args.render:
        image = np.asarray(Image.open(args.render).convert('RGB'))
        deltas = []
        for row in range(4):
            y = 24 + row * 100
            a = image[y+3:y+60, 27:213].astype(int)
            b = image[y+3:y+60, 267:453].astype(int)
            delta = int(np.abs(a-b).max())
            assert delta == 0, (row, delta)
            assert a.max() - a.min() > 40, 'Blank rendering cannot pass'
            deltas.append(delta)
        results['alpha_pair_max_deltas'] = deltas
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
