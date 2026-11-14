"""Production Vulkan rock geometry, material fallback and retained-buffer checks."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
from preview23_sky_life_tests import pixels, different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pack',type=Path,required=True);p.add_argument('--cache',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cases=[('off',64,5955,True),('on',65,5955,True),('direct',1,5955,True),('far',67,5955,True),('moving',69,5955,True),
       ('character-off',72,5955,True),('character-on',73,5955,True),('unknown-off',64,0,True),('unknown-on',65,0,True),
       ('lite-off',64,5955,False),('lite-on',65,5955,False),('baked',97,5955,True),('shadows',321,5955,True),('dense',449,5955,True),('all-catalog',1089,5955,True),('legacy-kind',2113,5955,True),('cave-wall',4161,5955,True),('transparent-off',8256,5955,True),('transparent-on',8257,5955,True)]
images={};rows={};dumps={}
for name,toggle,identity,full in cases:
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.cache.resolve()),AOVK_ROCK_PACK=str(a.pack.resolve() if full else out/'missing-full-pack'),AOVK_FIXTURE_ROCK_ID=str(identity))
    with (folder/'run.log').open('w') as f:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case','29','--visual-toggle',str(toggle),'--size','768','512','--fixed-clock','16','--frames','400','--shot','result.bmp'],cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
    log=(folder/'run.log').read_text(errors='replace');dump=(folder/'clusters-frame.txt').read_text(errors='replace')
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|persistence failed',log,re.I),name
    stats=re.search(r'rigid rock clusters: draws (\d+); rocks (\d+); triangles (\d+); built (\d+); cached (\d+); CPU bytes (\d+); camera deformation (\d+)',dump)
    assert stats,name
    draws,rocks,triangles,built,cached,memory,warp=map(int,stats.groups())
    expected=146 if name=='all-catalog' else 99 if name=='dense' else 0 if name in ['off','character-off','character-on','unknown-off','unknown-on','lite-off','transparent-off','transparent-on'] else 3
    assert draws==expected,(name,draws,expected)
    assert built==0 and warp==0 and cached<=512 and memory<=32*1024*1024,(name,stats.groups())
    assert 'rock geometry: draws 0' in dump,name
    if full: assert 'shared Full material ready' in log,name
    else: assert 'Full surface pack absent' in log and 'shared Full material ready' not in log,name
    images[name]=pixels(folder/'result.bmp');dumps[name]=dump
    rows[name]=dict(draws=draws,rocks=rocks,triangles=triangles,builds=built,cached=cached,cpuBytes=memory,gpu=[x for x in log.splitlines() if 'gpu ms:' in x][-1:])
    print(name,'passed',draws,rocks,memory,flush=True)
for left,right in [('on','direct'),('character-off','character-on'),('unknown-off','unknown-on'),('transparent-off','transparent-on')]:
    count=different(images[left],images[right]);assert count==0,(left,right,count)
for left,right in [('off','on'),('lite-off','lite-on'),('lite-on','on')]:
    count=different(images[left],images[right]);assert count>10000,(left,right,count)
for name in ['far','moving','shadows']:
    assert rows[name]['rocks']==rows['on']['rocks'],(name,rows[name],rows['on'])
assert rows['dense']['builds']==0
seen=set(map(int,re.findall(r'rdb:(\d+) type:1010004',dumps['all-catalog'])))
assert len(seen)==143,len(seen)
assert {5955,9863,205836,214375,205675,205838,202640}<=seen
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),demoSha256=hashlib.sha256((build/'rvk_demo.exe').read_bytes()).hexdigest(),catalogIdentities=len(seen),cases=rows,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print('All runtime cases passed',flush=True)
