"""Compare repeated static studio frames before/after the login-only correction."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--before',type=Path,required=True)
p.add_argument('--after',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache'))

def run(build,name,mode,temporal):
    folder=out/name;folder.mkdir(exist_ok=True)
    args=[str(build.resolve()/'rvk_demo.exe'),'--login-studio-fixture',str(mode),
          '--size','768','512','--frames','40' if temporal else '4',
          '--fixed-clock','.000001','--proc-sky','0','--shot','result.bmp']
    if temporal:args+=['--taa','--sharpen','.4']
    with (folder/'run.log').open('w') as log:
        subprocess.run(args,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=360,check=True)
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed|cannot write',(folder/'run.log').read_text(errors='replace'),re.I)
    files=sorted(folder.glob('result.bmp-*.bmp')) if temporal else [folder/'result.bmp']
    assert len(files)==(16 if temporal else 1)
    return np.stack([np.asarray(Image.open(f).convert('RGB')).astype(float) for f in files])

before=run(a.before,'before-temporal',1,True)
after=run(a.after,'after-temporal',1,True)
# Interior panel pixels exclude the background silhouette and interface. These
# have stationary high-frequency detail and must not behave like new effects.
roi=np.s_[:,100:410,150:620,:]
old=float(before[roi].std(axis=0).mean());new=float(after[roi].std(axis=0).mean())
assert old>.5,('Control did not expose temporal shimmer',old)
assert new<old*.65,('Login temporal stability did not improve sufficiently',old,new)
assert np.array_equal(before[:,8:24,8:40],after[:,8:24,8:40]),'Interface changed'
fresh=run(a.after,'fresh-start-unknown-engine',6,True)
returned=run(a.after,'return-from-world',7,True)
assert np.array_equal(fresh,returned),'Fresh and returned login differ'
for name in ('fresh-start-unknown-engine','return-from-world'):
    assert 'login studio 1; authored vertex lights 1' in (out/name/'studio-frame.txt').read_text()
world_before=run(a.before,'before-world-control',2,False)
world_after=run(a.after,'after-world-control',2,False)
assert np.array_equal(world_before,world_after),'World lighting changed'
# Use the accepted matte temple's existing regression captures as a reference.
# The login branch must leave the accepted temple output byte-for-byte intact.
temple=out/'temple-control';temple.mkdir(exist_ok=True)
with (temple/'run.log').open('w') as log:
    subprocess.run([str(a.after.resolve()/'rvk_demo.exe'),'--temple-lighting-fixture',
        '--temple-baked','--temple-matte','--temple-clear','--hdr','--pixel-lighting',
        '--light-override','--pt-light','.25','--light-headroom','1.5','--sky-zone','4625',
        '--size','768','512','--frames','4','--fixed-clock','.000001','--proc-sky','0',
        '--sky-time','1000','--shot','result.bmp'],cwd=temple,env=env,
        stdout=log,stderr=subprocess.STDOUT,timeout=360,check=True)
prior=Path(__file__).resolve().parents[1]/'build/startup-lighting-fix/verification/temple-matte-pbr-on/result.bmp'
assert np.array_equal(np.asarray(Image.open(prior)),np.asarray(Image.open(temple/'result.bmp'))),'Accepted temple output changed'
reference=run(a.after,'before-colour',4,False)
adjusted=run(a.after,'after-colour',5,False)
delta=np.abs(reference-adjusted)
assert delta.max()<=18,('Login colour adjustment is excessive',delta.max())
assert np.count_nonzero(delta)>1000,'Login colour adjustment was not exercised'
assert np.array_equal(reference[:,8:24,8:40],adjusted[:,8:24,8:40]),'Interface colour changed'
result=dict(result='passed',beforeTemporalStd=old,afterTemporalStd=new,
            reductionPercent=100*(1-new/old),maxColourChange=float(delta.max()),
            freshAndReturnedLoginIdentical=True,
            worldControlIdentical=True,templeControlIdentical=True,interfaceIdentical=True,
            rendererSha256=hashlib.sha256((a.after/'ao-vk.dll').read_bytes()).hexdigest(),retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
