"""Reproduce the individually generated R31 material exports (Pillow, NumPy)."""
from pathlib import Path
import argparse,json,hashlib
import numpy as np
from PIL import Image,ImageFilter
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
 records=json.loads((S/'material-packs/preview23-selected-r31.json').read_text())['materials'];args.output.mkdir(parents=True,exist_ok=True)
 for r in records:
  raw=S/'material-packs/ao-cyber-world/source-art'/(r['source']+'.png');assert hashlib.sha256(raw.read_bytes()).hexdigest()==r['sourceSha256']
  w,h=r['size']
  with Image.open(raw) as im:
   if r.get('exportCrop'):im=im.crop(r['exportCrop'])
   assert im.width>=w and im.height>=h,'Never enlarge a small render and call it HD'
   art=im.convert('RGB').resize((w,h),Image.Resampling.LANCZOS)
  stem=r['identity'].replace(':','_');art.save(args.output/(stem+'_d.png'),optimize=True)
  lum=art.convert('L');fine=(np.asarray(lum,dtype=np.float32)-np.asarray(lum.filter(ImageFilter.GaussianBlur(2)),dtype=np.float32))/255
  dy,dx=np.gradient(fine);normal=np.stack((-dx*.45,dy*.45,np.ones_like(dx)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
  Image.fromarray(np.rint(np.clip(normal*.5+.5,0,1)*255).astype('uint8')).resize((w//2,h//2),Image.Resampling.LANCZOS).save(args.output/(stem+'_n.png'),optimize=True)
  orm=np.stack((np.clip(.99+fine*.05,.96,1),np.clip(r['roughness']+fine*.08,.62,.96),np.zeros_like(fine)),axis=2)
  Image.fromarray(np.rint(orm*255).astype('uint8')).resize((w//2,h//2),Image.Resampling.LANCZOS).save(args.output/(stem+'_orm.png'),optimize=True)
 print(f'Baked {len(records)} independent generated R31 materials')
if __name__=='__main__':main()
