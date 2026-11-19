"""Temple column bake must approach the captured floor without dimming that floor."""
import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();exe=(a.build/'rvk_demo.exe').resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def run(name,zone,pixel,clear=False):
    d=out/name;d.mkdir(exist_ok=True)
    args=[str(exe),'--temple-lighting-fixture','--temple-baked','--hdr','--pt-light','0',
        '--light-headroom','1.5','--sky-zone',str(zone),'--proc-sky','0','--sky-time','1000',
        '--fixed-clock','.000001','--size','768','512','--frames','4','--shot','result.bmp']
    if pixel:args+=['--pixel-lighting','--light-override']
    if clear:args+=['--temple-clear']
    with (d/'run.log').open('w') as f:subprocess.run(args,cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(out/'pipelines.cache')),stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
    log=(d/'run.log').read_text(errors='replace');assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    return np.asarray(Image.open(d/'result.bmp').convert('RGB')),(d/'temple-frame.txt').read_text()

results={}
floor=np.s_[400:500,300:700];column=np.s_[100:280,100:400]
for mode,pixel in [('pixel',True),('vertex',False)]:
    corrected,dump=run(mode+'-corrected',4625,pixel)
    reference,_=run(mode+'-unadjusted-control',4872,pixel)
    clear,clear_dump=run(mode+'-scene-clear',4625,pixel,True)
    assert np.array_equal(corrected[floor],reference[floor]),mode+' dimmed the reference floor'
    assert corrected[column].mean()<reference[column].mean()*.60,mode+' did not correct the bright baked column'
    assert corrected[column].mean()<corrected[floor].mean()*1.65,mode+' column remains much brighter than floor'
    assert np.array_equal(corrected[floor],clear[floor]) and np.array_equal(corrected[column],clear[column]),mode+' scene clear changed the world lighting'
    assert 'VisualClearScreen_t (10)' in clear_dump and 'target scene' in clear_dump
    assert 'scene phase ended at draw 4 (fvf 0x44)' in clear_dump
    assert 'temple baked light:' in dump
    assert np.array_equal(corrected[8:24,8:40],np.broadcast_to([64,192,128],(16,32,3))),mode+' changed the real UI'
    results[mode]=dict(floorChangedPixels=0,columnMean=float(corrected[column].mean()),floorMean=float(corrected[floor].mean()),unadjustedColumnMean=float(reference[column].mean()),clearChangedWorldPixels=0)
    print(mode,'passed',results[mode],flush=True)
(out/'RESULTS.json').write_text(json.dumps(dict(result='passed',checks=results,retailPlaytest=False,
    rendererSha256=hashlib.sha256((a.build/'ao-vk.dll').read_bytes()).hexdigest()),indent=2)+'\n')
