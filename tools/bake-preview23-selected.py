"""Bake the three individually rendered R30 restorations without native game pixels.

Requires Pillow and NumPy. Output is explicit; other material maps are untouched.
The renderer samples native coverage separately. Export hashes can be checked
against material-packs/ao-cyber-world/manifest.json.
"""
from pathlib import Path
import argparse,json,hashlib
import numpy as np
from PIL import Image,ImageFilter

ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
 pack=ROOT/'material-packs/ao-cyber-world'
 records=json.loads((ROOT/'material-packs/preview23-selected-r30.json').read_text())['materials']
 args.output.mkdir(parents=True,exist_ok=True)
 for record in records:
  kind,identity=map(int,record['identity'].split(':'));w,h=record['size']
  source=pack/'source-art'/(record['source']+'.png')
  assert hashlib.sha256(source.read_bytes()).hexdigest()==record['sourceSha256'],source
  with Image.open(source) as im:
   if im.width<w or im.height<h:raise ValueError(f'{source}: source cannot supply {w}x{h}')
   art=im.convert('RGB').resize((w,h),Image.Resampling.LANCZOS)
  stem=f'{kind}_{identity}';art.save(args.output/(stem+'_d.png'),optimize=True)
  lum=art.convert('L');fine=(np.asarray(lum,dtype=np.float32)-np.asarray(lum.filter(ImageFilter.GaussianBlur(2)),dtype=np.float32))/255
  dy,dx=np.gradient(fine);normal=np.stack((-dx*.35,dy*.35,np.ones_like(dx)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
  Image.fromarray(np.rint(np.clip(normal*.5+.5,0,1)*255).astype('uint8')).resize((w//2,h//2),Image.Resampling.LANCZOS).save(args.output/(stem+'_n.png'),optimize=True)
  orm=np.stack((np.clip(.98+fine*.15,.94,1),np.clip(.68+fine*.1,.60,.76),np.full_like(fine,.35)),axis=2)
  Image.fromarray(np.rint(orm*255).astype('uint8')).resize((w//2,h//2),Image.Resampling.LANCZOS).save(args.output/(stem+'_orm.png'),optimize=True)
 print(f'Baked {len(records)} generated restorations into {args.output}')
if __name__=='__main__':main()
