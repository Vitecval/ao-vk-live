"""Shared Ground+: native terrain/clutter/LOD, classic lighting, no ripples.

Uses controlled rendered fixtures; does not claim live-game screenshot parity.
"""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build/windows-live'))
p.add_argument('--output',type=Path,default=Path('build/preview16/terrain'))
p.add_argument('--validate-only',action='store_true',help='Validate already rendered outputs without rerendering.')
p.add_argument('--reference',type=Path,help='Same ordinary Shared fixture rendered using the preserved previous renderer.')
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
exe=(a.build/'rvk_demo.exe').resolve()
common=['--native-ground-test','--ground-detail-fixture','--terrain-maps','--pixel-lighting','--sky-zone','655','--fixed-clock','1000','--size','768','512','--frames','480']
cases={
 'shared':['--terrain-blend','1'],
 'plus':['--terrain-blend','2'],
 'groundon':['--native-on','--terrain-blend','0'],
 'groundon-overrides':['--native-on','--terrain-blend','2'],
 'plus-no-extra-light':['--terrain-blend','2','--ground-ao','0','--ground-sun','0','--dune-light','0','--sand-ripple','0','--ripple-depth','0','--sand-glint','0'],
 'plus-no-rocks':['--terrain-blend','2','--ground-rocks','0'],
 'plus-no-pebbles':['--terrain-blend','2','--ground-pebbles','0'],
 'plus-no-cliffs':['--terrain-blend','2','--terrain-off'],
 'plus-far':['--terrain-blend','2','--ground-far-view'],
 'groundon-far':['--native-on','--terrain-blend','0','--ground-far-view'],
 'plus-far-no-cliffs':['--terrain-blend','2','--ground-far-view','--terrain-off'],
 'plus-night':['--terrain-blend','2','--sun-dir','0.55','0.3','0.45','--night-dark','0.8'],
 'plus-cycle':['--terrain-blend','2','--ground-mode-cycle'],
 'plus-unknown':['--terrain-blend','2','--native-unknown-pass'],
 'plus-separate':['--terrain-blend','2'],
}
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipeline.cache'))
metrics={};images={};timings={}
for name,extra in cases.items():
 dest=out/name;dest.mkdir(exist_ok=True)
 if not a.validate_only:
  with (dest/'run.txt').open('w') as log:
   subprocess.run([str(exe),*common,*([] if name=='plus-separate' else ['--grass-combined']),*extra,'--shot','result.bmp'],cwd=dest,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
 text=(dest/'run.txt').read_text(errors='replace');dump=(dest/'native-frame.txt').read_text(errors='replace')
 assert 'rendered; screenshot result.bmp' in text,name
 assert not re.search(r'VUID-|VK_ERROR|device lost|AddressSanitizer|Validation Error',text,re.I),name
 m=re.search(r'native terrain: shared-plus (\d+); enabled (\d+); patches (\d+); patches/frame ([\d.]+); rocks (\d+); pebbles (\d+); AO ([\d.]+); sun ([\d.]+)',dump)
 assert m,name
 metrics[name]=dict(zip(['plus','enabled','patches','patches_per_frame','rocks','pebbles','ao','sun'],map(float,m.groups())))
 images[name]=np.array(Image.open(dest/'result.bmp').convert('RGB')).astype(int)
 if name.startswith('plus') and name!='plus-unknown':
  assert 'lighting=classic-AO-baked-lightmap' in dump and 'ripples=disabled' in dump,name
  assert metrics[name]['ao']==metrics[name]['sun']==0,name
 (out/'progress.json').write_text(json.dumps(metrics,indent=2))
 print(name,'passed',flush=True)

def changed(a,b):return int(np.any(abs(images[a]-images[b])>2,axis=2).sum())
def delta(a,b):return int(abs(images[a]-images[b]).max())
assert metrics['shared']['patches_per_frame']==0 and metrics['shared']['rocks']==0
for n in ['plus','groundon','plus-no-extra-light','plus-no-rocks','plus-no-pebbles','plus-far','groundon-far','plus-cycle','plus-separate']:
 assert metrics[n]['patches_per_frame']>0,n
assert metrics['plus']['patches_per_frame']==metrics['groundon']['patches_per_frame'],'Different native terrain submissions'
assert metrics['plus']['rocks']==metrics['groundon']['rocks']>0,'Missing or duplicated native boulders'
assert metrics['plus']['pebbles']==metrics['groundon']['pebbles']>0,'Missing or duplicated pebbles'
assert metrics['plus-no-rocks']['rocks']==0
assert metrics['plus-no-pebbles']['pebbles']==0
assert metrics['plus-unknown']['patches_per_frame']==0,'Unknown ground passes must fall back safely'
assert delta('plus','plus-no-extra-light')<=1,'Native AO/sun/ripple controls leaked into classic mode'
assert delta('groundon','groundon-overrides')<=1,'GroundOn comparison mode changed with shared setting'
assert changed('plus','plus-no-rocks')>100,'Native boulders not visible'
assert changed('plus','plus-no-pebbles')>100,'Pebbles not visible'
assert changed('plus','plus-no-cliffs')>100,'Cliff material not visible'
assert changed('plus-far','plus-far-no-cliffs')>100,'Distant cliff material lost'
assert images['plus-night'].mean()<images['plus'].mean(),'Night lighting lost'
assert delta('plus','plus-cycle')<=1,'Mode cycle did not restore the same terrain'
pass_difference=abs(images['plus']-images['plus-separate'])
# Two-pass depth-equal rasterization can move a few silhouette samples. Check
# the lighting over the image, without mistaking isolated rock edges for a
# whole-surface brightness regression.
assert pass_difference.mean()<0.3 and np.percentile(pass_difference,99)<=2 and (pass_difference.max(2)>3).mean()<.0001,'Combined/separate terrain lighting differs'
baseline=a.reference
ordinary=None
if baseline and baseline.exists():
 ordinary=int(abs(np.array(Image.open(baseline).convert('RGB')).astype(int)-images['shared']).max())
 assert ordinary<=1,('Standard Shared Ground regression',ordinary)
report={'result':'passed','metrics':metrics,'visible_rock_pixels':changed('plus','plus-no-rocks'),'visible_pebble_pixels':changed('plus','plus-no-pebbles'),'visible_cliff_pixels':changed('plus','plus-no-cliffs'),'distant_cliff_pixels':changed('plus-far','plus-far-no-cliffs'),'classic_control_max_delta':delta('plus','plus-no-extra-light'),'combined_separate_max_delta':delta('plus','plus-separate'),'ordinary_shared_baseline_max_delta':ordinary,'in_game_visual_verification':False}
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
