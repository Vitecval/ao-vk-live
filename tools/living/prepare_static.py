"""Privately resolve original AO service vehicles, with existing local HD maps."""
import struct,json,hashlib,math
import numpy as np
from PIL import Image
from ao_living_assets import parse_static,scalar,array
from prepare_local import texture_maps,original_image


def opaque_material(material,objects):
    # Native material.cpp ConstructFrom uses alpha as a secondary material
    # channel when a delta does not enable blending. Preserve explicit cutouts.
    ds=scalar(material,'delta_state',-1)
    if ds<0:return False
    states=dict(zip((v[0] for v in array(objects[ds],'rst_type')),(v[0] for v in array(objects[ds],'rst_value'))))
    return not (states.get(27,0) or states.get(15,0))

def build_static(db,client,item,out):
    mesh=parse_static(db.get(1010001,item['model']));dim=item.get('dimension',512);materials={};vertices=[];indices=[];receipts=[]
    unique=set()
    for p in mesh['parts']:
        uv=p['vertices'][:,6:8];lo=np.floor(uv.min(0)+.0001);repeat=np.maximum(np.ceil(uv.max(0)-.0001)-lo,1).astype(int)
        unique.add((p['texture'],tuple(lo),tuple(repeat),opaque_material(p['material'],mesh['objects'])))
    grid=math.ceil(math.sqrt(len(unique)));side=dim//grid;gutter=4
    images=[Image.new('RGBA' if i==0 else 'RGB',(dim,dim),c) for i,c in enumerate(((0,0,0,255),(128,128,255),(255,205,0),(0,0,0)))]
    for part in mesh['parts']:
        if part['texture'] is None:raise ValueError('original service vehicle has unresolved material')
        v=part['vertices'].copy();kind,ident=part['texture'];uv=v[:,6:8];lo=np.floor(uv.min(0)+.0001);repeat=np.maximum(np.ceil(uv.max(0)-.0001)-lo,1).astype(int)
        if np.any(repeat>4):raise ValueError('vehicle UV repeat exceeds atlas cap')
        opaque=opaque_material(part['material'],mesh['objects'])
        key=(kind,ident,tuple(lo),tuple(repeat),opaque)
        if key not in materials:
            tile=len(materials)
            if tile>=grid*grid:raise ValueError('unexpected material UV variants')
            maps,receipt=texture_maps(db,client,ident,kind)
            if opaque:
                mask=original_image(db,ident,kind)[0].getchannel('A').resize(maps[2].size,Image.Resampling.LANCZOS)
                maps[0].putalpha(255)
                # The original non-transparency alpha is a material mask,
                # useful for hardware response, never a whole-hull glow mask.
                root=client/'randy-vk/packs'/receipt['hd'].get('pack','')
                from prepare_local import verified_hd
                if mask.getextrema()[0]!=mask.getextrema()[1] and (not receipt['hd'] or not verified_hd(root,root/f'{kind}_{ident}_orm.png')):
                    a=np.asarray(maps[2]).copy();m=np.asarray(mask).astype(float)/255
                    a[:,:,1]=np.clip(205-110*m,70,205).astype(np.uint8);a[:,:,2]=(m*165).astype(np.uint8)
                    maps[2].paste(Image.fromarray(a))
            receipt['original_alpha_as_material']=opaque;receipts.append(receipt)
            for image,source in zip(images,maps):
                tiled=Image.new(source.mode,(source.width*repeat[0],source.height*repeat[1]))
                for y in range(repeat[1]):
                    for x in range(repeat[0]):tiled.paste(source,(x*source.width,y*source.height))
                tileim=tiled.resize((side-2*gutter,side-2*gutter),Image.Resampling.LANCZOS);edge=tileim.resize((side,side));edge.paste(tileim,(gutter,gutter));image.paste(edge,((tile%grid)*side,(tile//grid)*side))
            materials[key]=tile
        tile=materials[key];v[:,6:8]=((uv-lo)/repeat*(side-2*gutter)+gutter+np.array((tile%grid,tile//grid))*side)/dim
        indices.extend(part['indices']+len(vertices));vertices.extend(v)
    v=np.asarray(vertices,dtype='<f4');idx=np.array(indices,dtype='<u2');bounds=[v[:,:3].min(0).tolist(),v[:,:3].max(0).tolist()];v[:,:3]-=(v[:,:3].min(0)+v[:,:3].max(0))/2
    if len(v)>20000 or len(idx)>60000:raise ValueError('vehicle geometry budget exceeded')
    target=out/(item['name']+'.lrm')
    with target.open('wb') as f:
        f.write(struct.pack('<8IfI',0x324d524c,len(v),len(idx),len(idx),len(idx),dim,0,0,0,0));f.write(v.tobytes())
        for _ in range(3):f.write(idx.tobytes())
        for image in images:
            rgba=np.asarray(image.convert('RGBA'),dtype=np.uint32);f.write((rgba[:,:,3]<<24|rgba[:,:,0]<<16|rgba[:,:,1]<<8|rgba[:,:,2]).astype('<u4').tobytes())
    images[0].save(out/(item['name']+'-atlas.png'))
    receipt=dict(name=item['name'],original_model=item['model'],vertices=len(v),triangles=len(idx)//3,original_bounds=bounds,textures=receipts,private_derived_asset=True,sha256=hashlib.sha256(target.read_bytes()).hexdigest());print(json.dumps(receipt),flush=True);return receipt
