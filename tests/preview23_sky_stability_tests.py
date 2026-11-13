"""Render regressions for sky camera precision, temporal stability and retail cloud replacement."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
from preview23_sky_life_tests import pixels,different
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out.parent/'checks'/'pipelines'))
rows=[]
def run(name,options):
    folder=out/name;folder.mkdir(exist_ok=True)
    cmd=[str(build/'rvk_demo.exe'),'--sky-enhance-fixture','--soft-sky','--water-lake','--water-off','--sky-dome','8FAACA','--water-pitch','-.18','--sky-zone','560','--stars','0','--meteors','0','--aurora','0','--sky-clouds','1','--sky-traffic','1','--sun-dir','.6','-.7','.3','--size','768','512','--fixed-clock','0.000001','--frames','40','--dump','frame.txt','--shot','result.bmp',*options]
    with (folder/'run.log').open('w') as f:subprocess.run(cmd,cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=600)
    log=(folder/'run.log').read_text();assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    dump=(folder/'sky-frame.txt').read_text();print(name,'rendered',flush=True)
    return pixels(folder/'result.bmp'),dump
base,_=run('origin',[])
for offset in ['3000','100000']:
    image,_=run('offset-'+offset,['--sky-camera-offset',offset]);delta=different(base[:256],image[:256]);assert delta==0,(offset,delta)
    rows.append(dict(offset=offset,skyPixelsChanged=delta))
image,dump=run('retail-sheets',['--clouds-game'])
assert dump.count("cloud sheet: replaced by enhanced sky clouds")>=10,'Retail cloud sheets were not replaced'
rows.append(dict(retailSheetsReplaced=dump.count('cloud sheet: replaced by enhanced sky clouds')))
x,_=run('taa-40',['--taa']);y,_=run('taa-41',['--taa','--frames','41'])
error=sum(abs(i-j) for aa,bb in zip(x[:256],y[:256]) for i,j in zip(aa,bb))/(768*256*3)
assert error<1.,('Static temporal sky unstable',error)
rows.append(dict(staticTaaMeanChannelDelta=error))
control,_=run('horizon-off',['--water-pitch','0','--sky-clouds','0','--sky-traffic','0'])
horizon,_=run('horizon-on',['--water-pitch','0','--sky-traffic','0'])
assert different(control[256:],horizon[256:])==0,'Clouds must not extend beneath the geometric horizon'
assert different(control[:256],horizon[:256])>1000,'Clouds missing above horizon'
rows.append(dict(cloudChangesBelowHorizon=0))
(out/'RESULTS.json').write_text(json.dumps(dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),scope='Controlled sky fixture; retail motion and performance need a playtest',cases=rows),indent=2)+'\n')
print('Revision 37 stability checks passed',flush=True)
