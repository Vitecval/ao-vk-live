"""Actual-renderer checks for mixed world depth ranges, moving scenery and depth reuse."""
from pathlib import Path
import argparse, os, subprocess, json, re, hashlib
from preview23_sky_life_tests import pixels, different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rows={};images={};dumps={};logs={}
cases=[('world-depth',0,140,True),('near-depth',512,140,True),('rotating',4096,140,True),
       ('dense-reference',2048,1250,False),('dense-cached',2048,1250,True)]
for name,toggle,frames,cache in cases:
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_STATIC_SHADOW_CACHE='1' if cache else '0',RANDYVK_PIPELINE_CACHE=str(out/'pipelines'))
    with (folder/'run.log').open('w') as log:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case','19','--visual-toggle',str(toggle),'--size','768','512',
                        '--fixed-clock','.000001','--frames',str(frames),'--shot','result.bmp'],
                       cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=480)
    log=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    images[name]=pixels(folder/'result.bmp');dumps[name]=(folder/'shadow-stability.txt').read_text();logs[name]=log
    print(name,'rendered',flush=True)
delta=different(images['world-depth'],images['near-depth']);assert delta==0,('Same world camera with different depth clipping lost shadows',delta)
rows['worldDepthChangedPixels']=delta
assert re.search(r'1 casters this frame, 0 remembered',dumps['rotating']),dumps['rotating'][-2000:]
rows['rotatingRetainedOldPoses']=0
delta=different(images['dense-reference'],images['dense-cached']);assert delta==0,('Reused static depths changed pixels',delta)
rows['staticDepthChangedPixels']=delta
assert re.search(r'depth cache: [1-4] cascades reused, 0 refreshed',dumps['dense-cached']),dumps['dense-cached'][-2000:]
def times(log):
    lines=[l for l in log.splitlines() if 'gpu ms:' in l];assert len(lines)>=2,'Missing steady GPU sample'
    return {k:float(v) for k,v in re.findall(r'([a-z][a-z +()/-]*?) ([0-9]+\.[0-9]+)(?: \||  \()',lines[-1].split('gpu ms:')[-1])}
rows['referenceGpuMs']=times(logs['dense-reference']);rows['cachedGpuMs']=times(logs['dense-cached'])
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),cases=rows,retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
