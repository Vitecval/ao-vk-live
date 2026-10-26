"""Verify the shared-prop batch's authored materials, light masks and base preservation."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'material-packs/ao-cyber-world'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path)
    args=parser.parse_args()
    cat=json.loads((PACK/'props-catalog.json').read_text())
    assert len(cat)==64 and len({x['id'] for x in cat})==64
    family={x['id']:x for x in json.loads((PACK/'family-catalog.json').read_text())}
    cat=[family.get(x['id'],x) for x in cat]
    def image(identity,suffix):
        return np.asarray(Image.open(PACK/'maps'/f'1010004_{identity}_{suffix}.png').convert('RGB'))
    def sample(identity,suffix,u,v):
        pixels=image(identity,suffix)
        return pixels[int(v*pixels.shape[0]),int(u*pixels.shape[1])]
    for identity in [55868,9698]:
        u,v=(.5,.5) if identity==55868 else (.68,.25)
        p=sample(identity,'orm',u,v)
        assert p[1]<25 and p[2]==0
        assert list(sample(identity,'n',u,v))==[128,128,255]
    assert sample(9698,'orm',.1,.1)[2]>190
    cushion=sample(6067,'orm',.25,.6)
    shell=sample(6067,'orm',.46,.45)
    assert cushion[1]>220 and cushion[2]==0 and shell[2]>190
    leather=image(6100,'orm')
    assert leather[:,:,1].min()>220 and leather[:,:,2].max()==0
    seat=sample(8750,'orm',.5,.71)
    table=sample(8757,'orm',.46,.56)
    assert seat[1]>210 and seat[2]==0 and table[1]>210 and table[2]==0
    wood=sample(296454,'orm',.5,.2)
    painted=sample(296454,'orm',.5,.8)
    assert wood[2]==0 and wood[1]>180
    assert painted[1]<160 and 10<painted[2]<30
    for identity in [13636,10606]:
        assert image(identity,'e').max()>0
        assert sample(identity,'e',.8,.8).max()==0
    for item in cat:
        assert image(item['id'],'d').shape[:2]==tuple(reversed(item['size']))
        assert (PACK/'source-art'/(item['source']+'.png')).is_file()
    preserved=0
    if args.baseline:
        baseline=json.loads(args.baseline.read_text())
        for name,digest in baseline['files'].items():
            if int(name.split('_')[1]) in family:continue
            assert hashlib.sha256((PACK/'maps'/name).read_bytes()).hexdigest()==digest,name
            preserved+=1
    print(json.dumps({'new_prop_repaints':64,'glass_metal_upholstery_separated':True,
                     'wood_and_painted_regions_separated':True,'emission_confined_to_lights':True,
                     'baseline_map_hashes_preserved':preserved},indent=2))

if __name__=='__main__':main()
