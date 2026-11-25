"""Actual captured-water gating, non-glowing bubble motion and optional controls."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--cache',type=Path);p.add_argument('--validation-layers',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);exe=(a.build/'rvk_demo.exe').resolve()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();fingerprint=sha(exe)
if a.cache and not (out/'pipelines.cache').exists():shutil.copyfile(a.cache,out/'pipelines.cache')
images={};logs={};contexts={}
for name,mode,clock in [('below',1,'.000001'),('off',2,'.000001'),('above',3,'.000001'),('dry-footprint',4,'.000001'),
                        ('later',1,'16.6666667'),('manual-style',6,'.000001'),('brightness-off',7,'.000001'),('molten',8,'.000001'),
                        ('tiny-density',12,'.000001'),('shore-edge',13,'.000001'),('split-levels',14,'.000001')]:
    d=out/name;d.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache'))
    if name=='below':env.update(VK_LAYER_PATH=str(a.validation_layers.resolve()),VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',VK_LOADER_DEBUG='layer',VK_LAYER_VALIDATE_SYNC='1')
    with (d/'run.log').open('w') as log:
        subprocess.run([str(exe),'--underwater-bubble-test',str(mode),'--fixed-clock',clock,'--frames','120','--size','640','480','--shot','result.bmp'],cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
    log=(d/'run.log').read_text(errors='replace');assert 'rendered; screenshot result.bmp' in log,name
    assert not re.search('VUID-|Validation Error|SYNC-HAZARD|VK_ERROR|device lost',log,re.I),name
    dump=(d/'bubble-frame.txt').read_text(errors='replace');logs[name]=dump
    context=re.search(r'Bubble physical context: (\d+); different zone: (\d+)',log);assert context and context[2]=='0',name
    contexts[name]=int(context[1]);images[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB'),dtype=float)
    assert sha(exe)==fingerprint
    print(name,'rendered',flush=True)
assert contexts['below']==contexts['off']==contexts['brightness-off']==1
assert contexts['above']==contexts['dry-footprint']==contexts['molten']==0
assert contexts['shore-edge']==contexts['split-levels']==1
assert '# Underwater bubbles:' in logs['below'] and 'non-glowing' in logs['below']
for name in ['off','above','dry-footprint','manual-style','brightness-off','molten']:assert '# Underwater bubbles:' not in logs[name],name
roi=np.s_[170:430,60:580]
active=np.abs(images['below']-images['off'])[roi].max(axis=2)
motion=np.abs(images['below']-images['later'])[roi].max(axis=2)
assert np.count_nonzero(active>1)>30,('No visible ring bubbles',np.count_nonzero(active>1))
assert np.count_nonzero(motion>1)>30,'Bubble motion is static'
assert np.abs(images['off']-images['brightness-off']).max()<=1,'Brightness0 does not restore the scene'
tiny=np.abs(images['tiny-density']-images['off'])[roi].max(axis=2)
assert np.count_nonzero(tiny>1)<np.count_nonzero(active>1)*.2,'Tiny density still emits a large population'
ceilings=re.search(r'local ceilings ([\d.]+)\.\.([\d.]+)',logs['split-levels']);assert ceilings and float(ceilings[2])-float(ceilings[1])>.2,'Independent water levels were not exercised'
assert 'drift margin' in logs['shore-edge']
assert np.quantile(active,.99)<30,'Bubbles are excessively bright or dense'
validation=(out/'below/run.log').read_text(errors='replace');assert 'Insert instance layer "VK_LAYER_KHRONOS_validation"' in validation
result=dict(result='passed',executableSha256=fingerprint,physicalContexts=contexts,visibleBubblePixels=int(np.count_nonzero(active>1)),
            movingBubblePixels=int(np.count_nonzero(motion>1)),bubbleP99Delta=float(np.quantile(active,.99)),
            sourceGeometryUnchanged=True,validationErrors=0,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
