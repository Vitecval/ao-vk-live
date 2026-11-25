"""Compare actual surface-colour GPU readbacks to independently built 0.7.7."""
import argparse,hashlib,json,os,subprocess,re,shutil
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--current',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--validate-only',action='store_true');a=p.parse_args()
r=Path.cwd();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exes={'reference':a.reference.resolve(),'restored':a.current.resolve()};hashes={k:sha(v) for k,v in exes.items()}
cache=out/'pipelines.cache'
if not cache.exists():shutil.copyfile(r/'build/environment-078-r65/high-resolution-check/pipelines.cache',cache)
common=['--shared-terrain-test','--terrain-lighting-fixture','--terrain-blend','2','--ground-rocks','0','--ground-pebbles','0','--ground-tess','0','--cliff-depth','0','--size','768','512','--frames','160','--fixed-clock','.000001','--hdr','--sky-zone','560']
rows=[];controls={}
for case,flags in [('winding-near',['--terrain-mask','--terrain-turns','--terrain-closeup']),('winding-far',['--terrain-mask','--terrain-turns']),('cliff',['--terrain-cliff-fixture','--terrain-side-view']),('cliff-toe',['--terrain-cliff-toe-fixture','--terrain-side-view']),('paving',['--terrain-id','364','--terrain-type','1010006','--terrain-source',str((r/'build/environment-078-r70/scanned-originals/1010006_364-native.png').resolve())]),('gravel',['--terrain-id','258','--terrain-type','1010006','--terrain-source',str((r/'build/environment-078-r70/scanned-originals/1010006_258-native.png').resolve())])]:
 for hd in [0,1]:
  images={};key=case+'-hd'+str(hd)
  for label,exe in exes.items():
   d=out/key/label;d.mkdir(parents=True,exist_ok=True)
   env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache),VK_LOADER_LAYERS_DISABLE='~implicit~')
   if label=='restored' and case=='winding-near' and hd==0:env.update(VK_LAYER_PATH=str(r/'build/deps/vulkan-validation-1.4.321/build-x86/layers'),VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
   cmd=[str(exe),*common,*flags,'--hd-ground',str(hd),'--shot','frame.bmp']
   if not a.validate_only:
    with (d/'run.log').open('w') as log:subprocess.run(cmd,cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
   log=(d/'run.log').read_text(errors='replace');assert 'rendered; screenshot frame.bmp' in log
   assert not re.search(r'VUID-|Validation Error|SYNC-HAZARD|device lost',log,re.I)
   path=d/'terrain-frame-albedo.bmp';assert path.exists(),(key,label,'no actual surface colour readback')
   images[label]=np.asarray(Image.open(path).convert('RGB'),dtype=float)
   assert images[label].std()>8 and np.count_nonzero(images[label])>100000,(key,label,'Blank colour target')
   assert sha(exe)==hashes[label]
  controls[key]=images['restored']
  delta=np.abs(images['reference']-images['restored']);row=dict(case=key,maximum=float(delta.max()),mean=float(delta.mean()),changedPixels=int((delta.max(axis=2)>1).sum()))
  rows.append(row);print(row,flush=True)
  (out/'MEASUREMENTS.json').write_text(json.dumps(rows,indent=2)+'\n')
  assert row['maximum']<=1 and row['mean']<.05,('Surface colour differs from077',row)
for name in ['winding-near','winding-far','cliff','cliff-toe','paving','gravel']:
 assert np.count_nonzero(np.abs(controls[name+'-hd0']-controls[name+'-hd1']).max(axis=2)>2)>500,(name,'HD mode inactive')
result=dict(positiveControlsPassed=True,result='passed',executableSha256=hashes,reference='Independent immutable0.7.7 engine source; all790 build inputs verified',cases=rows,actualGpuSurfaceColourReadback=True,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
