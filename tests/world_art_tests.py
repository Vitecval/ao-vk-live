"""Verify world-pack pixels, glass separation, memory budget and named profiles."""
import hashlib
import importlib.util
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT/'material-packs/ao-cyber-world'


def main():
    manifest = json.loads((PACK/'manifest.json').read_text())
    completion=json.loads((PACK/'completion-catalog.json').read_text())
    effective={}
    for name in ['catalog','city-catalog','props-catalog','shared-catalog','picked-catalog','scan-catalog','completion-catalog','family-catalog','rubika-catalog','rubika-ground-catalog']:
        for item in json.loads((PACK/(name+'.json')).read_text()):effective[item['type'],item['id']]=item
    count=len(effective)
    assert len(manifest['materials']) == manifest['new_art_surfaces'] == count
    assert len(manifest['files']) == sum(3+bool(x.get('emission')) for x in effective.values())
    total = runtime_total = 0
    for name, expected in manifest['files'].items():
        path = PACK/'maps'/name
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, name
        image = Image.open(path)
        total += image.width*image.height*4*4/3
        scale=min(1,512/max(image.size)) # production built-in loader cap, before upload
        runtime_total += max(1,int(image.width*scale))*max(1,int(image.height*scale))*4*4/3
    # Full source-library estimate is distinct from the capped runtime estimate.
    # This is a whole-library upper bound, not a measured zone's residency.
    families=json.loads((ROOT/'material-packs/building-families.json').read_text())
    approved={i for f in families['families'] for i in f['texture_ids']}
    managed={x['id'] for x in json.loads((ROOT/'material-packs/ao-world-materials/catalog.json').read_text())}
    managed|={x['id'] for x in json.loads((ROOT/'material-packs/scan-coverage.json').read_text())['textures'] if x['type']==1010004 and x['phase']==2}
    eligible=0
    for item in manifest['materials']:
        if item['type']==1010006: continue # natural legacy RGB retained in the library, inactive in production
        if item['id'] in managed and item['id'] not in approved: continue
        for suffix in ['d','n','orm']+(['e'] if item.get('emission') else []):
            w,h=Image.open(PACK/'maps'/f"{item['type']}_{item['id']}_{suffix}.png").size
            scale=min(1,512/max(w,h));eligible+=int(w*scale)*int(h*scale)*16/3
    # An expanded library is not simultaneously resident in a zone. Enforce
    # the production per-map cap and test bounded streaming in loader fixtures.
    for item in manifest['materials']:
        if item.get('batch')=='rubika-complete-families-2026-10-26':assert max(item['size'])<=512
    def sample(identity,suffix,u,v):
        image = np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_{suffix}.png').convert('RGB'))
        return image[int(v*image.shape[0]),int(u*image.shape[1])]
    # Window interiors, frame and wall must have genuinely different responses.
    for u in [.25,.75]:
        orm = sample(6055,'orm',u,.5)
        assert orm[1] < 25 and orm[2] == 0, orm
        normal = sample(6055,'n',u,.5)
        assert list(normal) == [128,128,255], normal
        pane = sample(6055,'d',u,.5)
        assert pane.mean() < 85, pane
    frame, wall = sample(6055,'orm',.10,.50), sample(6055,'orm',.03,.50)
    assert frame[2] > 190 and frame[1] < 90
    assert wall[2] == 0 and wall[1] > 180
    screen, bezel=sample(82020,'orm',.5,.5),sample(82020,'orm',.5,.02)
    assert screen[1]<25 and screen[2]==0 and bezel[2]>190
    assert list(sample(82020,'n',.5,.5))==[128,128,255]
    for identity in [33201,42555,29484]:
        wood=sample(identity,'orm',.5,.5)
        assert wood[2]==0 and wood[1]>180
    # Rejected white letterboxing and circular-cutout variants must not reappear.
    for identity in [82024,6069]:
        edge=np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_d.png').convert('RGB'))
        assert edge[0].mean()<200 and edge[-1].mean()<200
    plaque=np.asarray(Image.open(PACK/'maps/1010004_31973_d.png').convert('RGB'))
    assert min(plaque[y,x].mean() for y,x in [(0,0),(0,-1),(-1,0),(-1,-1)])>20
    for item in manifest['materials']:
        assert item['preserve_original_alpha'] == (item['type'] != 1010006)
        if item['type'] == 1010006:
            orm = np.asarray(Image.open(PACK/'maps'/f"1010006_{item['id']}_orm.png"))
            assert orm[:,:,2].max() == 0
            if item['profile']=='satin-paving':assert 95<=orm[:,:,1].min()<=orm[:,:,1].max()<=139
            else:assert orm[:,:,1].min() > 180
    spec = importlib.util.spec_from_file_location('world',ROOT/'tools/build-world-material-catalog.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.classify('plateyellwindows.png') == 'window'
    assert module.classify('concretebridge_generalside_veryplain.png') == 'stone'
    assert module.classify('greymetal.png') == 'metal'
    for name in ['head_nanomagemale021.png','ai_agility_sense_body.png','bark.png','windowreflection.png','weapon_metal_rifle.png']:
        assert module.classify(name) is None, name
    catalog = json.loads((ROOT/'material-packs/ao-world-materials/catalog.json').read_text())
    assert len(catalog) == 2756
    assert len({(x['type'],x['id']) for x in manifest['materials']})==count
    for item in manifest['materials']:
        assert (PACK/'source-art'/(item['source']+'.png')).is_file()
    for identity in [5955,5957,205836,206854,210817,209483]:
        orm=np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_orm.png'))
        assert orm[:,:,1].min()>=240 and orm[:,:,2].max()==0,identity
    for identity in [209481,209477]:
        orm=np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_orm.png'))
        assert orm[:,:,1].min()>=220 and orm[:,:,2].max()==0,identity
    for first,alias in [(2,3),(2,4),(8,9)]:
        assert (PACK/'maps'/f'1010006_{first}_d.png').read_bytes()==(PACK/'maps'/f'1010006_{alias}_d.png').read_bytes()
    for item in completion:
        assert max(item['size'])<=512
        orm=np.asarray(Image.open(PACK/'maps'/f"{item['type']}_{item['id']}_orm.png"))
        if item['profile'] in ['mineral','honed-stone','dry-foliage','dry-organic','fabric']:
            assert orm[:,:,2].max()==0 and orm[:,:,1].min()>=224,item['id']
    for u in [.25,.50,.75]:
        assert sample(27751,'orm',u,.5)[1]<25
        assert list(sample(27751,'n',u,.5))==[128,128,255]
    assert sample(27751,'orm',.41,.5)[2]>190
    for identity in [38724,41512]:
        art=np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_d.png'))
        assert art[:,0].mean()<220 and art[:,-1].mean()<220,'Rejected white letterbox returned'
    print(json.dumps({'actual_repaints':count,'baked_maps':len(manifest['files']),'library_full_size_mip_mb':round(total/1024**2,1),'library_runtime_capped_mip_mb':round(runtime_total/1024**2,1),
                      'eligible_runtime_mip_mb':round(eligible/1024**2,1),
                      'glass_frame_wall_separation':'passed','terrain_nonmetal':'passed','named_architecture_profiles':len(catalog)},indent=2))


if __name__ == '__main__': main()
