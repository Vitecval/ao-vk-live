"""Regional lighting and molten-liquid GPU comparisons, with actual render output."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--before',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--cache-file',type=Path);a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
cache=a.cache_file.resolve() if a.cache_file else out.parent/'render-check/pipelines.cache'
def run(name,exe,args):
    dest=out/name;dest.mkdir(exist_ok=True)
    with (dest/'run.log').open('w') as f:subprocess.run([str(exe.resolve()),*args,'--size','768','512','--frames','100','--fixed-clock','.000001','--shot','result.bmp','--dump','frame.txt'],cwd=dest,stdout=f,stderr=subprocess.STDOUT,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache)),check=True,timeout=360)
    log=(dest/'run.log').read_text(errors='replace')
    assert not re.search('VUID-|VK_ERROR|device lost|allocation failed',log,re.I),name
    print(name,'rendered',flush=True)
    return np.asarray(Image.open(dest/'result.bmp').convert('RGB')).astype(float)
exe=a.build/'rvk_demo.exe';before=a.before/'rvk_demo.exe';images={}
for zone in [655,4320,4005]:
    args=['--visual-case','7','--visual-toggle','1','--area-zone',str(zone),'--area-lighting','.6','--soft-sky','--shadows']
    images[f'{zone}-before']=run(f'{zone}-before',before,args)
    images[f'{zone}-after']=run(f'{zone}-after',exe,args)
assert np.array_equal(images['655-before'],images['655-after']),'unrelated outdoor profile changed'
for zone in [4320,4005]:
    old,new=images[f'{zone}-before'],images[f'{zone}-after']
    assert new[340:500].mean()>old[340:500].mean()+1,(zone,'regional light missing')
    assert new[240:460].std()>10,(zone,'material contrast flattened')
water=['--water-lake','--sky-zone','4005','--area-lighting','.6','--water-wall','--water-waves','0','--point-shadows','4','--pixel-lighting','--light-override','--shadows']
images['molten-before']=run('molten-before',before,water)
images['molten-after']=run('molten-after',exe,water)
delta=np.abs(images['molten-after']-images['molten-before'])
assert np.count_nonzero(delta>10)>500,'lava still rendered as water'
new=images['molten-after'];hot=(new[:,:,0]>new[:,:,1]*1.4)&(new[:,:,0]>new[:,:,2]*2)&(new[:,:,0]>100)
assert hot.sum()>500,'molten emission absent'
log=(out/'molten-after/run.log').read_text(errors='replace')
assert re.search(r'point shadows: [1-4] of [1-8] candidate lights',log),'surface lights not reaching lamp shadow selection'
report=dict(result='passed',unrelatedRegionPixelIdentical=True,hotPixels=int(hot.sum()),retailPlaytest=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
