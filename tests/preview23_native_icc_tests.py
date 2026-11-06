"""Exercise the real Shared Ground+ native tile-map gloss classification."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--maps',type=Path,required=True);p.add_argument('--cache',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];exe=(a.build/'rvk_demo.exe').resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.cache.resolve()));rows=[]
    for style,maps in [('anarchyhd',a.maps.resolve()),('enhanced',root/'material-packs/ao-icc-enhanced-original/maps')]:
        for identity in [21,22]:
            images=[]
            for gloss in [0,1]:
                d=out/f'{style}-{identity}-{gloss}';d.mkdir(exist_ok=True)
                command=[str(exe),'--shared-terrain-test','--shared-terrain-pebbles','--terrain-authored','--terrain-personal','--terrain-id',str(identity),'--shared-ground-maps',str(maps),'--terrain-blend','2','--sky-zone','655','--pbr-debug','2','--icc-gloss',str(gloss),'--size','512','384','--frames','8','--shot','result.bmp']
                with (d/'run.log').open('w') as f:subprocess.run(command,cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
                images.append(np.asarray(Image.open(d/'result.bmp')).astype(int))
            delta=abs(images[0]-images[1]);assert delta.max()>20,(style,identity,'native calibration absent')
            rows.append(dict(style=style,id=identity,max_pixel_delta=int(delta.max())))
    result=dict(result='passed',scope='Synthetic Shared Ground+ native tile map/PBR debug fixtures; not captured gameplay',cases=rows)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(result)

if __name__=='__main__':main()
