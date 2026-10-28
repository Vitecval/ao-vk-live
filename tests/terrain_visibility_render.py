"""Native terrain coverage is independent of the first retail patch's alpha.

The void fixture reserves one tile as the reviewed AO Brink marker, 1010006:774.
Compare camera angles, transparent/opaque first chunks and actual index counts;
this is a controlled GPU regression, not certification of live-game parity.
"""
import pathlib,subprocess,os,json,re
from PIL import Image
import numpy as np
import argparse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cases={'opaque':[], 'transparent':['--native-first-transparent'], 'void-opaque':['--native-void-fixture'], 'void-transparent':['--native-void-fixture','--native-first-transparent'],'yaw-opaque':['--terrain-yaw','0.6'], 'yaw-transparent':['--terrain-yaw','0.6','--native-first-transparent']}
ims={};rep={}
for name,args in cases.items():
 d=out/name;d.mkdir(exist_ok=True)
 with (d/'run.txt').open('w') as f:
  subprocess.run([str(root/'build/windows-live/rvk_demo.exe'),'--native-ground-test','--ground-detail-fixture','--native-coverage-test','--grass-combined','--terrain-blend','2','--pixel-lighting','--sky-zone','4001','--fixed-clock','1000','--size','512','384','--frames','180','--shot','result.bmp',*args],cwd=d,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipeline.cache')))
 text=(d/'run.txt').read_text(errors='replace');assert not re.search('VUID-|VK_ERROR|device lost|AddressSanitizer',text,re.I),name
 ims[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(int)
 dump=(d/'native-frame.txt').read_text();rep[name]={'coverage':next(s for s in dump.splitlines() if '# terrain coverage:' in s),'mean':float(ims[name].mean())};print(name,rep[name],flush=True)
for a,b in [('opaque','transparent'),('void-opaque','void-transparent'),('yaw-opaque','yaw-transparent')]:
 delta=int(abs(ims[a]-ims[b]).max());rep[a+'/'+b]={'max_delta':delta};assert delta<=1,(a,b,delta)
assert 'indices 73728; hidden-cells 4096' in rep['void-opaque']['coverage'],rep
assert np.any(ims['opaque']!=ims['void-opaque'])
(out/'validation.json').write_text(json.dumps({'result':'passed','metrics':rep,'live_game_fixed_build_verified':False},indent=2));print('PASSED',flush=True)
