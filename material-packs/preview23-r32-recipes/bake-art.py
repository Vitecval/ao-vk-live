"""Export individually rendered artwork; retain native alpha and flat graphic normals."""
from pathlib import Path
import json,hashlib,sys
O=Path(__file__).resolve().parent;B=O.parent;S=B/'source'
sys.path.insert(0,str(B.parent/'preview19/python-deps'))
from PIL import Image,ImageFilter
import numpy as np
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs={r['identity']:r for r in json.loads((B/'visual-polish-r31/LIBRARY-AUDIT.json').read_text())['missing']}
records=list({r['identity']:r for r in json.loads((O/'GENERATED-ART.json').read_text())}.values())
out=O/'runtime/randy-vk/packs/ao-cyber-world';out.mkdir(parents=True,exist_ok=True);results=[]
for r in records:
 native=Image.open(r['original']).convert('RGBA');raw=Image.open(S/r['source']).convert('RGB');crop=None
 nw,nh=native.size;ratio=nw/nh
 # The generator has a 3:1 canvas limit. Remove only added solid-black matte
 # when the native physical material has a non-black boundary. Otherwise keep
 # the native normalized composition (including intentional effect black).
 arr=np.asarray(raw);src=np.asarray(native.convert('RGB'))
 rim=np.concatenate([src[0],src[-1],src[:,0],src[:,-1]])
 if abs(raw.width/raw.height-ratio)>.04 and (float(rim.mean())>18 or r['id']==41965):
  mask=arr.max(2)>8;ys,xs=np.where(mask)
  if len(xs):
   left,right,top,bottom=int(xs.min()),int(xs.max()+1),int(ys.min()),int(ys.max()+1)
   if ratio>=1 and (top>8 or bottom<raw.height-8):
    height=min(raw.height,round(raw.width/ratio));cy=(top+bottom)/2;start=max(0,min(raw.height-height,round(cy-height/2)));crop=[0,start,raw.width,start+height]
   elif ratio<1 and (left>8 or right<raw.width-8):
    width=min(raw.width,round(raw.height*ratio));cx=(left+right)/2;start=max(0,min(raw.width-width,round(cx-width/2)));crop=[start,0,start+width,raw.height]
 if crop:raw=raw.crop(crop)
 longest=512 if max(native.size)<=32 else 1024
 size=(max(1,round(longest*nw/max(nw,nh))),max(1,round(longest*nh/max(nw,nh))))
 assert raw.width>=size[0] and raw.height>=size[1],(r['identity'],raw.size,size,'never enlarge a small render')
 rgb=np.asarray(raw.resize(size,Image.Resampling.LANCZOS),dtype=np.float32)
 # Bounded albedo calibration retains the original broad brightness/palette.
 before=src.mean((0,1));after=rgb.mean((0,1));gain=np.clip((before+2)/(after+2),.5,1.2)
 rgb=np.clip(rgb*gain[None,None,:],0,255)
 workflow='individual built-in image render, downsampled and albedo-calibrated'
 # These are connected cartographic quadrants or an exact mechanical UV atlas.
 # Keep the native layout and transfer only bounded new sub-texel detail.
 if r['id'] in [296030,88063,88064,88065,88066]:
  base=np.asarray(native.convert('RGB').resize(size,Image.Resampling.BILINEAR),dtype=np.float32)
  fine=rgb-np.asarray(Image.fromarray(np.uint8(rgb)).filter(ImageFilter.GaussianBlur(2)),dtype=np.float32)
  fine=np.clip(fine,-12,12)
  h,w=size[1],size[0];yy,xx=np.mgrid[:h,:w];edge=np.minimum.reduce([xx,yy,w-1-xx,h-1-yy]);fade=np.clip((edge-8)/8,0,1)
  rgb=np.clip(base+fine*fade[:,:,None],0,255)
  workflow='new rendered fine detail on native layout; connected edges and UV islands retained'
 # Transparent keyed pixels remain byte-equivalent in RGB as well as alpha.
 alpha=np.asarray(native.getchannel('A').resize(size,Image.Resampling.NEAREST))
 native_rgb=np.asarray(native.convert('RGB').resize(size,Image.Resampling.NEAREST))
 rgb=np.uint8(rgb+.5);rgb[alpha==0]=native_rgb[alpha==0]
 diffuse=Image.fromarray(rgb);diffuse.putalpha(native.getchannel('A').resize(size,Image.Resampling.NEAREST))
 aux=(max(1,size[0]//2),max(1,size[1]//2))
 normal=Image.new('RGB',aux,(128,128,255));orm=Image.new('RGB',aux,(255,217,0));maps={};stem=r['identity'].replace(':','_')
 for role,im in [('d',diffuse),('n',normal),('orm',orm)]:
  p=out/(stem+'_'+role+'.png');im.save(p,optimize=True);maps[role]={'path':str(p.resolve()),'sha256':sha(p),'size':list(im.size),'bytes':p.stat().st_size}
 assert np.array_equal(np.asarray(Image.open(maps['d']['path']).getchannel('A')),np.asarray(native.getchannel('A').resize(size,Image.Resampling.NEAREST)))
 result=dict(r,name=inputs[r['identity']].get('name') or '',type=1010004,pack='ao-cyber-world',maps=maps,sourceSha256=sha(S/r['source']),originalSha256=sha(Path(r['original'])),exportCrop=crop,exportSize=list(size),albedoChannelGains=gain.tolist(),nativeAlphaPreserved=True,flatGraphicNormals=True,workflow=workflow,retailMeshFitCertified=False)
 results.append(result)
report={'result':'passed','materials':results,'count':len(results),'recipeSha256':sha(Path(__file__)),'rejectedVariantIds':[130481,283659,90436],'correctedVariantsUsed':True}
(O/'ART-BAKE.json').write_text(json.dumps(report,indent=2)+'\n');print('Exported verified individual material sets',len(results))
