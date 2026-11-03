"""Privately resolve original AO service vehicles, with existing local HD maps."""
import struct,json,hashlib,math
import numpy as np
from PIL import Image
from ao_living_assets import parse_static
from prepare_local import texture_maps

def build_static(db,client,item,out):
    mesh=parse_static(db.get(1010001,item['model']));dim=item.get('dimension',512);materials={};vertices=[];indices=[];receipts=[]
    unique=list(dict.fromkeys(p['texture'] for p in mesh['parts']));grid=math.ceil(math.sqrt(len(unique)));side=dim//grid;gutter=4
    images=[Image.new('RGBA' if i==0 else 'RGB',(dim,dim),c) for i,c in enumerate(((0,0,0,255),(128,128,255),(255,205,0),(0,0,0)))]
    for part in mesh['parts']:
        if part['texture'] is None:raise ValueError('original service vehicle has unresolved material')
        v=part['vertices'].copy();kind,ident=part['texture'];uv=v[:,6:8];lo=np.floor(uv.min(0)+.0001);repeat=np.maximum(np.ceil(uv.max(0)-.0001)-lo,1).astype(int)
        if np.any(repeat>4):raise ValueError('vehicle UV repeat exceeds atlas cap')
        key=(kind,ident,tuple(lo),tuple(repeat))
        if key not in materials:
            tile=len(materials)
            if tile>=grid*grid:raise ValueError('unexpected material UV variants')
            maps,receipt=texture_maps(db,client,ident,kind);receipts.append(receipt)
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
