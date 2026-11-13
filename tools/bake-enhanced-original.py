"""Bake only the three reviewed generated ICC sources; never read the game database.

Source PNGs are retained verbatim. Public redistribution remains gated on rights
review because the generation used private original AO references.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1] / 'material-packs/ao-icc-enhanced-original'
TARGETS = [(1010004, 295610, .66, .65), (1010006, 21, .74, .08), (1010006, 22, .72, .08)]

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    maps = ROOT / 'maps'
    maps.mkdir(exist_ok=True)
    sources, materials = [], []
    for kind, identity, roughness, metallic in TARGETS:
        stem = f'{kind}_{identity}'
        source = ROOT / 'source-art' / (stem + '.png')
        original = Image.open(source).convert('RGBA')
        im = original.resize((1024, 1024), Image.Resampling.LANCZOS)
        assert np.asarray(im)[:, :, 3].min() == 255, 'Approved references are opaque'
        im.save(maps / (stem + '_d.png'), optimize=True)
        # Technical companion maps: gentle finite-difference relief, no new color art.
        gray = np.asarray(im.convert('L').filter(ImageFilter.GaussianBlur(.8)), dtype=np.float32) / 255
        dx = (np.roll(gray, -1, 1) - np.roll(gray, 1, 1)) * .5
        dy = (np.roll(gray, -1, 0) - np.roll(gray, 1, 0)) * .5
        normal = np.stack((-dx * 1.5, -dy * 1.5, np.ones_like(gray)), axis=2)
        normal /= np.linalg.norm(normal, axis=2, keepdims=True)
        Image.fromarray(np.uint8(np.clip(normal * .5 + .5, 0, 1) * 255 + .5)).save(maps / (stem + '_n.png'), optimize=True)
        orm = np.empty((1024, 1024, 3), dtype=np.float32)
        orm[:, :, 0] = 1
        orm[:, :, 1] = np.clip(roughness + (gray - gray.mean()) * .07, roughness - .04, roughness + .04)
        orm[:, :, 2] = metallic
        Image.fromarray(np.uint8(np.clip(orm, 0, 1) * 255 + .5)).save(maps / (stem + '_orm.png'), optimize=True)
        sources.append(dict(path='source-art/' + source.name, sha256=sha(source), size=original.size))
        materials.append(dict(type=kind, id=identity, maps=['d', 'n', 'orm'], size=[1024, 1024], roughness=roughness, metallic=metallic))
    manifest = dict(name='Enhanced Original ICC', version=1, playfield=655, status='private-test-only-rights-and-live-fit-review-pending',
        provider='built-in imagegen; separate prompt and immutable generated PNG per identity',
        redistributionApproved=False, originalPixelsIncluded=False, originalReferencesUsed=True,
        sources=sources, materials=materials, files={p.name: sha(p) for p in sorted(maps.glob('*.png'))})
    (ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Baked {len(materials)} materials, {len(manifest["files"])} maps; redistribution NOT approved.')

if __name__ == '__main__':
    main()
