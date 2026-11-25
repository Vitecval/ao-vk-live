"""Natural HD continuity must not depend on per-identity replacement residency."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image,ImageFilter
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
binary_hashes={name:sha(a.build/name) for name in ['rvk_demo.exe','ao-vk.dll']}
base=['--shared-terrain-test','--terrain-blend','2','--terrain-pebbles','--ground-rocks','0','--ground-pebbles','0','--sky-zone','566','--area-zone','566','--pbr-debug','23','--size','768','512','--frames','200','--fixed-clock','1000']
def run(name,extra):
 d=out/name;d.mkdir(exist_ok=True)
 with (d/'run.log').open('w') as f:subprocess.run([str((a.build/'rvk_demo.exe').resolve()),*base,*extra,'--shot','result.bmp'],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
 log=(d/'run.log').read_text(errors='replace');assert not re.search('VUID-|VK_ERROR|device lost|allocation failed',log,re.I),name
 if '--hd-ground' in extra and extra[extra.index('--hd-ground')+1]=='1':
  assert 'hd-masters=6' in (d/'terrain-frame.txt').read_text(),name+' missing HD upload'
 print(name,'rendered',flush=True)
 return np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(float)
mixed=run('mixed-private-legacy-missing',['--terrain-mixed-hd-fixture','1','--hd-ground','1'])
missing=run('all-individual-maps-missing',['--terrain-mixed-hd-fixture','2','--hd-ground','1'])
delta=float(np.abs(mixed-missing).max());assert delta<=1,('HD colour changes with replacement residency',delta)
legacy=run('legacy-separated-tiles',['--terrain-mixed-hd-fixture','1','--hd-ground','0'])
assert np.abs(mixed-legacy).mean()>3,'HD material did not replace legacy tile grain'
# View is almost orthographic from directly above: x=4/12m cell borders lie
# 118.24 pixels either side of the image centre. Ignore texture microdetail.
def seam_jump(array):
 smooth=np.asarray(Image.fromarray(array.astype('uint8')).filter(ImageFilter.GaussianBlur(3))).astype(float)
 return float(np.mean([np.abs(smooth[80:432,x+1]-smooth[80:432,x-1]).mean() for x in [266,502]]))
current_jump=seam_jump(mixed);legacy_jump=seam_jump(legacy)
assert current_jump<legacy_jump*.65,('grid boundary remains abrupt',current_jump,legacy_jump)
paving_on=run('manufactured-paving-hd',['--terrain-authored','--terrain-personal','--terrain-id','17','--hd-ground','1'])
paving_off=run('manufactured-paving-original',['--terrain-authored','--terrain-personal','--terrain-id','17','--hd-ground','0'])
paving_delta=float(np.abs(paving_on-paving_off).max());assert paving_delta<=1,('manufactured joints changed',paving_delta)
ice_on=run('specialized-ice-hd',['--terrain-authored','--terrain-personal','--terrain-id','706','--hd-ground','1'])
ice_off=run('specialized-ice-paint-control',['--terrain-authored','--terrain-personal','--terrain-id','706','--hd-ground','0'])
# A cell's interior retains the independently painted ice facets, with seam
# blending permitted only around borders. Soil masters must not replace ice.
ice_delta=float(np.abs(ice_on-ice_off)[180:210,310:340].max());assert ice_delta<=1,('ice painting replaced by granular ground',ice_delta)
assert binary_hashes=={name:sha(a.build/name) for name in binary_hashes},'Renderer binaries changed during seam run'
report=dict(result='passed',executableSha256=binary_hashes['rvk_demo.exe'],binarySha256=binary_hashes,replacementResidencyMaxPixelDelta=delta,hdBoundaryJump=current_jump,legacyBoundaryJump=legacy_jump,manufacturedPavingMaxPixelDelta=paving_delta,specializedIceInteriorMaxPixelDelta=ice_delta,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
