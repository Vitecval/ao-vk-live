"""Bake generated completion artwork through the source-layout-preserving workflow.

Original UV layout/coverage remains authoritative; the generated image supplies
bounded fine surface detail. This is runtime material baking, not new generation.
"""
import argparse,hashlib,json,sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('--plan',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--dependencies',type=Path)
    args=p.parse_args()
    if args.dependencies:sys.path.insert(0,str(args.dependencies.resolve()))
    import numpy as np
    from PIL import Image,ImageFilter
    plan=json.loads(args.plan.read_text());out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);records=[]
    for item in plan:
        if not item.get('generated_path'):continue
        t,i=item['type'],item['id'];original=Image.open(item['original']).convert('RGBA');generated=Image.open(item['generated_path']).convert('RGB')
        # Two wide tool outputs contain black letterbox rows. Remove only the
        # documented pure outer padding, then use normalized source coordinates.
        crop=None
        if i in {268794,270873}:
            a=np.asarray(generated);active=np.where((a.max(2)>24).mean(1)>.05)[0]
            assert len(active)
            crop=(0,int(active[0]),generated.width,int(active[-1])+1);generated=generated.crop(crop)
        scale=min(4.,1024/max(original.size));size=(max(1,round(original.width*scale)),max(1,round(original.height*scale)))
        old=original.convert('RGB').resize(size,Image.Resampling.LANCZOS);gen=generated.resize(size,Image.Resampling.LANCZOS)
        radius=max(1.5,scale*.7)
        detail=np.asarray(gen,dtype=np.float32)-np.asarray(gen.filter(ImageFilter.GaussianBlur(radius)),dtype=np.float32)
        # Keep generated low-frequency layout drift out of the retail mesh.
        detail=np.clip(detail,-24,24)*.65
        base=np.asarray(old,dtype=np.float32)
        source=np.asarray(original.convert('RGB'));flat=(source.max(2)-source.min(2)<4)&(source.max(2)<6)
        green=(source[:,:,1]>235)&(source[:,:,0]<20)&(source[:,:,2]<20)
        cyan=(source[:,:,1]>235)&(source[:,:,2]>235)&(source[:,:,0]<20)
        lock=np.asarray(Image.fromarray(np.uint8(flat|green|cyan)*255).resize(size,Image.Resampling.NEAREST))>0
        detail[lock]=0
        rgb=np.uint8(np.clip(base+detail,0,255)+.5);diffuse=Image.fromarray(rgb);diffuse.putalpha(original.getchannel('A').resize(size,Image.Resampling.NEAREST))
        delta=float(np.std(rgb.astype(float)-base));assert delta>.1,(t,i,'No generated detail')
        maps=out/'maps';maps.mkdir(exist_ok=True);stem=f'{t}_{i}'
        diffuse.save(maps/(stem+'_d.png'),optimize=True)
        lum=np.asarray(diffuse.convert('L'),dtype=np.float32)/255
        high=lum-np.asarray(diffuse.convert('L').filter(ImageFilter.GaussianBlur(2)),dtype=np.float32)/255
        dy,dx=np.gradient(high);normal=np.stack((-dx*.6,dy*.6,np.ones_like(dx)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
        skin=t==1010011 or i==29705
        graphic=i in {32239,203656}
        mineral=t==1010009
        metal=0.0 if skin or mineral or graphic else .25
        rough=.72 if skin or graphic else .75 if mineral else .58
        if graphic:normal[:]=[0,0,1]
        if i in {297474,297613,297624,297628}:normal[:round(size[1]*.34),:round(size[0]*.33)]=[0,0,1]
        normal[lock]=[0,0,1]
        orm=np.stack((np.ones_like(lum),np.full_like(lum,rough),np.full_like(lum,metal)),axis=2)
        companion=(max(1,size[0]//2),max(1,size[1]//2))
        for role,pixels in [('n',normal*.5+.5),('orm',orm)]:Image.fromarray(np.uint8(np.clip(pixels,0,1)*255+.5)).resize(companion,Image.Resampling.BOX).save(maps/(stem+'_'+role+'.png'),optimize=True)
        files={role:dict(path=str(maps/(stem+'_'+role+'.png')),sha256=hashlib.sha256((maps/(stem+'_'+role+'.png')).read_bytes()).hexdigest()) for role in ['d','n','orm']}
        records.append(dict(type=t,id=i,name=item['name'],maps=files,original=item['original'],generated=item['generated_path'],source_layout_preserved=True,original_alpha_preserved=True,generation_crop=crop,generated_detail_std=delta,size=size,retail_mesh_fit_certified=False))
    (out/'manifest.json').write_text(json.dumps(dict(records=records,generation_tool='built-in image_gen',workflow='bounded generated surface detail with original UV/layout/color/alpha controls; conservative companion maps'),indent=2)+'\n')
    print('Baked',len(records),'new original-ID materials')
if __name__=='__main__':main()
