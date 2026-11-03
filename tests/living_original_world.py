"""Original Newland renderer fixture held in RAM, adapted from V5 export_world.

No exported world, terrain or decoded material is written to disk.
"""
import struct
import io
import numpy as np
from PIL import Image
from ao_living_assets import parse_static
from survey_world import placements,ground
from prepare_local import original_image

def world_bytes(db,client,ground_source,center=(390,330),radius=185,pf=566):
    g=ground(db,pf,ground_source);models={};textures={};parts=[];center=np.array(center)
    for obj in placements(client/f'cd_image/data/statels/{pf}.pf'):
        ident=obj['id']
        if ident==0:continue
        if ident not in models:models[ident]=parse_static(db.get(1010001,ident))
        for part in models[ident]['parts']:
            v=part['vertices'];w=obj['matrix'];p=np.c_[v[:,:3],np.ones(len(v))]@w
            if np.any(p[:,(0,2)].min(0)>center+radius) or np.any(p[:,(0,2)].max(0)<center-radius):continue
            identity=part['texture']
            if identity not in textures:
                if identity is None:im=Image.new('RGBA',(1,1),(140,140,130,255))
                else:im=original_image(db,identity[1],identity[0])[0]
                im.thumbnail((128,128));textures[identity]=im
            parts.append((v,part['indices'],w,textures[identity]))
    H=g['heights'];C=g['cell'];tiles=g['tiles'];images={}
    for z in range(max(0,int((center[1]-radius)/C)),min(tiles.shape[0],int((center[1]+radius)/C))):
        for x in range(max(0,int((center[0]-radius)/C)),min(tiles.shape[1],int((center[0]+radius)/C))):
            identity=g['tex'][int(tiles[z,x])&0x3fff]
            if identity not in images:
                raw=db.get(1010006,identity);start=raw.find(b'\xff\xd8\xff');im=Image.open(io.BytesIO(raw[start:])).convert('RGBA');im.thumbnail((64,64));images[identity]=im
            a,b,d,c=H[z,x],H[z,x+1],H[z+1,x],H[z+1,x+1]
            v=np.array([[x*C,a,z*C,0,1,0,0,0],[(x+1)*C,b,z*C,0,1,0,1,0],[(x+1)*C,c,(z+1)*C,0,1,0,1,1],[x*C,d,(z+1)*C,0,1,0,0,1]],dtype='<f4')
            ad,bd=abs(a-c),abs(b-d);across=(ad>C or bd>C) and ad<bd-.01
            idx=[0,2,1,0,3,2] if across else [0,3,1,1,3,2]
            parts.append((v,np.array(idx,dtype='<u2'),np.eye(4),images[identity]))
    blob=bytearray(b'RVKT'+struct.pack('<II',2,len(parts)))
    for v,idx,w,im in parts:
        a=np.asarray(im,dtype=np.uint32);argb=a[:,:,3]<<24|a[:,:,0]<<16|a[:,:,1]<<8|a[:,:,2];flags=2 if np.any(a[:,:,3]<240) else 0
        blob+=struct.pack('<II',flags,96)+w.astype('<f4').tobytes()+struct.pack('<II',im.width,im.height)+argb.astype('<u4').tobytes()+struct.pack('<I',len(v))+v.astype('<f4').tobytes()+struct.pack('<I',len(idx))+idx.astype('<u2').tobytes()
        if len(idx)%2:blob+=b'\0\0'
    return bytes(blob),g
