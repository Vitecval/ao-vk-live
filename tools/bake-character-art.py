#!/usr/bin/env python3
"""Bake the generated character artwork into albedo, normal, ORM and emissive maps.

Requires Pillow and NumPy. Does not read or redistribute original AO pixel data.
The renderer samples original coverage/glow alpha from the player's live texture.
"""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1] / 'material-packs/ao-cyber-characters'


def save(path, values):
    image = Image.fromarray(np.uint8(np.clip(values, 0, 1) * 255 + .5))
    # Material maps do not need albedo resolution. Keep crowd/quality copies from
    # multiplying avoidable GPU and staging allocations in the retail client.
    image = image.resize((image.width // 2, image.height // 2), Image.Resampling.BOX)
    image.save(path, optimize=True)


def main():
    output = ROOT / 'maps'
    output.mkdir(exist_ok=True)
    catalog = json.loads((ROOT / 'catalog.json').read_text())
    completion = ROOT / 'completion-catalog.json'
    if completion.exists():
        catalog += json.loads(completion.read_text())
    if len({(x['type'], x['id']) for x in catalog}) != len(catalog):
        raise RuntimeError('Duplicate resource identity in equipment catalogs')
    files, materials = {}, []
    for item in catalog:
        stem = f"{item['type']}_{item['id']}"
        art = Image.open(ROOT / 'source-art' / (item['source'] + '.png')).convert('RGB')
        art = art.resize(tuple(item['size']), Image.Resampling.LANCZOS)
        art.save(output / (stem + '_d.png'), optimize=True)
        rgb = np.asarray(art, dtype=np.float32) / 255
        grey = art.convert('L')
        lum = np.asarray(grey, dtype=np.float32) / 255
        smooth = np.asarray(grey.filter(ImageFilter.GaussianBlur(2)), dtype=np.float32) / 255
        # Only small scale detail contributes relief: painted broad highlights must
        # not become giant bumps. Avoid wraparound slopes across disconnected UV islands.
        detail = np.clip(lum - smooth, -.12, .12)
        dy, dx = np.gradient(detail)
        normal = np.stack((-dx * .7, dy * .7, np.ones_like(dx)), axis=2)
        normal /= np.linalg.norm(normal, axis=2, keepdims=True)
        save(output / (stem + '_n.png'), normal * .5 + .5)
        # Neutral silver plates get metal response. Dark carbon weave and rubber
        # remain rough dielectrics; do not turn all black areas into polished steel.
        neutral = 1 - np.clip((rgb.max(axis=2) - rgb.min(axis=2)) / .18, 0, 1)
        metal = np.clip((lum - .21) / .21, 0, 1) * neutral
        rough = .83 * (1 - metal) + .29 * metal
        if item.get('profile') == 'coated-armour':
            # Bronze/painted panels are not neutral silver; black flexible
            # joints stay matte. No emission inferred from orange paint.
            panel = np.clip((lum-.12)/.22,0,1)
            metal = panel*float(item.get('metallic',.25))/.90
            rough = .84*(1-panel)+float(item.get('roughness',.57))*panel
        ao = np.clip(.98 + detail * .35, .92, 1)
        save(output / (stem + '_orm.png'), np.stack((ao, rough, metal * .90), axis=2))
        amber = (rgb[:, :, 0] > .74) & (rgb[:, :, 1] > .32) & (rgb[:, :, 2] < .24)
        if item.get('profile') == 'coated-armour': amber[:] = False
        save(output / (stem + '_e.png'), rgb * amber[:, :, None] * .55)
        suffixes = ['d', 'n', 'orm', 'e']
        for suffix in suffixes:
            name = stem + '_' + suffix + '.png'
            files[name] = hashlib.sha256((output / name).read_bytes()).hexdigest()
        materials.append({**item, 'maps': suffixes, 'preserve_original_alpha': True,
                          'material_map_size': [item['size'][0] // 2, item['size'][1] // 2],
                          'review': 'atlas-reviewed; live model fit pending'})
    unexpected = {p.name for p in output.glob('*.png')} - files.keys()
    if unexpected:
        raise RuntimeError(f'Unexpected baked maps: {sorted(unexpected)}')
    manifest = {'name': 'ao-cyber-characters', 'version': 2,
                'setting': 'RVK_AutoMat=1; RVK_MatStrength>0; RVK_Enhance=1',
                'new_art_surfaces': len(materials), 'materials': materials, 'files': files}
    (ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'{len(materials)} actual repaints, {len(files)} material maps')


if __name__ == '__main__':
    main()
