"""Penumbra patch geometry, Inferno legacy light colour and reduced molten glow."""
import argparse, json, os, re, subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--before',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def run(name,args,before=False):
    d=out/name;d.mkdir(exist_ok=True);exe=((a.before if before else a.build)/'rvk_demo.exe').resolve()
    with (d/'run.log').open('w') as f:
        subprocess.run([str(exe),*args,'--size','768','512','--frames','100','--fixed-clock','.000001','--shot','result.bmp','--dump','frame.txt'],cwd=d,
            stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),check=True,timeout=360)
    assert not re.search(r'VUID-|VK_ERROR|device lost|allocation failed',(d/'run.log').read_text(errors='replace'),re.I),name
    print(name,'rendered',flush=True)
    return np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(float),d
snow=['--snow-authored-fixture','--terrain-blend','2','--ground-rocks','0','--ground-pebbles','0','--ground-ao','0','--ground-sun','0','--ground-tess','1','--cliff-depth','0','--snow-powder','0','--sky-zone','4321','--grass-combined']
shared,d=run('snow-shared',snow+['--ground-far-view'])
dump=(d/'native-frame.txt').read_text();assert 'geometry=native-patches' in dump and 'original-terrain-consumed=1' in dump
assert re.search(r'patches/frame [1-9]',dump),'Shared Terrain+ submitted no height-field patches'
native,d=run('snow-enhanced',snow+['--native-on','--ground-far-view'])
# Geometry silhouettes must match Enhanced Ground even though lighting/material controls differ.
background=shared[0,0];mask=lambda im:np.max(np.abs(im-background),axis=2)>1
coverage=int(np.count_nonzero(mask(shared)!=mask(native)))
assert coverage<20,('Penumbra geometry differs from Enhanced Ground',coverage)
far,d=run('snow-far',snow+['--ground-far-view'])
assert 'geometry=native-patches' in (d/'native-frame.txt').read_text()
assert far[180:350].std()>10,'distant hills lack detail'
legacy=['--visual-case','7','--visual-toggle','1','--inferno-legacy-lighting','--area-zone','4005','--soft-sky','--shadows']
red,d=run('inferno-legacy',legacy+['--area-lighting','0'])
grey,d=run('inferno-neutral',legacy+['--area-lighting','.6'])
region=(slice(340,500),slice(40,730));before=red[region];after=grey[region]
cast=lambda im:float(np.maximum(im[:,:,0]-(im[:,:,1]+im[:,:,2])*.5,0).mean())
oldcast,newcast=cast(before),cast(after)
assert newcast<oldcast*.65,('Inferno red cast insufficiently reduced',oldcast,newcast)
# Include the facades and cast shadow when measuring tonal contrast. RGB
# variance on one flat floor confounds the very colour cast being removed.
tone=grey[240:500,40:730].mean(axis=2)
assert tone.std()>10,'Inferno shadow/facade contrast flattened'
water=['--water-lake','--sky-zone','4005','--area-lighting','.6','--water-wall','--water-waves','0','--point-shadows','4','--pixel-lighting','--light-override','--shadows']
old,d0=run('lava-before',water,True);new,d1=run('lava-after',water)
glow0=np.asarray(Image.open(d0/'frame-glow.bmp').convert('RGB')).astype(float)
glow1=np.asarray(Image.open(d1/'frame-glow.bmp').convert('RGB')).astype(float)
assert glow1.mean()<glow0.mean()*.90,('lava bloom not reduced',glow0.mean(),glow1.mean())
hot=(old[:,:,0]>old[:,:,1]*1.4)&(old[:,:,0]>140)
assert hot.sum()>500 and new[hot].mean()<old[hot].mean(),'lava emission not reduced'
report=dict(result='passed',penumbraEnhancedGeometryCoverageDifference=coverage,redCastBefore=oldcast,redCastAfter=newcast,lavaGlowBefore=float(glow0.mean()),lavaGlowAfter=float(glow1.mean()),retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
