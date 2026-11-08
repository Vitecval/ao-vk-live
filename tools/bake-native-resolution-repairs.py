"""Restore native atlas resolution with generated material grain and approved masks.
This is a technical material bake, not an independent generative repaint per ID.
"""
from pathlib import Path
import argparse,hashlib,json,sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--audit-dir',type=Path,required=True)
parser.add_argument('--seed-packs',type=Path,required=True)
args=parser.parse_args()
O=args.audit_dir.resolve();R=O.parents[2];sys.path.insert(0,str(R/'build/preview19/python-deps'))
from PIL import Image,ImageFilter
import numpy as np
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
queue=read(O/'UNDERSIZED-RENDER-QUEUE.json');registry=read(O/'DETAIL-MASTER-REGISTRY.json')['profileMasters']
blocked={(x['type'],x['id']) for x in read(O/'GENERATION-BLOCKS.json')}
seed=args.seed_packs.resolve()
output=O/'baked';output.mkdir(exist_ok=True);cache={};reports=[];plans=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def fine_master(path):
 if str(path) not in cache:
  im=Image.open(path).convert('L');w,h=im.size
  # Crop only a central material sample; large-scale atlas design never enters the bake.
  side=min(512,w,h);x=(w-side)//2;y=(h-side)//2;im=im.crop((x,y,x+side,y+side))
  a=np.asarray(im,dtype=np.float32)-np.asarray(im.filter(ImageFilter.GaussianBlur(1.8)),dtype=np.float32)
  a=np.clip(a,-12,12)*.6
  a=np.concatenate((a,a[:,::-1]),axis=1);a=np.concatenate((a,a[::-1,:]),axis=0)
  cache[str(path)]=a
 return cache[str(path)]
for num,job in enumerate(queue):
 key=job['type'],job['id'];identity=f'{key[0]}_{key[1]}';native=Image.open(O/'originals'/f'{identity}.png').convert('RGBA');w,h=native.size
 assert list(native.size)==job['originalSize']
 base=np.asarray(native.convert('RGB'),dtype=np.float32);alpha=np.asarray(native.getchannel('A'));grey=np.asarray(native.convert('L'),dtype=np.float32)
 control=(base.max(2)<6)|(alpha==0)
 journal=O/f'generation-{key[1]}.json';raw=O/'generated'/f'{identity}.png'
 if key in blocked:
  method='unmodified-native-original-retained-after-image-service-block';detail=np.zeros((h,w),np.float32);master=None;generation=None
 elif journal.exists() and raw.exists():
  method='individual-render-fine-detail-native-restoration';generation=read(journal);master=raw
  rendered=Image.open(raw);assert rendered.width>=w and rendered.height>=h
  fine=fine_master(raw);detail=np.tile(fine,((h+fine.shape[0]-1)//fine.shape[0],(w+fine.shape[1]-1)//fine.shape[1]))[:h,:w]
 elif job.get('profile')=='glass' or float(base.std((0,1)).max())<.05:
  method='native-original-structure-preserved-no-added-grain';detail=np.zeros((h,w),np.float32);master=None;generation=None
 else:
  master=O/registry[job.get('profile','')];generation=read(master.with_suffix('.json')) if master.parent.name=='detail-masters' else read(O/f"generation-{master.stem.split('_')[1]}.json")
  method='shared-rendered-material-grain-native-restoration';fine=fine_master(master)
  fine=np.rot90(fine,key[1]%4);detail=np.tile(fine,((h+fine.shape[0]-1)//fine.shape[0],(w+fine.shape[1]-1)//fine.shape[1]))[:h,:w]
 # Fade grain at existing high-contrast artwork edges, controls and atlas bounds.
 dy,dx=np.gradient(grey);edge=np.clip((np.maximum(abs(dx),abs(dy))-8)/24,0,1)
 yy,xx=np.mgrid[:h,:w];border=np.minimum.reduce((xx,yy,w-1-xx,h-1-yy));weight=np.clip((border-1)/6,0,1)*(1-edge)
 detail=detail*weight;detail[control]=0
 rgb=np.uint8(np.clip(base+detail[:,:,None],0,255)+.5);diffuse=Image.fromarray(rgb);diffuse.putalpha(native.getchannel('A'))
 # Preserve approved normal/height and AO/roughness/metalness masks.
 parent=seed/job['pack'];normal_source=parent/f'{identity}_n.png';orm_source=parent/f'{identity}_orm.png'
 assert normal_source.is_file() and orm_source.is_file(),('Missing approved masks',key)
 normal_old=Image.open(normal_source);normal=normal_old.convert('RGB').resize((w,h),Image.Resampling.BILINEAR)
 vector=np.asarray(normal,dtype=np.float32)/127.5-1;gy,gx=np.gradient(detail/255)
 vector[:,:,0]-=gx*.45;vector[:,:,1]+=gy*.45;vector/=np.maximum(np.linalg.norm(vector,axis=2,keepdims=True),1e-6)
 normal=Image.fromarray(np.uint8(np.clip(vector*.5+.5,0,1)*255+.5))
 if 'A' in normal_old.getbands():normal.putalpha(normal_old.getchannel('A').resize((w,h),Image.Resampling.BILINEAR))
 orm=Image.open(orm_source).convert('RGB').resize((w,h),Image.Resampling.BILINEAR)
 maps={}
 for role,im in {'d':diffuse,'n':normal,'orm':orm}.items():
  path=output/f'{identity}_{role}.png';im.save(path,optimize=True);maps[role]={'path':str(path),'sha256':sha(path),'size':list(im.size)}
 assert np.array_equal(np.asarray(diffuse.getchannel('A')),alpha)
 assert np.max(abs(rgb.astype(float)-base))<=8
 if key in blocked:assert np.array_equal(rgb,base.astype(np.uint8))
 record={'type':key[0],'id':key[1],'pack':job['pack'],'name':job.get('name',''),'sourceOriginalSize':[w,h],'maps':maps,'workflow':method,'detailMaster':str(master) if master else None,'detailMasterSha256':sha(master) if master else None,'originalImageSha256':sha(O/'originals'/f'{identity}.png'),'generation':generation,'layoutAndAlpha':'native original preserved','materialMaskPreservedFrom':str(orm_source),'materialMaskSourceSha256':sha(orm_source),'diffuseMaxNativeDelta':float(np.max(abs(rgb.astype(float)-base))),'state':'baked-awaiting-native-validation','independentRepaintClaimed':False}
 reports.append(record);plans.append({k:v for k,v in record.items() if k not in ('maps','generation')})
 if (num+1)%25==0:print('Exported and checked',num+1,'of',len(queue),'native-size materials',flush=True)
(O/'REPAIRED-MATERIALS.json').write_text(json.dumps({'materials':reports},indent=2)+'\n')
(O/'RESTORATION-PLAN.json').write_text(json.dumps({'result':'passed','materials':len(reports),'technicalNativeRestoration':True,'independentRepaintPerId':False,'blockedOriginalsRetained':len(blocked),'records':plans},indent=2)+'\n')
print('Full native-size restoration exports:',len(reports),flush=True)
