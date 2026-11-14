"""Rock objects must not switch cliff texturing back on for terrain."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels, different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rows={}
for native in [False,True]:
    images=[]
    for strength in [0,1]:
        name=('native' if native else 'shared')+'-'+str(strength);folder=out/name;folder.mkdir(exist_ok=True)
        env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'),AOVK_FIXTURE_ROCK_STRENGTH=str(strength))
        args=[str(build/'rvk_demo.exe'),'--shared-terrain-test','--terrain-pebbles','--terrain-cliff-fixture','--terrain-off','--hdr','--size','768','512','--fixed-clock','.000001','--frames','100','--shot','result.bmp']
        if native:args+=['--terrain-native-fixture']
        with (folder/'run.log').open('w') as log:subprocess.run(args,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
        text=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',text,re.I),name
        if strength:assert "terrain: the cliffs' rock is in" in text,name
        images.append(pixels(folder/'result.bmp'));print(name,'rendered',flush=True)
    rows['native' if native else 'shared']=different(*images);assert rows['native' if native else 'shared']==0,rows
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),changedTerrainPixels=rows)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
