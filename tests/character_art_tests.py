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
    workers=json.loads((pack/'worker-catalog.json').read_text())
    count=14+len(completion)+len(workers)
    assert manifest['new_art_surfaces'] == count
    assert len(manifest['files']) == sum(len(x['maps']) for x in manifest['materials'])
    total = runtime_total = worker_total = worker_runtime = 0
    for name, expected in manifest['files'].items():
        assert hashlib.sha256((pack / 'maps' / name).read_bytes()).hexdigest() == expected, name
    for item in manifest['materials']:
        new_worker = item.get('batch','').startswith('preview21-worker-')
        assert {'d','n','orm'} <= set(item['maps']) <= {'d','n','orm','e'}
        if not new_worker: assert set(item['maps']) == {'d','n','orm','e'}
        stem = f"{item['type']}_{item['id']}"
        assert item['preserve_original_alpha']
        maps = {suffix: np.asarray(Image.open(pack / 'maps' / (stem + '_' + suffix + '.png')).convert('RGB'))
                for suffix in item['maps']}
        w, h = item['size']
        assert maps['d'].shape == (h, w, 3)
        mw,mh=item.get('material_map_size',[w//2,h//2])
        assert all(maps[suffix].shape == (mh,mw,3) for suffix in item['maps'] if suffix!='d')
        if not new_worker:
            # New response must separate metal and fabric rather than making every pixel shiny.
            orm = maps['orm'] / 255
            if item.get('worker'):
                # Worker equipment has explicit material regions, not the legacy
                # silver-armor heuristic. Keep the old assertions for the old art.
                assert orm[:,:,2].max()<=.53 and orm[:,:,1].min()>=.35,stem
                assert np.mean((orm[:,:,2]<.1)&(orm[:,:,1]>.7))>.02,stem
                if item['id'] in [224103,224107,224111,301966]:assert maps['e'].max()==0,stem
            elif item.get('profile')=='coated-armour':
                assert orm[:,:,2].max()<=.26 and orm[:,:,1].min()>=.56,stem
                assert np.mean((orm[:,:,2]<.1)&(orm[:,:,1]>.7))>.02,stem
                assert maps['e'].max()==0,stem
            else:
                assert np.mean(orm[:, :, 2] > .6) > .02, stem
                assert np.mean((orm[:, :, 2] < .1) & (orm[:, :, 1] > .7)) > .15, stem
            if item.get('worker') and item['id'] in [268223,268602]:
                # These two original atlases have glow-alpha coverage above 4%.
                # Verify the actual bounded glow equation rather than applying the
                # legacy armor's sparse amber-LED assumption to a different mask.
                diffuse=Image.open(pack/'maps'/(stem+'_d.png'))
                size=(diffuse.width//2,diffuse.height//2)
                rgb=np.asarray(diffuse.convert('RGB').resize(size,Image.Resampling.BOX),dtype=np.float32)/255
                glow=np.asarray(diffuse.getchannel('A').resize(size,Image.Resampling.BOX),dtype=np.float32)/255*.35
                expected=np.uint8(np.clip(rgb*glow[:,:,None],0,1)*255+.5)
                assert np.array_equal(maps['e'],expected),stem
            else:assert np.mean(np.max(maps['e'], axis=2) > 0) < .04, stem
        else:
            # New sealed textiles/skin/metal atlases use reviewed material profiles,
            # rather than the old silver-armour or sparse-LED assumptions.
            provenance=item['provenance']
            assert len(provenance['handoff_sha256'])==64 and len(provenance['original_reference_sha256'])==64
            assert hashlib.sha256((pack/'source-art'/(item['source']+'.png')).read_bytes()).hexdigest()==provenance['source_sha256']
        normals = maps['n'] / 127.5 - 1
        if new_worker:
            assert normals[:,:,2].min()>0 and abs(np.linalg.norm(normals,axis=2)-1).max()<.035,stem
        else: assert np.min(normals[:, :, 2]) > .98, stem
        # Four full-resolution map mip chains, per independently created AO surface.
        material_total=sum(value.shape[0] * value.shape[1] * 4 * 4 / 3 for value in maps.values())
        material_runtime=0
        for value in maps.values():
            h,w=value.shape[:2];scale=min(1,512/max(w,h))
            material_runtime+=max(1,int(w*scale))*max(1,int(h*scale))*16/3
        if item.get('worker'):
            assert max(item['size'])<=2048 and material_runtime<=4*512*512*16/3
            worker_total+=material_total;worker_runtime+=material_runtime
        else:total+=material_total;runtime_total+=material_runtime
    assert total < 80 * 1024 * 1024 and runtime_total < 48 * 1024 * 1024
    assert all(max(x['size'])<=512 and x['group']=='preserved-equipment' for x in completion)
    results = {'actual_repaints': count, 'baked_maps': len(manifest['files']), 'library_full_size_mip_mb': round(total / 1024**2, 1),'library_runtime_capped_mip_mb':round(runtime_total/1024**2,1)}
    results.update(worker_repaints=len(workers),worker_full_size_mip_mb=round(worker_total/1024**2,1),worker_runtime_capped_mip_mb=round(worker_runtime/1024**2,1))
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
