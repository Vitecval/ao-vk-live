"""Bake generated material masters into native transition layouts, with edge proofs.

These are shared-master detail restorations, not 714 independent image renders.
The original coarse painting and all colour/control borders remain unchanged.
"""
from pathlib import Path
import json,hashlib,sys,concurrent.futures
O=Path(__file__).resolve().parent;B=O.parent;S=B/'source'
sys.path.insert(0,str(B.parent/'preview19/python-deps'))
from PIL import Image,ImageFilter
import numpy as np
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=json.loads((O/'ground-inputs.json').read_text())
masters=json.loads((O/'GENERATED-MASTERS.json').read_text());fields={}
for r in masters:
 p=S/r['source'];im=Image.open(p).convert('RGB');assert min(im.size)>=1024
 im=im.resize((512,512),Image.Resampling.LANCZOS)
 detail=np.asarray(im,dtype=np.float32)-np.asarray(im.filter(ImageFilter.GaussianBlur(2)),dtype=np.float32)
 fields[r['name'].removeprefix('ground-')]=np.clip(detail,-32,32)
 r['sourceSha256']=sha(p);r['generatedSize']=list(Image.open(p).size)
assert len(fields)==8
out=O/'runtime/randy-vk/packs/ao-cyber-world';out.mkdir(parents=True,exist_ok=True)

def bake(r):
 identity=r['identity'];kind,num=map(int,identity.split(':'))
 if num in (27,774):return None
 p=Path(r['original']);original=Image.open(p).convert('RGBA');assert original.size in [(256,256),(128,128)]
 assert original.getchannel('A').getextrema()==(255,255)
 baseIm=original.convert('RGB').resize((512,512),Image.Resampling.BILINEAR);base=np.asarray(baseIm,dtype=np.float32)
 # Continuous masks derived from the native painting, never random tile labels.
 coarse=np.asarray(baseIm.filter(ImageFilter.GaussianBlur(3)),dtype=np.float32)/255
 red,green,blue=np.moveaxis(coarse,-1,0);lum=coarse.mean(2)
 grass=np.clip((green-np.maximum(red,blue)-.015)*9,0,1)
 snow=np.clip((lum-.64)*5,0,1)*(1-grass)
 sand=np.clip((red-blue-.07)*5,0,1)*(1-grass)*(1-snow)
 dark=np.clip((.32-lum)*4,0,1)*(1-grass)*(1-snow)*(1-sand)
 asphalt=np.clip((.22-np.abs(red-blue))*3,0,1)*dark
 litter=dark*(1-asphalt)*.6
 soil=dark*(1-asphalt)*.4
 rock=(1-grass)*(1-snow)*(1-sand)*(1-dark)
 gravel=rock*np.clip((lum-.42)*2,0,.6);rock=rock-gravel
 masks={'grass':grass,'snow':snow,'sand':sand,'asphalt':asphalt,'litter':litter,'soil':soil,'rock':rock,'gravel':gravel}
 total=sum(masks.values());total=np.maximum(total,1e-5)
 detail=sum(fields[k]*(v/total)[:,:,None] for k,v in masks.items())
 # A bounded added fine signal, zero mean in every native texel footprint.
 yy,xx=np.indices((512,512));edge=np.minimum.reduce((xx,yy,511-xx,511-yy));border=8
 fade=np.clip((edge-border)/8,0,1).astype(np.float32)
 delta=np.clip(detail*.85,-16,16)*fade[:,:,None]
 factor=512//original.width
 blocks=delta.reshape(original.height,factor,original.width,factor,3);delta-=blocks.mean((1,3)).repeat(factor,0).repeat(factor,1)
 delta[edge<=border]=0
 rgb=np.uint8(np.clip(base+delta,0,255)+.5)
 assert np.array_equal(rgb[edge<=border],np.uint8(base+.5)[edge<=border])
 d=Image.fromarray(rgb);coarseRms=float(np.sqrt(np.mean((np.asarray(d.resize(original.size,Image.Resampling.BOX),float)-np.asarray(baseIm.resize(original.size,Image.Resampling.BOX),float))**2)))
 assert coarseRms<1
 high=delta.mean(2)/255;dy,dx=np.gradient(high)
 n=np.stack((-dx*.45,dy*.45,np.ones_like(dx)),2);n/=np.linalg.norm(n,axis=2,keepdims=True)
 # Manufactured tread/tiles retain their flat normal to avoid fake ground relief.
 if num in {17,68,69,71,174,176,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,209,211}:n[:]=[0,0,1]
 normal=Image.fromarray(np.uint8(np.clip(n*.5+.5,0,1)*255+.5)).resize((256,256),Image.Resampling.BOX)
 orm=Image.new('RGB',(256,256),(255,230,0));stem=identity.replace(':','_');maps={}
 for role,im in [('d',d),('n',normal),('orm',orm)]:
  dest=out/f'{stem}_{role}.png';im.save(dest,optimize=True)
  maps[role]={'path':str(dest.resolve()),'sha256':sha(dest),'size':list(im.size),'bytes':dest.stat().st_size}
 return {'identity':identity,'type':kind,'id':num,'name':r.get('name') or 'Native terrain transition '+str(num),'original':str(p),'originalSha256':sha(p),'workflow':'generated shared-master fine detail; native coarse painting retained','maps':maps,'edgePixelValuesPreserved':True,'borderPixels':border,'coarseColourRmsError':coarseRms,'alphaPreserved':True,'heightOrCoverageDataChanged':False,'generatedMasterNames':list(fields),'retailMeshFitCertified':False}

if __name__=='__main__':
 result=[]
 with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
  for i,r in enumerate(pool.map(bake,rows),1):
   if r:result.append(r)
   if i%40==0:print('Baked and verified ground',i,'/',len(rows),flush=True)
 report={'result':'passed','materials':result,'count':len(result),'independentMasterRenders':8,'controlsRetained':[27,774],'edgePixelValuesPreserved':True,'maxCoarseColourRmsError':max(r['coarseColourRmsError'] for r in result),'recipeSha256':sha(Path(__file__)),'masters':masters}
 (O/'GROUND-BAKE.json').write_text(json.dumps(report,indent=2)+'\n');print('DONE',len(result),flush=True)
