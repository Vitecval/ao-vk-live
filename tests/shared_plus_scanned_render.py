"""Replay captured native tile families against independently built077; no live-frame claim."""
from pathlib import Path
import hashlib,json,os,re,subprocess,shutil
import numpy as np
from PIL import Image
r=Path.cwd();out=r/'build/environment-078-r70/scanned-shared077-check';out.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
exes={k:Path('F:/AO-VK-D2/shared-plus-scanned-r70')/k/'shared_plus_scanned_scene.exe' for k in ['reference','current']};hashes={k:sha(v) for k,v in exes.items()}
cache=out/'pipelines.cache';shutil.copyfile(r/'build/environment-078-r70/shared077-render-check/pipelines.cache',cache)
rows=[];images={};paintCounts={}
for scan in ['143644','143711']:
 for hd in [0,1]:
  for paint in [False,True]:
   key=scan+'-hd'+str(hd)+('-paint' if paint else '-native')
   pair={}
   for label,exe in exes.items():
    d=out/key/label;d.mkdir(parents=True,exist_ok=True)
    command=[str(exe),'--scan',str(r/f'build/environment-078-r70/scanned-originals/scan-{scan}'),'--albedo-only','--hd-ground',str(hd),'--output','frame.bmp','--frames','100']
    if paint:command+=['--paints',str(r/'material-packs/ao-penumbra-078/maps')]
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache),VK_LOADER_LAYERS_DISABLE='~implicit~')
    if label=='current' and hd==0 and not paint:env.update(VK_LAYER_PATH=str(r/'build/deps/vulkan-validation-1.4.321/build-x86/layers'),VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
    with (d/'run.log').open('w') as log:subprocess.run(command,cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
    log=(d/'run.log').read_text(errors='replace');assert not re.search(r'VUID-|Validation Error|SYNC-HAZARD|device lost',log,re.I)
    count=re.search(r'attached-paints=(\d+)',log);assert count and (int(count[1])>10 if paint else int(count[1])==0);paintCounts[key+'/'+label]=int(count[1])
    files=list(d.glob('*-albedo.bmp'));assert len(files)==1,(key,label,'No actual material readback',files)
    a=np.asarray(Image.open(files[0]).convert('RGB'),dtype=float);assert a.std()>8 and np.count_nonzero(a)>200000,(key,label,'Blank surface target')
    pair[label]=a;assert sha(exe)==hashes[label]
   delta=np.abs(pair['current']-pair['reference']);row=dict(case=key,maximum=float(delta.max()),mean=float(delta.mean()),changedPixels=int((delta.max(axis=2)>1).sum()));print(row,flush=True);rows.append(row);images[key]=pair['current']
   (out/'MEASUREMENTS.json').write_text(json.dumps(rows,indent=2)+'\n')
   assert row['maximum']<=1 and row['mean']<.05,('Scanned surface differs from077',row)
 for hd in [0,1]:assert np.max(np.abs(images[scan+'-hd'+str(hd)+'-native']-images[scan+'-hd'+str(hd)+'-paint']))<=1,'Side-loaded paintings changed nonsnow Shared+'
 assert np.count_nonzero(np.abs(images[scan+'-hd0-native']-images[scan+'-hd1-native']).max(axis=2)>2)>1000,'HD setting has no effect'
result=dict(result='passed',executableSha256=hashes,cases=rows,attachedPaintCounts=paintCounts,sourceNativeTiles=76,capturedCellCountPerScene=441,hdOffAndOnCompared=True,sideLoadedPaintingsRejected=True,geometry='Original per-cell AO quads, no GroundOn replacement',actualRetailFrameReplay=False,limitations='Local geometry rebase changes procedural phase; controlled top-down camera/white-light/alpha255; no live lighting or camera orientation claim')
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
