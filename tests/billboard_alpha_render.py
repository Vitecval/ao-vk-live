"""Soft screen planes must never receive leaf lighting or foliage distance LOD."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--cache-file',type=Path)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cache=(a.cache_file.resolve() if a.cache_file else out/'pipelines.cache')
images={};counts={}
for name,kind,leaf,lod in [('soft-off',1,0,0),('soft-light',1,1,0),('soft-lod',1,1,1),('leaf-off',2,0,0),('leaf-light',2,1,0),('leaf-lod',2,1,1)]:
 d=out/name;d.mkdir(exist_ok=True)
 args=['--screen-card-test',str(kind),'--leaf',str(leaf),'--foliage-lod',str(lod),'--hdr','--sky-zone','566','--size','768','512','--frames','650','--fixed-clock','1000','--shot','result.bmp']
 with (d/'run.log').open('w') as f:subprocess.run([str((a.build/'rvk_demo.exe').resolve()),*args],cwd=d,env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(cache)),stdout=f,stderr=subprocess.STDOUT,check=True,timeout=360)
 log=(d/'run.log').read_text(errors='replace');assert not re.search('VUID-|VK_ERROR|device lost|allocation failed',log,re.I),name
 stats=re.findall(r'foliage: (\d+) draws, (\d+) of them far',log);assert stats,name+' missing classification counters'
 counts[name]=list(map(int,stats[-1]));images[name]=np.asarray(Image.open(d/'result.bmp').convert('RGB'),dtype=float)
 if kind==1:assert 'alpha edges 0.000 soft 1.00' in (d/'screen-card-frame.txt').read_text(),name+' fixture does not match captured soft card'
 print(name,'rendered',counts[name],flush=True)
soft_light=float(np.abs(images['soft-off']-images['soft-light']).max());soft_lod=float(np.abs(images['soft-light']-images['soft-lod']).max())
assert soft_light<=1 and soft_lod<=1,('Soft screen colour depends on foliage controls',soft_light,soft_lod)
assert all(counts[n]==[0,0] for n in ['soft-off','soft-light','soft-lod']),counts
assert all(counts[n][0]>0 for n in ['leaf-off','leaf-light','leaf-lod']) and counts['leaf-lod'][1]>0,counts
leaf_delta=float(np.abs(images['leaf-off']-images['leaf-light']).max());assert leaf_delta>5,'Genuine leaf backlighting was removed'
report=dict(result='passed',softLeafLightingMaxPixelDelta=soft_light,softFoliageLodMaxPixelDelta=soft_lod,leafBacklightMaxPixelDelta=leaf_delta,foliageDrawCounts=counts,retailFlickerVerified=False)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
