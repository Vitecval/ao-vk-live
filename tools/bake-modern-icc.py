#!/usr/bin/env python3
"""Bake generated ICC source art into conservative, reproducible runtime materials.

Requires Pillow and NumPy. No original game assets are needed or redistributed.
Only fine surface detail becomes bump; painted regions never become displacement.
"""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1] / 'material-packs' / 'ao-modern-icc'
SURFACES = [
    (295992, 'ICC concrete', (1024, 1024), .91),
    (297326, 'ICC ivory wall panel', (1024, 1024), .76),
    (297327, 'ICC segmented ivory wall panel', (1024, 1024), .78),
    (296442, 'ICC building bottom trim', (1024, 256), .77),
    (295444, 'ICC shuttle grille atlas', (1024, 1024), .79),
    (295502, 'ICC shuttle base plate', (1024, 1024), .88),
]


def save(path, values):
    Image.fromarray(np.uint8(np.clip(values, 0, 1) * 255 + .5)).save(path, optimize=True)


def main():
    output = ROOT / 'maps'
    output.mkdir(exist_ok=True)
    materials = []
    for texture_id, name, size, roughness in SURFACES:
        stem = f'1010004_{texture_id}'
        art = Image.open(ROOT / 'source-art' / f'{stem}.png').convert('RGB').resize(size, Image.Resampling.LANCZOS)
        art.save(output / f'{stem}_d.png', optimize=True)
        rgb = np.asarray(art, dtype=np.float32) / 255
        lum = np.asarray(art.convert('L'), dtype=np.float32) / 255
        smooth = np.asarray(art.convert('L').filter(ImageFilter.GaussianBlur(2)), dtype=np.float32) / 255
        # Remove macro colour/paint differences before deriving micro-bump. No height map:
        # neither ivory paint nor brown backing is inferred to be a raised sheet.
        detail = np.clip(lum - smooth, -.06, .06)
        dx = (np.roll(detail, -1, axis=1) - np.roll(detail, 1, axis=1)) * .65
        dy = (np.roll(detail, -1, axis=0) - np.roll(detail, 1, axis=0)) * .65
        normal = np.stack((-dx, dy, np.ones_like(dx)), axis=2)
        normal /= np.linalg.norm(normal, axis=2, keepdims=True)
        save(output / f'{stem}_n.png', normal * .5 + .5)  # OpenGL tangent-space convention
        metal = np.zeros_like(lum)
        # Hand-authored broad UV regions: exposed grey frames/slats, never ivory paint
        # or the brown powder coat. Keep metal conservative to avoid mirror-like AO.
        yy, xx = np.mgrid[:size[1], :size[0]]
        u, v = (xx + .5) / size[0], (yy + .5) / size[1]
        neutral = (np.max(rgb, axis=2) - np.min(rgb, axis=2)) < .09
        if texture_id in (297326, 297327):
            frame = (u < .13) | (u > .87) | (v < .075) | (v > .94)
            metal[frame & neutral & (lum > .18) & (lum < .58)] = .45
        elif texture_id == 296442:
            metal[neutral & (v > .64) & (lum > .24)] = .5
        elif texture_id == 295444:
            slats = ((u > .11) & (u < .51) & (v < .81)) | ((u > .54) & (v < .71))
            metal[slats & neutral & (lum > .26)] = .45
        rough = np.clip(roughness + detail * .8 - metal * .28, .48, .96)
        # Tiny micro-occlusion only; no baked lighting or large artificial darkening.
        ao = np.clip(1 + np.minimum(detail, 0) * .35, .975, 1)
        save(output / f'{stem}_orm.png', np.stack((ao, rough, metal), axis=2))
        materials.append({'type': 1010004, 'id': texture_id, 'name': name, 'size': list(size),
                          'maps': ['albedo', 'normal', 'occlusion-roughness-metallic']})
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.glob('*.png'))}
    (ROOT / 'manifest.json').write_text(json.dumps({'name': 'ao-modern-icc', 'version': 1,
        'setting': 'RVK_ModernTex', 'default': False, 'materials': materials, 'files': files}, indent=2) + '\n')
    print(f'Baked {len(materials)} materials, {len(files)} maps, {sum(p.stat().st_size for p in output.glob("*.png")) / 1048576:.2f} MiB')


if __name__ == '__main__':
    main()
