"""Actual Vulkan checks for shared photographic rock detail and its exclusions."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels, different

p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
images={};logs={};rows={}
cases=[('off',0,5955),('on',1,5955),('tile-rock',1,9863),('moving',9,5955),
       ('character-off',4,5955),('character-on',5,5955),('unknown-off',0,0),('unknown-on',1,0),
       ('baked-off',16,5955),('baked-on',17,5955),('baked-unknown-off',16,0),('baked-unknown-on',17,0)]
for name,toggle,identity in cases:
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'),AOVK_FIXTURE_ROCK_ID=str(identity))
    with (folder/'run.log').open('w') as log:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case','23','--visual-toggle',str(toggle),'--size','768','512',
                        '--fixed-clock','.000001','--frames','1250','--shot','result.bmp'],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
    text=(folder/'run.log').read_text(errors='replace')
    assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',text,re.I),name
    images[name]=pixels(folder/'result.bmp');logs[name]=text
    print(name,'rendered',flush=True)
rows['detailPixels']=different(images['off'],images['on']);assert rows['detailPixels']>10000,rows
for name,left,right in [('tileRockMatchesSharedMaterial','on','tile-rock'),
                        ('charactersUnchanged','character-off','character-on'),('unknownSurfacesUnchanged','unknown-off','unknown-on'),
                        ('bakedUnknownSurfacesUnchanged','baked-unknown-off','baked-unknown-on')]:
    rows[name]=different(images[left],images[right]);assert rows[name]==0,(name,rows[name])
rows['turningChangedPixels']=different(images['on'],images['moving'])
rows['turningMaximumChannelDifference']=max(abs(x-y) for row,other in zip(images['on'],images['moving']) for x,y in zip(row,other))
assert rows['turningMaximumChannelDifference']<=1,rows # one display quantization step
rows['bakedRockDetailPixels']=different(images['baked-off'],images['baked-on']);assert rows['bakedRockDetailPixels']>10000,rows
assert "terrain: the cliffs' rock is in" in logs['on'].lower(), 'Photographic material did not load'
rows['gpuSamples']={name:[line.split('gpu ms:')[-1] for line in logs[name].splitlines() if 'gpu ms:' in line][-2:] for name in ['off','on']}
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),cases=rows,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
