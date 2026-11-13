"""Rendered sparse pebbles, huge-shadow toggle and angular sky-traffic regression."""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess
from preview23_sky_life_tests import pixels, different

p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--reuse-pebbles',action='store_true');a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines'))
def run(name,options):
    folder=out/name;folder.mkdir(exist_ok=True)
    with (folder/'run.log').open('w') as log:
        subprocess.run([str(build/'rvk_demo.exe'),*options,'--shot','result.bmp'],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
    log=(folder/'run.log').read_text(errors='replace');assert not re.search('VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    return folder,pixels(folder/'result.bmp')

counts={}
common=['--native-ground-test','--ground-detail-fixture','--terrain-maps','--pixel-lighting','--sky-zone','655','--fixed-clock','1000','--size','768','512','--frames','480','--grass-combined','--terrain-blend','2']
for density in ['0','0.15','0.5','1']:
    folder=out/('pebbles-'+density)
    if not a.reuse_pebbles: folder,_=run('pebbles-'+density,[*common,'--ground-pebbles',density])
    dump=(folder/'native-frame.txt').read_text()
    m=re.search(r'native terrain: shared-plus \d+; enabled \d+; patches \d+; patches/frame [\d.]+; rocks \d+; pebbles (\d+)',dump);assert m
    counts[density]=int(m[1]);print('pebbles',density,counts[density],flush=True)
assert counts['0']==0 and 0<counts['0.15']<counts['0.5']<counts['1'],counts
assert counts['0.15']<counts['0.5']*.45,counts

shadow=[];images=[]
for toggle in ['0','4']:
    folder,image=run('huge-shadow-'+toggle,['--visual-case','15','--visual-toggle',toggle,'--size','768','512','--frames','40','--fixed-clock','.001'])
    dump=(folder/'visual-frame.txt').read_text()
    m=re.search(r'Huge shadow filter: enabled (\d+); skipped (\d+) of (\d+) casters',dump);assert m,dump[-200:]
    shadow.append(list(map(int,m.groups())));images.append(image);print('huge shadows',shadow[-1],flush=True)
assert shadow[0][1]==0 and 0<shadow[1][1]<shadow[1][2],shadow
changed=different(*images);assert changed>100,changed

folder,_=run('ship-underside',['--sky-enhance-fixture','--soft-sky','--water-lake','--water-off','--sky-dome','8FAACA','--sky-zone','560','--sky-clouds','0','--sky-traffic','1','--water-yaw','2.189','--water-pitch','-.519','--stars','0','--meteors','0','--aurora','0','--sun-dir','.6','-.7','.3','--fixed-clock','.000001','--size','1920','1080','--frames','40'])
report=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),pebbleCounts=counts,hugeShadowCases=shadow,hugeShadowChangedPixels=changed,shipFixture=str(folder/'result.bmp'),retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
