"""Camera-independent ground decorations and captured temple light regressions."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--before',type=Path,help='Pre-fix demo with the same fixture support')
a=p.parse_args();exe=(a.build/'rvk_demo.exe').resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def run(name,args,binary=exe):
    d=out/name;d.mkdir(exist_ok=True)
    with (d/'run.log').open('w') as f:
        subprocess.run([str(binary),*args,'--proc-sky','0','--sky-time','1000','--fixed-clock','.000001','--size','768','512','--shot','result.bmp'],cwd=d,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
    log=(d/'run.log').read_text(errors='replace')
    assert not re.search(r'VK_ERROR|VUID-|device lost|allocation failed',log,re.I),name
    print(name,'rendered',flush=True)
    return np.asarray(Image.open(d/'result.bmp').convert('RGB')),d

temple=['--temple-lighting-fixture','--hdr','--pixel-lighting','--light-override','--pt-light','.25','--light-headroom','1.5','--frames','4']
normal,nd=run('temple',temple+['--sky-zone','4625'])
marker,md=run('temple-screen-probe',temple+['--sky-zone','4625','--temple-marker'])
assert np.array_equal(normal,marker),'Offscreen screen-space draw changed the room or ended HDR'
assert 'scene phase ended at draw 4 (fvf 0x44)' in (md/'temple-frame.txt').read_text()
assert np.array_equal(normal[8:24,8:40],np.broadcast_to([64,192,128],(16,32,3))),'Genuine interface pixels were tone mapped'
outside,_=run('outside-control',temple+['--sky-zone','4872'])
assert normal[80:450,80:550].mean()<outside[80:450,80:550].mean()*.8,'Temple spots were not reduced'
rows={'screenProbeChangedPixels':0,'templeMean':float(normal[80:450,80:550].mean()),'outsideMean':float(outside[80:450,80:550].mean())}
if a.before:
    before,_=run('outside-before',temple+['--sky-zone','4872'],a.before.resolve())
    assert np.array_equal(outside,before),'Another zone changed'
    rows['outsideChangedPixels']=0

for mode in ['combined','two-pass']:
    for angle,pitch in [('low','.15'),('high','1.2')]:
        args=['--ground-decoration-fixture','--terrain-blend','2','--sky-zone','4872','--native-first-transparent','--eye','200','5' if angle=='low' else '20','140','--view-yaw','1.5708','--view-pitch',pitch,'--frames','80']
        if mode=='combined':args+=['--grass-combined']
        rocks,rd=run(mode+'-'+angle,args+['--ground-rocks','1','--ground-pebbles','1'])
        bare,_=run(mode+'-'+angle+'-bare',args+['--ground-rocks','0','--ground-pebbles','0'])
        changed=int(np.count_nonzero(np.any(rocks!=bare,axis=2)))
        assert changed>100,(mode,angle,'Decorations disappeared',changed)
        lines=(rd/'native-frame.txt').read_text().splitlines()
        records=[];draw=''
        for s in lines:
            if s.startswith('D '):draw=s
            if 'terrain-record:' in s:records.append(draw)
        assert records and all('terrain-alpha 0/' in s and 'st1 1/' in s and '| z 1/1/4 ' in s and '| blend ' not in s for s in records),'Decorations inherited retail transparency, second stage, blend or equal-depth state'
        rows[mode+'-'+angle]={'visibleDecorationPixels':changed}

(out/'RESULTS.json').write_text(json.dumps({'result':'passed','checks':rows,'retailPlaytest':False,
    'rendererSha256':hashlib.sha256((a.build/'ao-vk.dll').read_bytes()).hexdigest(),
    'demoSha256':hashlib.sha256(exe.read_bytes()).hexdigest()},indent=2)+'\n')
print(json.dumps(rows,indent=2),flush=True)
