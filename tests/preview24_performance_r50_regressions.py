"""Candidate renders against the already-verified r49 images and native/shared terrain."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels,different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pipeline',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rows={}
cases=[('mesh-direct',19,49152),('mesh-retained',19,16384),('mesh-shared',19,81920),('hidden-direct',19,49154),('hidden-retained',19,16386),('hidden-shared',19,81922),('turning-retained',19,16402),('detail-on',23,129),('legacy-shape-on',23,161)]
for name,scene,toggle in cases:
 folder=out/name;folder.mkdir(exist_ok=True)
 env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.pipeline.resolve()),AOVK_FIXTURE_ROCK_ID='5955',AOVK_ROCK_PACK=str(out/'no-external-pack'))
 with (folder/'run.log').open('w') as f:subprocess.run([str((a.build/'rvk_demo.exe').resolve()),'--visual-case',str(scene),'--visual-toggle',str(toggle),'--size','768','512','--fixed-clock','16' if scene==19 else '.000001','--frames','140' if scene==19 else '1250','--shot','result.bmp'],cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
 log=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
 delta=different(pixels(folder/'result.bmp'),pixels(root/'build/preview24/stability-r49/gpu-final'/name/'result.bmp'));assert delta==0,(name,delta)
 dump=(folder/('shadow-stability.txt' if scene==19 else 'visual-frame.txt')).read_text()
 assert 'rigid rock clusters: draws 0; rocks 0;' in dump and 'rock geometry: draws 0;' in dump,name
 if scene==19:assert re.search(r'\b2 remembered\b',dump),name
 rows[name]={'r49ChangedPixels':delta};print(name,'passed',flush=True)
pins=json.loads((root/'build/preview24/rocks-r45/SOURCE-FILES.json').read_text());original=next(x['sha256'] for x in pins if x['path']=='rvk/shaders/rock_objects.glsl');actual=hashlib.sha256((root/'rvk/shaders/rock_objects.glsl').read_text().encode()).hexdigest();assert original==actual
(out/'RESULTS.json').write_text(json.dumps({'result':'passed','rendererSha256':hashlib.sha256((a.build/'ao-vk.dll').read_bytes()).hexdigest(),'cases':rows,'originalDetailShaderSha256':actual},indent=2)+'\n')
