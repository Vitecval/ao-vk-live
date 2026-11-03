"""V3 processing of an already prepared owner cache. Never opens an installation.
All original/derived AO artwork produced here remains private, outside the ZIP.
"""
from pathlib import Path
import struct,json,hashlib,math,argparse,shutil,os
import numpy as np
from PIL import Image
LOCAL_ROOT = None

def local_cache_root(path):
    """Restrict reusable conversion functions to the selected installation cache."""
    global LOCAL_ROOT
    LOCAL_ROOT = Path(path).resolve()

def safe(path):
    p=Path(os.path.abspath(path))
    if LOCAL_ROOT is not None:
        if p != LOCAL_ROOT and LOCAL_ROOT not in p.parents:raise ValueError('Converted resources must remain inside the selected AO cache')
    elif p.drive.upper()=='F:' or len(p.drive)!=2 or p.drive[1]!=':':raise ValueError('V3 accepts local non-F: cache paths only')
    # lstat detects junctions without following them onto a protected drive.
    for q in [p,*p.parents]:
        try:info=os.lstat(q)
        except FileNotFoundError:continue
        if getattr(info,'st_file_attributes',0)&0x400:raise ValueError('V3 cache paths must not traverse reparse points')
    return p

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mesh(path):
    path=safe(path);b=path.read_bytes();h=list(struct.unpack_from('<10I',b));at=40;extra=b''
    if h[0] in (0x334d524c,0x344d524c):
        n=struct.unpack_from('<I',b,52)[0];extra=b[40:56+n*4];at=56+n*4
        if h[0]==0x344d524c:
            count=struct.unpack_from('<I',b,at+4)[0];at+=8+16*count
            contacts=struct.unpack_from('<I',b,at)[0];at+=4+4*contacts;extra=b[40:at]
    v=np.frombuffer(b,'<f4',h[1]*(17 if h[6] else 8),at).reshape(h[1],-1).copy();at+=v.nbytes;indices=[]
    for n in h[2:5]:indices.append(np.frombuffer(b,'<u2',n,at).copy());at+=n*2
    palettes=b[at:at+h[6]*h[7]*48];at+=len(palettes)
    maps=[np.frombuffer(b,'<u4',h[5]*h[5],at+j*h[5]*h[5]*4).reshape(h[5],h[5]).copy() for j in range(4)]
    return dict(header=h,extra=extra,vertices=v,indices=indices,palettes=palettes,maps=maps)
def save(path,m):
    with safe(path).open('wb') as f:
        f.write(struct.pack('<10I',*m['header']));f.write(m.get('extra',b''));f.write(m['vertices'].astype('<f4').tobytes())
        for idx in m['indices']:f.write(idx.astype('<u2').tobytes())
        f.write(m.get('palettes',b''))
        for p in m['maps']:f.write(p.astype('<u4').tobytes())
def rgba(p):return np.stack(((p>>16)&255,(p>>8)&255,p&255,(p>>24)&255),-1).astype(np.uint8)
def argb(a):a=np.asarray(a,dtype=np.uint32);return (a[:,:,3]<<24)|(a[:,:,0]<<16)|(a[:,:,1]<<8)|a[:,:,2]
def world(path):
    b=safe(path).read_bytes();assert b[:4]==b'RVKT' and struct.unpack_from('<I',b,4)[0]==2;at=12;parts=[]
    for i in range(struct.unpack_from('<I',b,8)[0]):
        start=at;flags,ref=struct.unpack_from('<II',b,at);at+=8;w=np.frombuffer(b,'<f4',16,at).reshape(4,4).copy();at+=64;tw,th=struct.unpack_from('<II',b,at);at+=8;p=np.frombuffer(b,'<u4',tw*th,at).reshape(th,tw).copy();at+=p.nbytes
        nv=struct.unpack_from('<I',b,at)[0];at+=4;v=np.frombuffer(b,'<f4',nv*8,at).reshape(-1,8).copy();at+=v.nbytes;ni=struct.unpack_from('<I',b,at)[0];at+=4;idx=np.frombuffer(b,'<u2',ni,at).copy();at+=ni*2+(ni%2)*2
        parts.append(dict(number=i,flags=flags,ref=ref,matrix=w,pixels=p,vertices=v,indices=idx,raw=b[start:at]))
    assert at==len(b);return parts
def geometry_hash(v,idx,w):
    p=np.c_[v[:,:3].astype(float),np.ones(len(v))]@w.astype(float);q=np.sign(p[:,:3])*np.floor(abs(p[:,:3])*200+.5);h=14695981039346656037
    for x in list(q.astype(np.int64).ravel())+list(idx):
        for byte in struct.pack('<I',int(x)&0xffffffff):h=((h^byte)*1099511628211)&0xffffffffffffffff
    return h
def basic(v,idx,dim=32,bones=0,frames=0,palettes=b'',maps=None):
    fps=struct.unpack('<I',struct.pack('<f',30. if bones else 0.))[0]
    if maps is None:maps=[np.full((dim,dim),c,dtype=np.uint32) for c in (0xff53595b,0xff8080ff,0xfffac428,0xff000000)]
    return dict(header=[0x324d524c,len(v),len(idx),len(idx),len(idx),dim,bones,frames,fps,0],vertices=np.array(v,dtype='<f4'),indices=[np.array(idx,dtype='<u2')]*3,palettes=palettes,maps=maps)

def variants(source,out,receipts):
    styles=[('resident-jacket',['resident','frontier-coat','navy-security'],[(133,107,76),(57,78,96)]),('traveler-bomber',['traveler','market-merchant','olive-traveler'],[(111,55,51),(81,91,72)]),('technician-workshirt',['technician','maintenance-crew','market-worker'],[(156,105,58),(125,124,109)])];inventory=[]
    for base,names,colours in styles:
        if not any(x['name']==base for x in receipts):continue
        original=mesh(source/(base+'.lrm'));atlas=[p.copy() for p in original['maps']];dim=original['header'][5];side=dim//4;receipt=next(x for x in receipts if x['name']==base)
        clothing={i for i,x in enumerate(receipt['textures']) if x['id'] in {'resident-jacket':{42252,42253,42250},'traveler-bomber':{247894,247940,244580},'technician-workshirt':{9607,9609,244580}}[base]};nexttile=len(receipt['textures']);remaps=[{}]
        for target in colours:
            remap={}
            for tile in sorted(clothing):
                if nexttile>=16:raise ValueError('civilian material atlas full')
                sy,sx=divmod(tile,4);dy,dx=divmod(nexttile,4)
                for j in range(4):atlas[j][dy*side:(dy+1)*side,dx*side:(dx+1)*side]=original['maps'][j][sy*side:(sy+1)*side,sx*side:(sx+1)*side]
                a=rgba(atlas[0][dy*side:(dy+1)*side,dx*side:(dx+1)*side]);rgb=a[:,:,:3].astype(float)/255;lum=rgb@np.array((.2126,.7152,.0722));t=np.array(target)/255;t/=max(t@np.array((.2126,.7152,.0722)),.1)
                a[:,:,:3]=np.clip((.22*rgb+.78*lum[:,:,None]*t)*255,0,255).astype('uint8');atlas[0][dy*side:(dy+1)*side,dx*side:(dx+1)*side]=argb(a);remap[tile]=nexttile;nexttile+=1
            remaps.append(remap)
        for name,remap in zip(names,remaps):
            m={**original,'maps':atlas,'vertices':original['vertices'].copy()};uv=m['vertices'][:,12:14];tiles=np.minimum((uv[:,0]*4).astype(int),3)+4*np.minimum((uv[:,1]*4).astype(int),3)
            for old,new in remap.items():uv[tiles==old]+=np.array((new%4-old%4,new//4-old//4))/4
            save(out/('civilian-'+name+'.lrm'),m);inventory.append(dict(name='civilian-'+name,original=base,original_head=receipt['head'],material_change='clothing only; original skin/head/normal maps retained',triangles=len(m['indices'][0])//3,sha256=sha(out/('civilian-'+name+'.lrm'))))
    return inventory

def machinery(parts,out):
    fan=[p for p in parts if p is not None and np.linalg.norm(p['matrix'][3,:3]-[412.1188659668,43.3,358.0699157715])<.01];rotors=[p for p in fan if len(p['vertices'])==6 and len(p['indices'])==12]
    if len(rotors)!=3:raise ValueError('cached original fan blade parts differ from reviewed geometry')
    # The visible front wheel is separate from its stationary cylindrical shaft/casing. Its axis comes
    # from the original shaft section, not from the wheel's asymmetric (curved/worn) blade bounds.
    outer=[parts[n] for n in (946,947,950)];allparts=rotors+outer;root=rotors[0]['matrix'];pivot=np.array((0.,4.4980078,-.011344,1.));worldpivot=pivot@root;shaft=parts[948]['vertices'][:,:3];outerpivot=np.r_[0.,(shaft[:,1:].min(0)+shaft[:,1:].max(0))/2,1.]
    pivots=[pivot,outerpivot];vertices=[];indices=[];materials={};unique={p['pixels'].tobytes() for p in allparts};grid=2 if len(unique)<=4 else 4;dim=grid*128;maps=[np.full((dim,dim),c,np.uint32) for c in (0xff000000,0xff8080ff,0xfffac428,0xff000000)]
    for i,p in enumerate(allparts):
        bone=0 if i<3 else 1;center=pivots[bone];key=p['pixels'].tobytes()
        if key not in materials:
            tile=len(materials);materials[key]=tile;a=Image.fromarray(rgba(p['pixels'])).resize((118,118),Image.Resampling.LANCZOS);maps[0][tile//grid*128+5:tile//grid*128+123,tile%grid*128+5:tile%grid*128+123]=argb(a)
        tile=materials[key];v=p['vertices'];nv=np.zeros((len(v),17),np.float32);nv[:,:3]=nv[:,3:6]=(v[:,:3]-center[:3])@root[:3,:3];nv[:,6:9]=(v[:,:3]-pivot[:3])@root[:3,:3];nv[:,9:12]=v[:,3:6]@root[:3,:3];nv[:,9:12]/=np.maximum(np.linalg.norm(nv[:,9:12],axis=1,keepdims=True),1e-8);nv[:,12:14]=(np.clip(v[:,6:8],0,1)*118+5+np.array((tile%grid,tile//grid))*128)/dim;nv[:,14:16].view('<i4')[:]=bone;nv[:,16]=1
        indices.extend(p['indices']+len(vertices));vertices.extend(nv)
    poses=[]
    for i in range(120):
        for bone,center in enumerate(pivots):
            t=i*math.tau/120*(2 if bone==0 else 1);c,s=math.cos(t),math.sin(t);orientation=root[:3,:3]/np.linalg.norm(root[0,:3]);rot=orientation.T@np.array([[1,0,0],[0,c,s],[0,-s,c]])@orientation;poses.extend(np.r_[rot.ravel(),(center[:3]-pivot[:3])@root[:3,:3]])
    m=basic(vertices,indices,dim,2,120,np.array(poses,dtype='<f4').tobytes(),maps);save(out/'ao-verified-fan-rotors.lrm',m)
    rules=[dict(vertices=len(p['vertices']),indices=len(p['indices']),origin=p['matrix'][3,:3].tolist(),hash=geometry_hash(p['vertices'],p['indices'],p['matrix'])) for p in allparts]
    cap=parts[926];rim=np.delete(cap['vertices'][:,:3],2,axis=0);rim_world=np.c_[rim,np.ones(len(rim))]@cap['matrix'];outlet=rim_world[:,:3].mean(0);outlet[1]+=.04
    panel=parts[962];face=panel['vertices'][panel['vertices'][:,2]>panel['vertices'][:,2].max()-.02,:3];lens=np.r_[face.mean(0)+[0,0,.012],1.]@root
    return dict(service_lens_heading=math.atan2(root[2,0],root[2,2]),service_lens_offset=(lens[:3]-worldpivot[:3]).tolist(),service_lens_source_part=962,original_mesh=46302,shaft_axis_source_part=948,outer_wheel_pivot=(outerpivot@root)[:3].tolist(),chimney_mesh=21741,chimney_part=926,outlet=outlet.tolist(),outlet_evidence='eight-sided original flue rim surrounding recessed cap center; not a bounding-box guess',original_parts=[p['number'] for p in allparts],pivot=worldpivot[:3].tolist(),rules=rules,source='V2 private original Newland reconstruction; live fingerprint agreement pending')

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-cache',required=True);p.add_argument('--world-cache',required=True);p.add_argument('--output',required=True);a=p.parse_args();source=safe(a.source_cache);worldroot=safe(a.world_cache);out=safe(a.output);out.mkdir(parents=True,exist_ok=True)
    if source==out:raise ValueError('Keep V2 cache intact')
    receipts=json.loads(safe(source/'asset-receipts.json').read_text());inv=variants(source,out,receipts['assets']);parts=world(worldroot/'newland.tree');fan=machinery(parts,out)
    for name in ('minibronto','wingdings','ao-transporter','ao-hoverdrone'):shutil.copy2(safe(source/(name+'.lrm')),safe(out/(name+'.lrm')))
    drone=mesh(out/'ao-hoverdrone.lrm');dim=drone['header'][5];orm=rgba(drone['maps'][2]);orm[:,:,1]=np.minimum(orm[:,:,1],205);orm[:,:,2]=70;orm[0:135,0:300,2]=155;orm[0:135,0:300,1]=172;drone['maps'][2]=argb(orm)
    emissive=rgba(drone['maps'][3]);emissive[210:420,395:402,:3]=(24,123,109);drone['maps'][3]=argb(emissive);save(out/'ao-hoverdrone.lrm',drone)
    # Small physical glass navigation lenses; these add detail to the original craft, not replacement craft shapes.
    for name,colour in [('port',(174,28,18)),('starboard',(20,152,72)),('service',(36,112,160))]:
        v=[];idx=[]
        for i in range(16):
            t=math.tau*i/16;v.append([.055*math.cos(t),.055*math.sin(t),0,0,0,1,.5+.45*math.cos(t),.5+.45*math.sin(t)])
        v.append([0,0,.012,0,0,1,.5,.5])
        for i in range(16):idx.extend((16,i,(i+1)%16))
        m=basic(v,idx,16);m['maps'][0][:]=0xffd0d8da;m['maps'][3][:]=0xff000000|(colour[0]<<16)|(colour[1]<<8)|colour[2];save(out/('lens-'+name+'.lrm'),m)
    for pf in (566,567):
        d=np.load(worldroot/f'world-{pf}.npz');h=d['heights'];(out/f'ground-{pf}.lrg').write_bytes(struct.pack('<4If',0x3152474c,pf,h.shape[1],h.shape[0],float(d['cell']))+h.astype('<f4').tobytes())
    (out/'v3-inventory.json').write_text(json.dumps(dict(source_index=receipts['database_index_sha256'],civilian_variants=inv,machinery=fan,original_heads=3,original_models_private=True,drone_materials='preserved HD albedo/normal; painted metal ORM; emissive inside existing recessed UV slot'),indent=2));(out/'PRIVATE-NOT-FOR-DISTRIBUTION.txt').write_text('AO-derived owner cache. Never include in a distributable archive. V3 does not access F:.\n');print('Prepared nine civilian wardrobes, preserved original creatures/craft, original fan rotors and small navigation lenses from the V2 private cache.')
if __name__=='__main__':main()
