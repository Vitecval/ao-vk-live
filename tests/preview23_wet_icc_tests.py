"""Check the independent wet/legacy SSR mask on verified ICC fixture materials."""
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
        for kind,identity in [(1010006,21),(1010006,22),(1010004,295610)]:
            levels=[]
            for gloss in [0,.2,1]:
                d=out/f'{style}-{identity}-{gloss}';d.mkdir(exist_ok=True)
                if kind==1010006:
                    args=['--shared-terrain-test','--terrain-authored','--terrain-personal','--terrain-id',str(identity),'--shared-ground-maps',str(maps)]
                else:args=['--icc-floor-test','--pbr-maps',str(maps),'--shared-maps-control']
                command=[str(exe),*args,'--sky-zone','655','--ssr','1','--ssr-wet','1','--icc-gloss',str(gloss),'--fixed-clock','1000','--dump','wet-frame.txt','--size','512','384','--frames','8','--shot','result.bmp']
                with (d/'run.log').open('w') as f:subprocess.run(command,cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
                locals=sorted(d.glob('*local.bmp'));assert locals,(style,identity,'SSR mask capture missing')
                mask=np.asarray(Image.open(locals[-1]).convert('RGB'))[:,:,1]
                # The gallery's floor is below the spheres; the tile fixture is all floor here.
                roi=mask[330:380,100:400] if kind==1010004 else mask[160:300,190:320]
                levels.append(float(roi.mean()))
            assert levels[0]<levels[1]<levels[2],(style,identity,'wet reflections bypass the slider',levels)
            assert levels[0]<levels[2]*.3,(style,identity,'minimum is still mirror-like',levels)
            rows.append(dict(style=style,type=kind,id=identity,gloss_values=[0,.2,1],wet_ssr_mask_means=levels))
    result=dict(result='passed',scope='Controlled real HDR/SSR mask output with forced legacy wetness; not live rainfall gameplay',cases=rows)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(result)

if __name__=='__main__':main()
