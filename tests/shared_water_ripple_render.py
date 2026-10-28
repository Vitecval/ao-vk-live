"""Keep normal water visible while Shared Ground+ masks added ripple controls.

Measure against a same-setting repeat: reflection/raster variation makes an
absolute all-pixel equality assertion unsuitable for this water scene.
"""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=Path('build/preview16/water'));a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);images={}
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(o/'pipeline.cache'))
for name,mode,amount in [('shared-0',1,0),('shared-2',1,2),('plus-0',2,0),('plus-2',2,2),('plus-repeat',2,2)]:
 d=o/name;d.mkdir(exist_ok=True)
 with (d/'run.txt').open('w') as log:subprocess.run([str(r/'build/windows-live/rvk_demo.exe'),'--water-lake','--sky-zone','655','--terrain-blend',str(mode),'--fixed-clock','0.001','--size','768','512','--frames','160','--water-ripples',str(amount),'--dump','water-frame.txt','--shot','result.bmp'],cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
 text=(d/'run.txt').read_text(errors='replace');assert 'rendered; screenshot result.bmp' in text
 assert not re.search(r'VK_ERROR|VUID-|device lost|Validation Error',text,re.I),name
 if mode==2:
  dump=(d/'water-frame.txt').read_text(errors='replace');assert 'native terrain: shared-plus 1' in dump and 'ripples=disabled' in dump,name
 images[name]=np.array(Image.open(d/'result.bmp').convert('RGB')).astype(float)
control=abs(images['shared-0']-images['shared-2']);difference=abs(images['plus-0']-images['plus-2']);repeat=abs(images['plus-2']-images['plus-repeat'])
def changed(v):return int((v.max(2)>2).sum())
assert changed(control)>5000,'Ripple positive control did not exercise water detail'
assert difference.mean()<=max(.05,repeat.mean()*2),'Ripple setting changed Plus beyond repeat variation'
assert changed(difference)<changed(control)*.025,'Plus retained the ordinary ripple pattern'
report={'result':'passed','standard_ripple_changed_pixels':changed(control),'plus_control_changed_pixels':changed(difference),'plus_repeat_changed_pixels':changed(repeat),'plus_control_mean_delta':float(difference.mean()),'plus_repeat_mean_delta':float(repeat.mean()),'mode_and_disabled_ripple_diagnostics_verified':True,'ordinary_water_visible':True,'pixel_identity_claimed':False,'in_game_visual_verification':False}
(o/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
