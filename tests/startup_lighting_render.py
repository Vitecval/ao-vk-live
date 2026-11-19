"""Captured login-light colours and fully matte temple stone regressions."""
import argparse,hashlib,json,os,re,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache'))
rows={}
def run(name,args):
 d=out/name;d.mkdir(exist_ok=True);start=time.monotonic()
 with (d/'run.log').open('w') as log:subprocess.run([str(build/'rvk_demo.exe'),*args,'--size','768','512','--frames','4','--fixed-clock','.000001','--proc-sky','0','--sky-time','1000','--shot','result.bmp'],cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=360)
 text=(d/'run.log').read_text(errors='replace');assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|cannot write',text,re.I),name
 rows[name]={'seconds':round(time.monotonic()-start,3)};print(name,'rendered',rows[name],flush=True)
 return np.asarray(Image.open(d/'result.bmp').convert('RGB')).astype(int),d
stock,_=run('studio-authored-reference',['--login-studio-fixture','0'])
login,ld=run('studio-enhanced',['--login-studio-fixture','1'])
changed,_=run('studio-world-lighting-control',['--login-studio-fixture','2'])
error=int(np.max(abs(stock-login)));assert error<=2,('Login studio changed authored lighting',error)
assert np.count_nonzero(np.any(changed!=login,axis=2))>1000,'The studio control did not exercise added lighting'
assert 'login studio 1; authored vertex lights 1' in (ld/'studio-frame.txt').read_text()
assert np.array_equal(stock[8:24,8:40],login[8:24,8:40]),'Login fix changed interface colours'
rows['loginAuthoredMaxChannelError']=error
args=['--temple-lighting-fixture','--temple-baked','--temple-matte','--temple-clear','--hdr','--pixel-lighting','--light-override','--pt-light','.25','--light-headroom','1.5','--sky-zone','4625']
matte,md=run('temple-matte-pbr-on',args)
reference,_=run('temple-matte-pbr-off',args+['--pbr-off'])
error=int(np.max(abs(matte-reference)));assert error<=2,('Temple added specular/environment response',error)
dump=(md/'temple-frame.txt').read_text();assert 'temple matte stone: specular 0; environment 0; SSR 0' in dump
assert 'scene phase ended at draw 4 (fvf 0x44)' in dump
assert np.array_equal(matte[8:24,8:40],stock[8:24,8:40]),'Temple fix changed interface colours'
rows['matteReflectionMaxChannelError']=error
rows['cacheSaved']=bool((out/'pipelines.cache').stat().st_size>32)
(out/'RESULTS.json').write_text(json.dumps(dict(result='passed',checks=rows,rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),retailPlaytest=False),indent=2)+'\n')
print(json.dumps(rows,indent=2),flush=True)
