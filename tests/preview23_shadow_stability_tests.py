"""Actual Vulkan regressions for retained scenery, view turns and huge-shadow stability."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from preview23_sky_life_tests import pixels,different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'));images={};dumps={};rows=[]
cases=[('drawn',0),('hidden-after-one',1),('hidden-after-thirty',2),('turning',17),('approaching',65),('huge-all',12),('huge-partial',13),('huge-no-memory',268),('huge-partial-no-memory',269),('no-building',32),('huge-not-filtered',4)]
for name,toggle in cases:
 folder=out/name;folder.mkdir(exist_ok=True)
 with (folder/'run.log').open('w') as log:
  subprocess.run([str(build/'rvk_demo.exe'),'--visual-case','19','--visual-toggle',str(toggle),'--size','768','512','--fixed-clock','.000001','--frames','140','--shot','result.bmp'],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
 log=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
 images[name]=pixels(folder/'result.bmp');dumps[name]=(folder/'shadow-stability.txt').read_text()
 print(name,'rendered',flush=True)
negative=different(images['drawn'],images['no-building']);assert negative>10000,('Shadow fixture has no visible shadow',negative)
for name in ['hidden-after-one','hidden-after-thirty','turning']:
 delta=different(images['drawn'],images[name]);assert delta==0,(name,delta)
 assert '0 casters this frame, 1 remembered' in dumps[name],name
 rows.append(dict(case=name,changedPixels=delta));print(name,'stable',flush=True)
approach=different(images['drawn'],images['approaching']);assert approach<negative*.02,('Shadow changed too much while approaching',approach,negative)
rows.append(dict(case='approaching',changedPixels=approach,shadowPixels=negative))
for left,right in [('huge-all','huge-partial'),('huge-no-memory','huge-partial-no-memory')]:
 delta=different(images[left],images[right]);assert delta==0,(left,right,delta)
 assert re.search(r'Huge shadow filter: enabled 1; skipped [12] of [12] casters',dumps[right]),right
 rows.append(dict(case=right,changedPixels=delta));print(right,'stable',flush=True)
filter_effect=different(images['huge-not-filtered'],images['huge-all']);assert filter_effect>1000,filter_effect
rows.append(dict(case='negative-controls',shadowPixels=negative,hugeFilterPixels=filter_effect))
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),cases=rows,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
