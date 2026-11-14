"""Renderer checks for single-path flashes and sun-shadow recovery after weather."""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess
from preview23_sky_life_tests import pixels, different

p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out.parent/'pipelines'))
rows={};images={}
def times(log,phase='strike window: '):
    line=next(l for l in reversed(log.splitlines()) if phase+'gpu ms:' in l)
    return {k.strip():float(v) for k,v in re.findall(r'([^|]+?) ([0-9]+\.[0-9]+)(?: \||  \()',line.split('gpu ms:')[-1])}
for name,case,toggle,width,height in [('storm',25,0,1280,720),('own-flash',25,1,1280,720),
    ('legacy-glow',25,3,1280,720),('near-bolt',25,5,1280,720),('sky-off',25,17,1280,720),
    ('legacy-fallback',25,11,1280,720),('fullhd-legacy',25,3,1920,1080),('shadow-recovery',26,1,768,512),
    ('own-no-fog',25,33,1280,720),('own-no-fog-legacy-off',25,49,1280,720)]:
    folder=out/name;folder.mkdir(exist_ok=True)
    with (folder/'run.log').open('w') as f:
        subprocess.run([str(build/'rvk_demo.exe'),'--visual-case',str(case),'--visual-toggle',str(toggle),
            '--size',str(width),str(height),'--fixed-clock','16','--frames','1260' if case==26 else '1250',
            '--shot','result.bmp'],cwd=folder,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=480)
    log=(folder/'run.log').read_text(errors='replace')
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    images[name]=pixels(folder/'result.bmp')
    if case==25: rows[name]=times(log)
    else:
        dumps={s:(folder/(s+'.txt')).read_text() for s in ('before','during','after')}
        for phase,dump in dumps.items():
            assert re.search(r'2 casters this frame, 2 remembered',dump),(phase,dump[-1500:])
        for draw in dumps['during'].splitlines():
            if draw.startswith('D ') and 'visual Visual' in draw: assert '| shadow' not in draw,draw
        assert different(pixels(folder/'before.bmp'),images[name])==0,'Weather left altered world shadows'
        rows[name]={s:times(log,s+': ') for s in dumps}
    print(name,'passed',flush=True)

changed=different(images['storm'],images['own-flash']);assert changed>10000,'Positive flash control disappeared'
assert different(images['storm'],images['legacy-fallback'])>10000,'Legacy fallback disappeared'
# Compare the legacy toggle with unrelated rain/fog evolution excluded from both controls.
assert different(images['own-no-fog'],images['own-no-fog-legacy-off'])==0,'Legacy glow toggle changed replacement lightning'
rows['flashChangedPixels']=changed
result=dict(result='passed',rendererSha256=hashlib.sha256((build/'ao-vk.dll').read_bytes()).hexdigest(),
    rendererCoreSha256=hashlib.sha256((build/'rvk.lib').read_bytes()).hexdigest(),fixtureSha256=hashlib.sha256((build/'rvk_demo.exe').read_bytes()).hexdigest(),cases=rows,
    syntheticLegacyGlowLayers=40,retailStrongHarmReproduction=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2),flush=True)
