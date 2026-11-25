"""Automatic material-edge blending must retain narrow authored route shapes."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
binary_hashes={name:sha(a.build/name) for name in ['rvk_demo.exe','ao-vk.dll']}
base=['--shared-terrain-test','--terrain-pebbles','--terrain-mask','--terrain-lighting-fixture','--ground-rocks','0','--ground-pebbles','0','--sky-zone','566','--area-zone','566','--pbr-debug','23','--size','768','512','--frames','160','--fixed-clock','1000']
images=[]
for name,extra in [('original',['--terrain-blend','0','--hd-ground','0']),('hd',['--terrain-blend','2','--hd-ground','1'])]:
 d=out/name;d.mkdir(exist_ok=True)
 with (d/'run.log').open('w') as f:subprocess.run([str((a.build/'rvk_demo.exe').resolve()),*base,*extra,'--shot','result.bmp'],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
 images.append(np.asarray(Image.open(d/'result.bmp').convert('RGB'),dtype=float));print(name,'rendered',flush=True)
def classes(c):
 r,g,b=c.transpose(2,0,1);lum=.3*r+.59*g+.11*b
 return np.where((g-np.maximum(r,b))/np.maximum(lum,1)>.09,1,np.where((r-b)/np.maximum(lum,1)>.18,2,3))
original,hd=[classes(c)[40:470,170:600] for c in images];route=original==2;painted=hd==2
recall=float((route&painted).sum()/route.sum());iou=float((route&painted).sum()/(route|painted).sum());area=float(painted.sum()/route.sum())
assert recall>.98 and iou>.97 and .97<area<1.03,('Authored route changed',recall,iou,area)
# Grass/gravel touching at old square boundaries deliberately blends now; its
# exact old pixel class is not an invariant. The winding route remains one.
assert binary_hashes=={name:sha(a.build/name) for name in binary_hashes},'Renderer binaries changed during path run'
report=dict(result='passed',executableSha256=binary_hashes['rvk_demo.exe'],binarySha256=binary_hashes,pathRecall=recall,pathIntersectionOverUnion=iou,pathAreaRatio=area,allMaterialClassAgreement=float((original==hd).mean()),retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
