"""Shared Ground+ must retain AO-only cliff cutouts, overhangs and diagonals."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
exe=(a.build/'rvk_demo.exe').resolve();images={};rows={}
for pass_name in ['combined','two-pass']:
 for mode in ['original','enhanced']+(['reconstructed-control'] if pass_name=='combined' else []):
  name=pass_name+'-'+mode;d=out/name;d.mkdir(exist_ok=True)
  args=[str(exe),'--retail-cliff-fixture','--ground-rocks','0','--ground-pebbles','0',
   '--ground-tess','0','--cliff-depth','0','--terrain-blend','0' if mode=='original' else '2',
   '--sky-zone','4605','--size','768','512','--fixed-clock','.000001','--frames','100','--shot','result.bmp']
  if pass_name=='combined':args+=['--grass-combined']
  if mode=='reconstructed-control':args+=['--native-on']
  with (d/'run.log').open('w') as f:
   subprocess.run(args,cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),
     stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
  log=(d/'run.log').read_text(errors='replace')
  assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
  images[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB'))
  if mode=='enhanced':
   dump=(d/'native-frame.txt').read_text()
   assert 'geometry=classic-submissions-HD-materials' in dump and 'original-terrain-consumed=0' in dump,name
  print(name,'rendered',flush=True)
 bg=images[pass_name+'-original'][0,0]
 assert int(bg[2])>int(bg[0])+20,('Fixture needs visible clear sky',bg.tolist())
 mask=lambda x:np.any(x!=bg,axis=2)
 baseline=mask(images[pass_name+'-original'])
 delta=int(np.count_nonzero(baseline!=mask(images[pass_name+'-enhanced'])))
 assert delta==0,(pass_name,'Enhanced materials changed the AO cliff outline/cutouts',delta)
 rows[pass_name]={'changedCoveragePixels':delta,'sourceSurfacePixels':int(baseline.sum())}
 if pass_name=='combined':
  changed=int(np.count_nonzero(baseline!=mask(images[pass_name+'-reconstructed-control'])))
  assert changed>500,('The control did not expose the reconstruction error',changed)
  rows[pass_name]['reconstructedControlWrongPixels']=changed
report=dict(result='passed',rendererSha256=hashlib.sha256((a.build/'ao-vk.dll').read_bytes()).hexdigest(),
 cases=rows,liveInfernoPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
