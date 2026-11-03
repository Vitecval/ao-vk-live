"""Prepare a private cosmetic cache from the owner's AO installation (read-only).
No game files are patched. Generated .lrm files contain game assets and must never
be included in a distributable archive. Public catalogs contain IDs only.
"""
import argparse,io,json,struct,hashlib,math
from pathlib import Path
import numpy as np
from PIL import Image
from ao_living_assets import Database,parse_mesh,parse_static,parse_animation,palettes,transform
_material_manifests={}

def verified_hd(root,path):
    try:
        if root not in _material_manifests:
            manifest=root/'manifest.json'
            if not manifest.is_file() or manifest.stat().st_size>16*1024*1024:return False
            _material_manifests[root]=json.loads(manifest.read_text())['files']
        expected=_material_manifests[root].get(path.name)
        return bool(expected and path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==expected)
    except (OSError,ValueError,KeyError):return False

def loop(anim):
    events={name.strip().lower():time for time,name in anim['events']}
    a=events.get('loopstart',0.);b=events.get('loopend',anim['length'])
    if not 0<=a<b<=anim['length']+1:raise ValueError('invalid original loop markers')
    return a,b

def original_image(db,identity,kind=None):
    for t in ([kind] if kind else [1010004,1010011,1010019,1010020]):
        if (t,identity) not in db.index:continue
        b=db.get(t,identity);starts=[b.find(sig,12) for sig in (b'\x89PNG\r\n\x1a\n',b'\xff\xd8\xff',b'BM')];starts=[i for i in starts if i>=0]
        if not starts:continue
        im=Image.open(io.BytesIO(b[min(starts):]));
        if not 1<=im.width<=4096 or not 1<=im.height<=4096:raise ValueError('original image exceeds cap')
        return im.convert('RGBA'),t
    raise ValueError(f'original texture {kind}:{identity} unavailable')

def texture_maps(db,client,identity,kind=None,base=None):
    im,t=original_image(db,identity,kind);source=im
    chosen={}
    for pack in ('ao-cyber-world','ao-cyber-characters','ao-modern-icc','ao-cyber-icc'):
        root=client/'randy-vk/packs'/pack;path=root/f'{t}_{identity}_d.png'
        if verified_hd(root,path):
            im=Image.open(path).convert('RGB').convert('RGBA');im.putalpha(source.getchannel('A').resize(im.size,Image.Resampling.LANCZOS));chosen={'pack':pack};break
    if base:
        base=base.resize(im.size,Image.Resampling.LANCZOS);im=Image.alpha_composite(base,im)
    normal=Image.new('RGB',im.size,(128,128,255));orm=Image.new('RGB',im.size,(248,205,0));emissive=Image.new('RGB',im.size,(0,0,0))
    if chosen:
        for suffix,target in (('n','normal'),('orm','orm'),('e','emissive')):
            path=root/f'{t}_{identity}_{suffix}.png'
            if verified_hd(root,path):
                tex=Image.open(path).convert('RGB').resize(im.size,Image.Resampling.LANCZOS)
                if target=='normal':normal=tex
                elif target=='orm':orm=tex
                else:emissive=tex
    return (im,normal,orm,emissive),{'type':t,'id':identity,'hd':chosen}

def skin_points(vertices,bids,matrices):
    p0=np.c_[vertices[:,:3],np.ones(len(vertices))];p1=np.c_[vertices[:,3:6],np.ones(len(vertices))]
    a=np.einsum('ni,nij->nj',p0,matrices[bids[:,0]])[:,:3];b=np.einsum('ni,nij->nj',p1,matrices[bids[:,1]])[:,:3];w=vertices[:,16:17]
    return a*w+b*(1-w)

def build(db,client,item,out):
    model=parse_mesh(db.get(1010002,item['model']));walk=parse_animation(db.get(1010003,item['walk']));idle=parse_animation(db.get(1010003,item['idle']));dim=item.get('dimension',1024)
    if model['skeleton']!=walk['skeleton'] or model['skeleton']!=idle['skeleton']:raise ValueError('model/walk/idle skeleton identities differ')
    if len(model['bones'])>64:raise ValueError('ambient bone limit exceeded')
    # Vehicle idles may contain one authored pose with zero duration. Keep
    # that pose; never substitute an unrelated humanoid animation.
    if item.get('posed_vehicle'):
        for clip in (walk,idle):
            if clip['length'] < 200:
                clip['length']=1000.;clip['events']=[]
    images=[Image.new('RGBA' if i==0 else 'RGB',(dim,dim),c) for i,c in enumerate(((0,0,0,255),(128,128,255),(255,205,0),(0,0,0)))];tiles={};textures=[]
    def add_material(identity,kind=None,base=None,repeat=(1,1),material=None):
        opaque=bool(material and material['flags']&8)
        glow=bool(item.get('posed_vehicle') and material and material['name'].lower() in ('engine','glow'))
        key=(identity,kind,repeat,opaque,glow,hashlib.sha256(base.tobytes()).digest() if base else None)
        if key in tiles:return tiles[key]
        tile=len(tiles)
        if tile>=16:raise ValueError('too many original material slots')
        maps,receipt=texture_maps(db,client,identity,kind,base)
        if opaque:
            # CAT flag 8 explicitly disables alpha transparency (cat_data.cpp).
            # Its alpha supplies the environment blend, not holes in the hull.
            mask=np.asarray(original_image(db,identity,kind)[0].getchannel('A'))
            maps[0].putalpha(255)
            if not receipt['hd'] and mask.min()!=mask.max():
                a=np.asarray(maps[2]).copy();m=Image.fromarray(mask).resize(maps[2].size)
                a[:,:,1]=np.clip(205-np.asarray(m)*.45,70,205).astype(np.uint8)
                a[:,:,2]=(np.asarray(m)*.65).astype(np.uint8);maps[2].paste(Image.fromarray(a))
        if glow:
            a=np.asarray(maps[0].convert('RGB')).astype(float)*.12
            maps[3].paste(Image.fromarray(np.clip(a,0,32).astype(np.uint8)))
        receipt.update(original_alpha_as_environment=opaque,original_engine_geometry=glow)
        textures.append(receipt)
        side=dim//4;x=(tile%4)*side;y=(tile//4)*side;gutter=4
        for target,im in zip(images,maps):
            if repeat!=(1,1):
                tiled=Image.new(im.mode,(im.width*repeat[0],im.height*repeat[1]))
                for ty in range(repeat[1]):
                    for tx in range(repeat[0]):tiled.paste(im,(tx*im.width,ty*im.height))
                im=tiled
            im=im.resize((side-2*gutter,side-2*gutter),Image.Resampling.LANCZOS)
            # Extend edge texels into a gutter; avoids material bleeding in UV islands.
            edge=im.resize((side,side),Image.Resampling.NEAREST);edge.paste(im,(gutter,gutter));target.paste(edge,(x,y))
        tiles[key]=tile;return tile
    vertices=[];ids=[];indices=[]
    slots={s[0]:s[1] for s in model['slots']};overrides=item.get('textures',{})
    selected=item.get('groups')
    for group in model['groups']:
        if selected and group['name'] not in selected:continue
        for part in group['pieces']:
            mat=model['materials'][part['material']];name=mat['name'];original=slots.get(name)
            if not original:raise ValueError('material slot missing from original CIR')
            override=overrides.get(name,original)
            kind=override.get('type') if isinstance(override,dict) else None
            identity=override['id'] if isinstance(override,dict) else override
            skin=item.get('skin',{}).get(name)
            # A CIR can already contain a complete robe or armor material.
            # Skin is the underlay for modular overrides, never a replacement
            # for the body's own authored clothing/armor atlas.
            base=original_image(db,skin,1010011)[0] if skin and identity!=skin and identity!=original else original_image(db,original)[0] if identity!=original else None
            v=part['vertices'].copy();bi=part['bone_ids'].copy();bi[v[:,16]>.99,1]=bi[v[:,16]>.99,0]
            if bi.min()<0 or bi.max()>=len(model['bones']):raise ValueError('skin bone outside independent skeleton')
            side=dim//4;g=4;uv=v[:,12:14].copy()
            lo=np.floor(uv.min(0)+.0001);hi=np.ceil(uv.max(0)-.0001);repeat=np.maximum(hi-lo,1).astype(int)
            if np.any(repeat>4):raise ValueError('original UV repeat exceeds 4x4 budget')
            tile=add_material(identity,kind=kind,base=base,repeat=tuple(repeat),material=mat);uv=(uv-lo)/repeat
            v[:,12]=(tile%4+(g+uv[:,0]*(side-2*g))/side)/4;v[:,13]=(tile//4+(g+uv[:,1]*(side-2*g))/side)/4
            indices.extend((part['indices']+sum(len(x) for x in vertices)).tolist());vertices.append(v);ids.append(bi)
    if item.get('head'):
        attach=next(a for a in model['attractors'] if 'head' in a['name'].lower());p=attach['transform'];at=transform(p[:3],p[3:7],p[7]);head=parse_static(db.get(1010001,item['head']))
        for part in head['parts']:
            if not part['texture']:raise ValueError('head has unresolved original material')
            kind,identity=part['texture'];tile=add_material(identity,kind);src=part['vertices'];nv=len(src);v=np.zeros((nv,17),np.float32);pos=(np.c_[src[:,:3],np.ones(nv)]@at)[:,:3]
            v[:,:3]=v[:,3:6]=pos;v[:,6:9]=(np.c_[pos,np.ones(nv)]@palettes(model,walk,loop(walk)[0])[attach['bone']])[:,:3];v[:,9:12]=src[:,3:6]@at[:3,:3]
            side=dim//4;g=4;v[:,12]=(tile%4+(g+np.clip(src[:,6],0,1)*(side-2*g))/side)/4;v[:,13]=(tile//4+(g+np.clip(src[:,7],0,1)*(side-2*g))/side)/4;v[:,16]=1
            indices.extend((part['indices']+sum(len(x) for x in vertices)).tolist());vertices.append(v);ids.append(np.full((nv,2),attach['bone'],np.int32))
    v=np.concatenate(vertices);bi=np.concatenate(ids);idx=np.asarray(indices,dtype='<u2');nb=len(model['bones'])
    wa,wb=loop(walk);ia,ib=loop(idle);nw=min(48,max(24,round((wb-wa)*.03)));ni=min(60,max(24,round((ib-ia)*.016)))
    if len(v)>20000 or len(idx)>60000:raise ValueError('original mesh exceeds ambient geometry budget')
    # Extract only actual looping locomotion. Animation events are never dispatched.
    frames=[];mins=[];foot_samples=[];horizontal_radius=0.;height=0.
    foot_indices=item.get('foot_bones',[]).copy()
    if item.get('grounded',True) and not foot_indices:
        for token in ('l foot','r foot'):
            found=[i for i,b in enumerate(model['bones']) if token in b['name'].lower()]
            if found:foot_indices.append(found[0])
    for anim,start,end,count in ((walk,wa,wb,nw),(idle,ia,ib,ni)):
        for i in range(count):
            pose=palettes(model,anim,start+(end-start)*i/count);skin=skin_points(v,bi,pose)
            floor=float(skin[:,1].min()) if item.get('grounded',True) else float((skin[:,1].max()+skin[:,1].min())/2)
            horizontal_radius=max(horizontal_radius,float(np.linalg.norm(skin[:,(0,2)],axis=1).max()))
            height=max(height,float(skin[:,1].max()-floor))
            pose[:,3,1]-=floor;mins.append(floor);frames.append(pose)
            if len(frames)<=nw and len(foot_indices)>=2:foot_samples.append(pose[foot_indices,3,:3].copy())
    # Distance phase curve follows the lowest foot's rearward motion during stance.
    curve=np.linspace(0,float(item.get('stride',1.0)),nw+1)
    gait_error=None
    if foot_samples:
        feet=np.asarray(foot_samples);dz=np.roll(feet[:,:,2],-1,axis=0)-feet[:,:,2];ys=(feet[:,:,1]+np.roll(feet[:,:,1],-1,axis=0))/2
        active=np.argmin(ys+np.where(dz<-.0001,0,1000),axis=1);steps=-dz[np.arange(nw),active]
        if np.median(steps)<0:raise ValueError('walking clip forward axis requires review')
        median=np.median(steps[steps>0]);steps=np.clip(steps,median*.2,median*2.0);curve=np.r_[0,np.cumsum(steps)];gait_error=float(np.max(np.abs(steps+dz[np.arange(nw),active])))
    stride=float(curve[-1]);duration=(wb-wa)/1000;idle_duration=(ib-ia)/1000
    target=out/(item['name']+'.lrm');out.mkdir(parents=True,exist_ok=True)
    with target.open('wb') as f:
        f.write(struct.pack('<8IfI',0x334d524c,len(v),len(idx),len(idx),len(idx),dim,nb,nw+ni,nw/duration,nw))
        f.write(struct.pack('<3fI',duration,idle_duration,stride,len(curve)));f.write(np.asarray(curve,dtype='<f4').tobytes())
        packed=v.astype('<f4').copy();packed.view('<i4')[:,14:16]=bi;f.write(packed.tobytes())
        for _ in range(3):f.write(idx.tobytes())
        for pose in frames:f.write(np.c_[pose[:,:3,:3].reshape(nb,9),pose[:,3,:3]].astype('<f4').tobytes())
        for im in images:
            rgba=np.asarray(im.convert('RGBA'),dtype=np.uint32);argb=(rgba[:,:,3]<<24)|(rgba[:,:,0]<<16)|(rgba[:,:,1]<<8)|rgba[:,:,2];f.write(argb.astype('<u4').tobytes())
    images[0].save(out/(item['name']+'-atlas.png'))
    receipt={'name':item['name'],'original_model':item['model'],'head':item.get('head'),'walk':item['walk'],'idle':item['idle'],'loop_ms':[wa,wb],'idle_loop_ms':[ia,ib],'walk_frames':nw,'idle_frames':ni,'stride':stride,'walk_m_per_second':stride/duration,'estimated_stance_phase_residual_m':gait_error,'vertices':len(v),'triangles':len(idx)//3,'bones':nb,'textures':textures,'private_derived_asset':True,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    receipt.update(horizontal_radius=horizontal_radius,animated_height=height)
    print(json.dumps(receipt),flush=True);return receipt

def main():
    p=argparse.ArgumentParser();p.add_argument('--client',type=Path,required=True);p.add_argument('--catalog',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    client=a.client.resolve();out=a.output.resolve()
    if out==client or client in out.parents:raise SystemExit('The local cache must be outside the game installation.')
    catalog=json.loads(a.catalog.read_text());db=Database(client/'cd_image/data/db');receipts=[build(db,client,item,out) for item in catalog['assets']]
    from prepare_static import build_static
    receipts.extend(build_static(db,client,item,out) for item in catalog.get('statics',[]))
    (out/'PRIVATE-NOT-FOR-DISTRIBUTION.txt').write_text('Contains assets read from the owner\'s local Anarchy Online installation. Do not redistribute.\n')
    (out/'asset-receipts.json').write_text(json.dumps({'database_index_sha256':db.index_sha256,'assets':receipts},indent=2))
if __name__=='__main__':main()
