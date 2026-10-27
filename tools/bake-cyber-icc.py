#!/usr/bin/env python3
"""Bake authored ICC material profiles and generated artwork; requires Pillow + NumPy.

No proprietary originals are used. The catalog records material response separately
from albedo replacement, so coverage and new-art coverage can be audited independently.
"""
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1] / 'material-packs/ao-cyber-icc'
# Roughness / metallic. Natural surfaces are deliberately non-metallic.
PROFILES = {'metal': (.18, .9), 'panel': (.24, .72), 'painted_metal': (.30, .35),
    'grille': (.28, .75), 'bronze': (.23, .9), 'stone': (.48, 0), 'glass': (.14, 0),
    'cloth': (.9, 0), 'bark': (.84, 0), 'asphalt': (.30, 0), 'paving': (.28, .08),
    'deck': (.20, .85), 'grass': (.94, 0), 'gravel': (.82, 0), 'rock': (.76, 0),
    'rock_transition': (.80, 0)}


def save(path, values):
    Image.fromarray(np.uint8(np.clip(values, 0, 1) * 255 + .5)).save(path, optimize=True)


def main():
    output = ROOT / 'maps'
    output.mkdir(exist_ok=True)
    catalog = json.loads((ROOT / 'catalog.json').read_text())
    deck = json.loads((ROOT / 'deck-family.json').read_text())
    deck_shapes = {x['id']:x['shape'] for x in deck['members']}
    # This family keeps the original AO painting in every edition and preset.
    # Only parameter maps are published; an older generated repaint cannot win.
    catalog = [x for x in catalog if not (x['type']==1010006 and x['id'] in deck_shapes)]
    catalog += [{'type':1010006,'id':identity,'name':f'AO tread-plate family {identity}',
                 'group':'ground','profile':'matte-deck','source':None,'size':[128,128]}
                for identity in deck_shapes]
    expected = set()
    summary = []
    for item in catalog:
        stem = f"{item['type']}_{item['id']}"
        rough, metal = (.82,.70) if item['profile']=='matte-deck' else PROFILES[item['profile']]
        rough = item.get('roughness', rough)
        orm = np.empty((128, 128, 3), dtype=np.float32)
        orm[:] = (1, rough, metal)
        maps = ['orm']
        if item['profile']=='matte-deck':
            shape=deck_shapes[item['id']]
            y,x=np.mgrid[:128,:128];u=(x+.5)/128;v=(y+.5)/128
            square=(u>.25)&(u<.75)&(v>.25)&(v<.75)
            triangle=v>.5+np.abs(u-.5)
            def soft(value,width=.12):
                t=np.clip(.5+value/width,0,1)
                return t*t*(3-2*t)
            mask={'full':np.ones_like(u,dtype=bool),'square':square,'outside-square':~square,
                  'bottom-triangle':triangle,'outside-bottom-triangle':~triangle,
                  'upper-left':u+v<1,'lower-right':u+v>1,
                  'gravel-diamond':soft(.43-np.abs(u-.5)-np.abs(v-.5),.22),
                  'gravel-lower-right':soft(u+v-1,.24),
                  'gravel-upper-left':soft(1-u-v,.24),
                  'gravel-top-band':soft(v-.16,.2),
                  'gravel-right-band':soft(.86-u,.2)}[shape]
            orm[:,:,1]=.94+(.82-.94)*mask
            orm[:,:,2]=.70*mask
        if item['source']:
            art = Image.open(ROOT / 'source-art' / (item['source'] + '.png')).convert('RGB')
            art = art.resize(tuple(item['size']), Image.Resampling.LANCZOS)
            art.save(output / (stem + '_d.png'), optimize=True)
            rgb = np.asarray(art, dtype=np.float32) / 255
            grey = art.convert('L')
            lum = np.asarray(grey, dtype=np.float32) / 255
            radius = 8 if item['profile'] == 'bark' else 2
            smooth = np.asarray(grey.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float32) / 255
            detail = np.clip(lum - smooth, -.15, .15)
            strength = 2.2 if item['profile'] == 'bark' else 1.3 if item['profile'] in ('asphalt', 'stone', 'grass') else .65
            strength = item.get('normal_strength', strength)
            dx = (np.roll(detail, -1, axis=1) - np.roll(detail, 1, axis=1)) * strength
            dy = (np.roll(detail, -1, axis=0) - np.roll(detail, 1, axis=0)) * strength
            normal = np.stack((-dx, dy, np.ones_like(dx)), axis=2)
            normal /= np.linalg.norm(normal, axis=2, keepdims=True)
            save(output / (stem + '_n.png'), normal * .5 + .5)
            maps += ['d', 'n']
            small = np.asarray(art.resize((128, 128), Image.Resampling.BOX), dtype=np.float32) / 255
            bright = small.mean(axis=2)
            saturation = small.max(axis=2) - small.min(axis=2)
            if item['profile'] in ('metal', 'panel', 'painted_metal'):
                # Brushed silver regions and their graphite frames are different materials.
                silver = (bright > .3) & (saturation < .16)
                orm[:, :, 2] = np.where(silver, .90, min(metal, .4))
                orm[:, :, 1] = np.where(silver, .18, max(rough, .34))
            if item['profile'] == 'paving':
                y, x = np.mgrid[:128, :128]
                edge = (x < 5) | (x > 122) | (y < 5) | (y > 122)
                orm[edge, 1] = .16
                orm[edge, 2] = .88
            if item['profile'] == 'asphalt':
                orm[:, :, 1] = np.clip(.34 + (bright - bright.mean()) * .6, .24, .48)
            if item['profile'] == 'bark':
                save(output / (stem + '_h.png'), .5 + detail * .18)
                maps.append('h')
            # Encode the specifically painted light inlays as emitted light. Saturated
            # dark teal paint and neutral metal remain non-emissive.
            cyan = (rgb[:, :, 0] < .32) & (rgb[:, :, 1] > .62) & (rgb[:, :, 2] > .62)
            amber = (rgb[:, :, 0] > .78) & (rgb[:, :, 1] > .35) & (rgb[:, :, 2] < .25)
            if item['type'] == 1010004 and item['profile'] != 'bark' and (cyan | amber).any():
                emission = rgb * (cyan | amber)[:, :, None]
                save(output / (stem + '_e.png'), emission)
                maps.append('e')
        save(output / (stem + '_orm.png'), orm)
        expected.update(stem + '_' + suffix + '.png' for suffix in maps)
        summary.append({**item, 'maps': maps})
    # Never silently ship stale output after a catalog change. Retain the unexpected
    # files for inspection and fail instead of deleting author work.
    # Keep the old master-derived repaint for provenance, but never publish it.
    retired = {'1010006_17_d.png','1010006_17_n.png'} | {f'1010006_{i}_n.png' for i in deck_shapes}
    unexpected = {p.name for p in output.glob('*.png')} - expected - retired
    if unexpected:
        raise RuntimeError(f'Unexpected old baked maps: {sorted(unexpected)}')
    files = {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in sorted(expected)}
    replaced = sum('d' in item['maps'] for item in summary)
    (ROOT / 'manifest.json').write_text(json.dumps({'name': 'ao-cyber-icc', 'version': 1,
        'setting': 'RVK_ModernTex=2', 'materials': summary, 'new_art_surfaces': replaced, 'files': files}, indent=2) + '\n')
    print(f'{len(summary)} material assignments, {replaced} new-art surfaces, {len(files)} baked maps')


if __name__ == '__main__':
    main()
