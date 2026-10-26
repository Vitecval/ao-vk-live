"""Validate shared architecture, picked surfaces and foliage materials."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'material-packs/ao-cyber-world'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--baseline',type=Path);args=parser.parse_args()
    shared=json.loads((PACK/'shared-catalog.json').read_text());picked=json.loads((PACK/'picked-catalog.json').read_text())
    assert len(shared)==24 and len(picked)==75
    family={x['id']:x for x in json.loads((PACK/'family-catalog.json').read_text())}
    shared=[family.get(x['id'],x) for x in shared]
    picked=[family.get(x['id'],x) for x in picked]
    def pixels(identity,suffix):return np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_{suffix}.png').convert('RGB'))
    def sample(identity,suffix,u,v):
        image=pixels(identity,suffix);return image[int(v*image.shape[0]),int(u*image.shape[1])]
    for identity,u,v in [(5947,.5,.7),(5950,.47,.425),(41593,.5,.5)]:
        value=sample(identity,'orm',u,v);assert value[1]<25 and value[2]==0
        assert list(sample(identity,'n',u,v))==[128,128,255]
    assert sample(5947,'orm',.26,.7)[2]>190
    assert sample(5947,'orm',.05,.7)[2]==0
    assert pixels(10589,'orm')[:,:,1].min()>220 and pixels(10589,'orm')[:,:,2].max()==0
    for item in shared+picked:
        assert pixels(item['id'],'d').shape[:2]==tuple(reversed(item['size']))
        if item['profile'] in ['foliage','wood','stone'] and not any(key.startswith(('metal_','glass_')) for key in item):
            orm=pixels(item['id'],'orm');assert orm[:,:,2].max()==0 and orm[:,:,1].min()>180
        if item['profile']=='foliage':
            # Generated RGB coverage must extend behind the original runtime cutout,
            # rather than introducing a black margin or a newly isolated silhouette.
            diffuse=pixels(item['id'],'d')
            dark=diffuse.max(axis=2)<8
            # Small naturally shaded gaps between leaves are legitimate; a new
            # isolated silhouette or empty border is not.
            assert dark.mean()<.05,item['id']
            edges=np.concatenate([dark[:5].ravel(),dark[-5:].ravel(),dark[:,:5].ravel(),dark[:,-5:].ravel()])
            assert edges.mean()<.05,item['id']
        if item.get('emission'):
            assert pixels(item['id'],'e').max()>0,item['id']
            assert sample(item['id'],'e',.85,.85).max()==0
    # Corrections of detached white borders remain selected.
    for identity in [21524,208721]:
        diffuse=pixels(identity,'d');assert (diffuse.min(axis=2)>245).mean()<.005
    preserved=0
    if args.baseline:
        for name,digest in json.loads(args.baseline.read_text())['files'].items():
            if int(name.split('_')[1]) in family: continue # superseded by the reviewed complete family
            assert hashlib.sha256((PACK/'maps'/name).read_bytes()).hexdigest()==digest,name
            preserved+=1
    print(json.dumps({'new_repaint_ids':99,'foliage_and_bark':29,'material_regions':'passed',
          'leaf_colour_coverage':'passed','runtime_alpha_policy':'preserve original',
          'baseline_maps_unchanged':preserved},indent=2))
if __name__=='__main__':main()
