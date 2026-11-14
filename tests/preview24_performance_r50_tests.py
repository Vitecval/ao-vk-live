"""Actual GPU coverage, late identity, residency and equivalent grass interaction checks."""
import argparse, json, os, re, subprocess, hashlib
from pathlib import Path
from preview23_sky_life_tests import pixels, different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pipeline',type=Path,required=True);p.add_argument('--reuse-raised',action='store_true');a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rows={};images={};dumps={}
def run(name,args,extra=None):
    folder=out/name;folder.mkdir(exist_ok=True)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(a.pipeline.resolve()))
    env.update(extra or {})
    if not (a.reuse_raised and name.startswith('raised-') and (folder/'result.bmp').exists()):
      with (folder/'run.log').open('w') as log:
        subprocess.run([str(build/'rvk_demo.exe')]+args,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
    log=(folder/'run.log').read_text(errors='replace')
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|persistence failed',log,re.I),name
    print(name,'passed',flush=True)
    return folder,log
for name,flag in [('upload','--grass-upload-test'),('material-residency','--material-cache-test')]:
    folder,log=run(name,[flag]);rows[name]={'passed':True,'log':str(folder/'run.log')}
for mode in [1,2,3,4,5,6,7,8]:
    name='raised-'+str(mode)
    folder,log=run(name,['--sky-time','1000','--shared-terrain-test','--raised-grass',str(mode),'--hdr','--size','768','512','--fixed-clock','.000001','--frames','1250','--shot','result.bmp'])
    dump=(folder/'terrain-frame.txt').read_text();m=re.search(r'Raised grass: tiles (\d+) blades (\d+) root heights ([^ \n]+) ([^ \n]+)',dump)
    count=int(m[2]) if m else 0
    if mode in [2,3,8]:assert count==0,(mode,count)
    else:assert count>1000,(mode,count)
    if mode==4:assert float(m[4])-float(m[3])>=1.9,m.groups()
    rows[name]={'blades':count,'tiles':int(m[1]) if m else 0}
    images[name]=pixels(folder/'result.bmp')
folder,log=run('raised-control-repeat',['--sky-time','1000','--shared-terrain-test','--raised-grass','5','--hdr','--size','768','512','--fixed-clock','.000001','--frames','1250','--shot','result.bmp'])
repeatDelta=different(images['raised-5'],pixels(folder/'result.bmp'))
rows['raised-repeat']={'changedPixels':repeatDelta}
assert repeatDelta==0,('Fixed sky scene is not repeatable',repeatDelta)
print('Identical raised-scene repeat changed',repeatDelta,'pixels',flush=True)
for mode in [6,7]:
    delta=different(images['raised-5'],images['raised-'+str(mode)])
    assert rows['raised-'+str(mode)]['blades']==rows['raised-5']['blades'],('Late identity changed density',mode)
    if repeatDelta==0:assert delta==0,('Late identity changed coverage',mode,delta)
    rows['raised-'+str(mode)]['referenceChangedPixels']=delta
for crowd,ref in [(False,True),(False,False),(True,True),(True,False)]:
    name=('crowd-' if crowd else '')+('pushers-reference' if ref else 'pushers-optimised')
    folder,log=run(name,['--sky-time','1000','--grass-bench','--grass-bench-still','--grass-bench-cross','--grass-field-dist','80','--size','1024','768','--frames','1500','--fixed-clock','16','--profile-window','--shot','result.bmp'],{'AOVK_TEST_GRASS_PUSHER_REFERENCE':'1' if ref else '0','AOVK_TEST_GRASS_DENSITY':'12','AOVK_TEST_GRASS_WALKERS':'16' if crowd else '1'})
    dump=(folder/'grass-frame.txt').read_text();m=re.search(r'Grass pusher culling: (\d+) of (\d+) tile-character pairs retained; reference (\d+); distance ([^ ]+) density ([^\n]+)',dump);assert m,(name,dump[-1000:])
    kept,tested=int(m[1]),int(m[2]);assert tested>0 and float(m[4])==80 and float(m[5])==12,m.groups()
    if ref:assert kept==tested
    else:assert kept<tested/2,m.groups()
    profile=[line for line in log.splitlines() if 'grass window:' in line and 'gpu ms:' in line];assert profile,(name,log[-2000:])
    timing={k:float(v) for k,v in re.findall(r'([a-z][a-z +()/-]*?) ([0-9]+\.[0-9]+)(?: \||  \()',profile[-1].split('gpu ms:')[-1])}
    rows[name]={'retainedPairs':kept,'testedPairs':tested,'gpuMs':timing}
    images[name]=pixels(folder/'result.bmp')
for prefix in ['', 'crowd-']:
 delta=different(images[prefix+'pushers-reference'],images[prefix+'pushers-optimised']);assert delta==0,('Pusher culling changed the scene',prefix,delta)
 rows[prefix+'pushers-optimised']['referenceChangedPixels']=delta
(out/'RESULTS.json').write_text(json.dumps({'result':'passed','rendererSha256':hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),'cases':rows},indent=2)+'\n')
print('All performance and coverage checks passed',flush=True)
