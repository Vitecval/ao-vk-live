"""Real Vulkan material/FSR output from controlled fixtures, never live gameplay."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
import numpy as np
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--cache',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True)
    p.add_argument('--maps',type=Path,required=True)
    p.add_argument('--baseline-cache',type=Path)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    exe=(a.build/'rvk_demo.exe').resolve();base=a.baseline.resolve();hd=a.maps.resolve();enh=root/'material-packs/ao-icc-enhanced-original/maps'
    cache=out/'render.cache'
    if not cache.exists():shutil.copy2(a.cache,cache)
    env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache));rows=[]
    baseline_cache=out/'preview22.cache'
    if not baseline_cache.exists() and a.baseline_cache:shutil.copy2(a.baseline_cache,baseline_cache)
    def run(name,args,binary=exe):
        folder=out/name;folder.mkdir(exist_ok=True);started=time.perf_counter()
        signature=dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),arguments=args)
        if (folder/'result.bmp').exists() and (folder/'run.json').exists() and json.loads((folder/'run.json').read_text())==signature:
            return np.asarray(Image.open(folder/'result.bmp').convert('RGB')).astype(np.int16)
        run_env=dict(env,RANDYVK_PIPELINE_CACHE=str(baseline_cache if binary==base else cache))
        with (folder/'run.log').open('w') as f:
            proc=subprocess.run([str(binary),*args,'--size','512','384','--frames','8','--shot','result.bmp'],cwd=folder,env=run_env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
        text=(folder/'run.log').read_text(errors='replace');assert proc.returncode==0,(name,proc.returncode,text[-1000:])
        assert not re.search(r'VUID-|Validation Error|VK_ERROR|device lost',text,re.I),(name,'Vulkan error')
        pixels=np.asarray(Image.open(folder/'result.bmp').convert('RGB')).astype(np.int16)
        assert pixels.std()>3,(name,'blank output')
        (folder/'run.json').write_text(json.dumps(signature,indent=2)+'\n')
        rows.append(dict(name=name,seconds=time.perf_counter()-started,binary=str(binary)))
        (out/'TIMINGS.json').write_text(json.dumps(rows,indent=2)+'\n')
        return pixels
    # Existing fixture understands the same maps and identity; extra --icc-gloss
    # is only passed to the candidate, never to the frozen executable.
    comparisons={}
    for style,maps in [('anarchyhd',hd),('enhanced-original',enh)]:
        for typ,ident in [(1010006,21),(1010006,22),(1010004,295610)]:
            args=['--shared-terrain-test','--terrain-authored','--terrain-personal','--terrain-id',str(ident),'--terrain-type',str(typ),'--shared-ground-maps',str(maps),'--sky-zone','655','--pbr-debug','2'] if typ==1010006 else ['--icc-floor-test','--pbr-maps',str(maps),'--sky-zone','655','--pbr-debug','2']
            levels=[]
            for value in range(11):
                image=run(f'{style}-{ident}-g{value}',args+['--icc-gloss',str(value/10)])
                levels.append(float(image[160:300,190:320].mean()))
            assert levels[0]>levels[-1]+3,(style,ident,'roughness calibration not visible',levels)
            assert all(levels[i]>=levels[i+1]-.05 for i in range(10)),(style,ident,'non-monotonic roughness',levels)
            comparisons[f'{style}-{ident}']=dict(roughness_roi_levels=levels)
            for value in [1,.2]:
                litargs=[x for x in args]
                at=litargs.index('--pbr-debug');del litargs[at:at+2]
                run(f'{style}-{ident}-lit-{value}',litargs+['--icc-gloss',str(value)])
    # Outside ICC both gloss extremes must reproduce the frozen Preview 22 output.
    for ident in [21,22]:
        args=['--shared-terrain-test','--terrain-authored','--terrain-personal','--terrain-id',str(ident),'--shared-ground-maps',str(hd),'--sky-zone','540']
        old=run(f'preview22-nonicc-{ident}',args,base)
        for value in [0,1]:
            new=run(f'preview23-nonicc-{ident}-{value}',args+['--icc-gloss',str(value)])
            delta=int(np.abs(old-new).max());assert delta==0,('non-ICC parity',ident,value,delta)
    # FSR2 and FSR3 are distinct executions; this fixture does not cover SDK
    # initialization failure or the user's full-client crash.
    for mode in [0,1,2]:
        run(f'fsr-mode-{mode}',['--pbr-test','--hdr','--taa','--render-scale','67','--upscaler',str(mode)])
        log=(out/f'fsr-mode-{mode}'/'run.log').read_text()
        if mode:assert ('FSR 2:' if mode==1 else 'FSR 3.1:') in log,('FSR path not exercised',mode)
    result=dict(result='passed',scope='controlled renderer fixtures, not live AO',material_comparisons=comparisons,non_icc_pixel_delta=0,fsr2_and_fsr3_rendered=True,reported_client_crash_reproduced=False,timings=rows)
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

if __name__=='__main__':main()
