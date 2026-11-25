"""Verify HD ground is a full colour painting and cliffs survive coarse AO normals."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--before',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def run(name,args,before=False):
 d=out/name;d.mkdir(exist_ok=True);exe=((a.before if before else a.build)/'rvk_demo.exe').resolve()
 with (d/'run.log').open('w') as f:subprocess.run([str(exe),*args,'--size','768','512','--frames','200','--fixed-clock','1000','--shot','result.bmp'],cwd=d,
  stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),check=True,timeout=360)
 log=(d/'run.log').read_text(errors='replace');assert not re.search(r'VUID-|VK_ERROR|device lost|allocation failed',log,re.I),name
 dump=(d/'terrain-frame.txt').read_text()
 assert ('detail-masters=6' if name=='old-detail' else 'hd-masters=6') in dump,name
 print(name,'rendered',flush=True);return np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(float)
base=['--shared-terrain-test','--terrain-blend','2','--hd-ground','1','--sky-zone','566','--area-zone','566','--terrain-pebbles','--ground-pebbles','0','--ground-rocks','0','--pbr-debug','18']
old=run('old-detail',base+['--ambient-life-fixture','3','--hd-ground','0'])
new=run('full-painting',base+['--ambient-life-fixture','3'])
region=np.s_[330:490,120:650]
assert np.abs(old-new)[region].mean()>3,'HD painting remains a bounded detail layer'
assert new[region].std()>3 and np.abs(old-new)[region].max()>12,'HD painting lacks visible material detail'
cliff=run('cliff',base+['--terrain-cliff-toe-fixture','--terrain-side-view'])
coarse=run('cliff-coarse-normals',base+['--terrain-cliff-toe-fixture','--terrain-side-view','--terrain-flat-normals'])
delta=float(np.abs(cliff-coarse).max());assert delta<=1,('coarse AO normals suppress cliff material',delta)
off=run('cliff-off',base+['--terrain-cliff-toe-fixture','--terrain-side-view','--terrain-off'])
assert np.count_nonzero(np.abs(cliff-off).max(axis=2)>5)>1000,'cliff material not visibly replacing original'
report=dict(result='passed',oldFloorContrast=float(old[region].std()),newFloorContrast=float(new[region].std()),coarseNormalMaxDelta=delta,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
