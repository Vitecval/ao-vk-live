"""Render the floor's real map outputs; originals may be supplied locally."""
import argparse,json,os,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=Path('build/windows-live'))
p.add_argument('--output',type=Path,default=Path('build/deck-family'))
args=p.parse_args();root=Path(__file__).resolve().parents[1]
exe=(args.build/'rvk_demo.exe').resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
maps=root/'material-packs/ao-cyber-icc/maps'
env=dict(os.environ,RANDYVK_PIPELINE_CACHE=str(root/'build/shared-terrain-check/pipeline.cache'))
cases={
 'rough17':['--terrain-id','17','--pbr-debug','2'],
 'normal17':['--terrain-id','17','--pbr-debug','5'],
 'normal17-alias':['--terrain-id','17','--terrain-type','1010021','--pbr-debug','5'],
 'rough176':['--terrain-id','176','--pbr-debug','2'],
 'metal176':['--terrain-id','176','--pbr-debug','3'],
 'metal186':['--terrain-id','186','--pbr-debug','3'],
 'shared17':['--terrain-id','17','--terrain-blend','1'],
 'original17':['--terrain-id','17','--terrain-blend','0'],
}
images={}
for name,extra in cases.items():
 dest=out/name;dest.mkdir(exist_ok=True)
 with (dest/'run.txt').open('w') as log:
  subprocess.run([str(exe),'--shared-terrain-test','--terrain-authored','--shared-ground-maps',str(maps),
                  '--sky-zone','655','--frames','80','--size','768','512',*extra,'--shot','result.bmp'],
                 cwd=dest,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90)
 text=(dest/'run.txt').read_text(errors='replace')
 assert not re.search(r'VUID-|VK_ERROR|device lost|errors:',text,re.I),name
 images[name]=np.array(Image.open(dest/'result.bmp').convert('RGB')).astype(float)
roi=np.s_[150:370,240:528]
rough=images['rough17'][roi].mean()/255
assert .81<rough<.85,rough
norm=images['normal17'][roi]
assert norm[:,:,0].std()<1 and norm[:,:,1].std()<1 and norm[:,:,2].std()<1,'added noisy normals remain'
assert abs(images['normal17']-images['normal17-alias']).max()==0,'quality alias differs'
assert abs(images['shared17']-images['original17']).max()==0,'shared detail stamped over the tread'
for name in ['rough176','metal176','metal186']:
 values=images[name][roi].mean(2)
 assert values.max()-values.min()>20,(name,'transition mask lost')
report={'result':'passed','roughness':round(float(rough),3),'flat_normal_rgb':norm.mean(axis=(0,1)).round(1).tolist(),
        'alias_pixel_delta':0,'shared_pixel_delta':0,'transition_masks':'visible in roughness and metallic debug views',
        'in_game_visual_verification':False}
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
