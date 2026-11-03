"""Read-only AO visual resources, independent of game DLLs and gameplay objects.

Decoded game content is for the owner's local cache only, never distributable art.
Layouts are implemented from this repository's native CAT/stream source audit.
"""
from pathlib import Path
import struct, zlib, math, bisect, hashlib
import numpy as np

class Reader:
    def __init__(self,data): self.data=data;self.offset=0
    def take(self,n):
        if n<0 or self.offset+n>len(self.data):raise ValueError('truncated visual resource')
        out=self.data[self.offset:self.offset+n];self.offset+=n;return out
    def unpack(self,fmt):return struct.unpack('<'+fmt,self.take(struct.calcsize('<'+fmt)))
    def i(self):return self.unpack('i')[0]
    def u(self):return self.unpack('I')[0]
    def f(self):
        x=self.unpack('f')[0]
        if not math.isfinite(x):raise ValueError('non-finite resource value')
        return x
    def count(self,cap):
        n=self.i()
        if not 0<=n<=cap:raise ValueError(f'resource count {n} exceeds {cap}')
        return n
    def string(self):return self.take(self.count(4096)).decode('cp1252').rstrip('\0')

class Database:
    def __init__(self,path):
        self.path=Path(path)
        if (self.path/'ResourceDatabase.idx').stat().st_size>64*1024*1024:raise ValueError('resource index exceeds compatibility budget')
        raw=(self.path/'ResourceDatabase.idx').read_bytes()
        self.block=struct.unpack_from('<I',raw,12)[0];self.part=struct.unpack_from('<I',raw,184)[0]
        if not 0<self.block<self.part<=2**32:raise ValueError('invalid database geometry')
        at=struct.unpack_from('<I',raw,72)[0];seen=set();self.index={}
        while at:
            if at in seen or at+28>len(raw):raise ValueError('invalid or cyclic index')
            seen.add(at);n=struct.unpack_from('<H',raw,at+8)[0]
            if n>4096 or at+28+n*16>len(raw):raise ValueError('invalid index block length')
            for i in range(n):
                o=at+28+i*16;hi,lo=struct.unpack_from('<II',raw,o);kind,identity=struct.unpack_from('>II',raw,o+8)
                key=(kind,identity)
                if key in self.index:raise ValueError('duplicate resource identity')
                self.index[key]=(hi<<32)|lo
            at=struct.unpack_from('<I',raw,at)[0]
        self.index_sha256=hashlib.sha256(raw).hexdigest()
    def get(self,kind,identity):
        offset=self.index[kind,identity]
        if offset<self.part:part,pos=0,offset
        else:part,rem=divmod(offset-self.part,self.part-self.block);part+=1;pos=rem+self.block
        def take(n):
            nonlocal part,pos
            chunks=[]
            while n:
                count=min(n,self.part-pos);name='ResourceDatabase.dat'+(f'.{part:03}' if part else '')
                with (self.path/name).open('rb') as f:f.seek(pos);b=f.read(count)
                if len(b)!=count:raise ValueError('truncated database part')
                chunks.append(b);n-=count;pos+=count
                if pos==self.part:part+=1;pos=self.block
            return b''.join(chunks)
        header=take(22);actual_kind,actual_id,n=struct.unpack_from('<III',header,10)
        if (actual_kind,actual_id)!=(kind,identity) or not 0<n<32*1024*1024:raise ValueError('invalid visual record identity/size')
        return take(n)
    def names(self):
        data=self.get(1000010,1);r=Reader(data);r.take(12);out={}
        for _ in range(r.count(1000)):
            kind=r.u()
            for _ in range(r.count(1000000)):
                identity=r.u();out[kind,identity]=r.string()
        return out

def parse_mesh(blob):
    if struct.unpack_from('<I',blob)[0]==1010002:blob=blob[12:]
    r=Reader(blob);name=r.take(32).split(b'\0')[0].decode('cp1252');n=max(r.u()//1009-1,0)
    if n>128:raise ValueError('too many texture bindings')
    slots=[]
    for _ in range(n):slots.append((r.take(32).split(b'\0')[0].decode('cp1252'),*r.unpack('3I')))
    colour=r.u()
    if r.i()!=4:raise ValueError('not a CAT mesh')
    version=r.i()
    if version not in (0x67,0x104):raise ValueError('unsupported CAT mesh version')
    skeleton=r.i();anim_radius=r.f();extra=r.f();materials=[]
    for _ in range(r.count(128)):
        mat={'name':r.string(),'flags':r.u(),'texture':r.string()}
        if mat['flags']&2:mat['env']=r.string()
        mat['values']=[r.f() for _ in range(15)];materials.append(mat)
    spheres=r.count(2048);r.take(spheres*16);bones=[]
    for _ in range(r.count(256)):
        b={'name':r.string(),'scale':r.f()};b['children']=[r.i() for _ in range(r.count(256))];bones.append(b)
    groups=[];attractors=[]
    for _ in range(r.count(256)):
        g={'name':r.string(),'pieces':[]}
        for _ in range(r.count(128)):
            material=r.i();nv=r.count(65535)
            if not 0<=material<len(materials):raise ValueError('material out of range')
            if version==0x104:
                data=r.take(nv*68);v=np.frombuffer(data,dtype='<f4').reshape(nv,17).copy();bone_ids=np.frombuffer(data,dtype='<i4').reshape(nv,17)[:,14:16].copy()
            else:
                v=np.zeros((nv,17),dtype=np.float32);bone_ids=np.empty((nv,2),np.int32)
                for i in range(nv):
                    v[i,:6]=r.unpack('6f');v[i,9:14]=r.unpack('5f');bone_ids[i]=r.unpack('2i');v[i,16]=r.f()
            if not np.isfinite(v[:,:14]).all() or not np.isfinite(v[:,16]).all():raise ValueError('invalid mesh floats')
            ni=r.count(300000);indices=np.frombuffer(r.take(ni*2),dtype='<u2').copy()
            if ni%3 or (ni and int(indices.max())>=nv):raise ValueError('invalid mesh triangles')
            g['pieces'].append({'material':material,'vertices':v,'bone_ids':bone_ids,'indices':indices})
        r.take(r.count(2048)*20)
        for _ in range(r.count(2048)):attractors.append({'name':r.string(),'transform':r.unpack('8f'),'bone':r.i()})
        groups.append(g)
    if r.offset!=len(blob):raise ValueError(f'CAT mesh trailing bytes {len(blob)-r.offset}')
    return {'name':name,'slots':slots,'materials':materials,'bones':bones,'groups':groups,'skeleton':skeleton,'radius':anim_radius,'colour':colour,'attractors':attractors}

class Channels:
    def __init__(self,r,bits,n):
        if not 1<=bits<=32:raise ValueError('invalid compression bit width')
        self.width=(bits+7)//8;self.mask=(1<<bits)-1;self.channels=[];self.sums=[0]*n;self.at=0;self.offset=0
        for _ in range(n):
            packed=r.count(16*1024*1024);size=r.count(16*1024*1024);data=zlib.decompress(r.take(packed))
            if len(data)!=size:raise ValueError('animation decompression size mismatch')
            self.channels.append(data)
    def next(self):
        i=self.at;data=self.channels[i]
        if self.offset+self.width>len(data):raise ValueError('animation channel underflow')
        d=int.from_bytes(data[self.offset:self.offset+self.width],'big');self.sums[i]=(self.sums[i]+d)&self.mask
        v=self.sums[i];self.at+=1
        if self.at==len(self.channels):self.at=0;self.offset+=self.width
        return v

def parse_animation(blob):
    if struct.unpack_from('<I',blob)[0]==1010003:blob=blob[12:]
    r=Reader(blob);name=r.take(32).split(b'\0')[0].decode('cp1252');events=[]
    for _ in range(r.count(4096)):events.append((r.i(),r.take(32).split(b'\0')[0].decode('cp1252')))
    if r.i()!=3:raise ValueError('not a CAT animation')
    version=r.u();compressed=bool(version&0x1000000);version&=~0x1000000
    if not 0x105<=version<=0x107:raise ValueError('unsupported CAT animation version')
    old=version<0x106;length=float(r.i()) if old else r.f();skeleton=r.i();radius=r.f();n=r.count(256);tracks=[None]*n
    if compressed:
        rot_bits,pos_bits=r.unpack('bb');ints=Channels(r,32,1);times=Channels(r,20 if old else 32,1);rots=Channels(r,rot_bits,4);ranges=Channels(r,32,1);positions=Channels(r,pos_bits,3)
        asfloat=lambda v:struct.unpack('<f',struct.pack('<I',v))[0]
        timestamp=lambda:float(times.next()) if old else asfloat(times.next())
        for _ in range(n):
            index=ints.next();unknown=ints.next();mask=ints.next();nr=ints.next()
            if index>=n or nr>8192:raise ValueError('invalid animation track')
            rk=[]
            for _ in range(nr):
                t=timestamp();q=np.array([rots.next()-float(1<<(rot_bits-1)) for _ in range(4)]);l=np.linalg.norm(q)
                if l<1e-8:raise ValueError('zero quaternion')
                rk.append((t,q/l))
            bounds=[asfloat(ranges.next()) for _ in range(6)];npk=ints.next()
            if npk>8192:raise ValueError('too many translation keys')
            pk=[]
            for _ in range(npk):
                t=timestamp();p=[bounds[j*2]+(bounds[j*2+1]-bounds[j*2])*positions.next()/((1<<pos_bits)-1) for j in range(3)];pk.append((t,np.array(p)))
            tracks[index]={'rotations':rk,'positions':pk,'mask':mask}
    else:
        for _ in range(n):
            index=r.i();r.i();mask=r.u();nk=r.count(8192);rk=[];pk=[]
            for _ in range(nk):
                t=float(r.i()) if old else r.f();rk.append((t,np.array(r.unpack('4f'))));pk.append((t,np.array(r.unpack('3f'))))
            tracks[index]={'rotations':rk,'positions':pk,'mask':mask}
    if r.offset!=len(blob):raise ValueError('animation trailing bytes')
    if old:
        # Native CAT data repairs repeated quantized timestamps before playing.
        for tr in tracks:
            for channel in ('rotations','positions'):
                prev=-1
                for i,(t,v) in enumerate(tr[channel]):
                    t=max(t,prev+1);tr[channel][i]=(t,v);prev=t
    return {'name':name,'length':length,'skeleton':skeleton,'radius':radius,'tracks':tracks,'events':events,'old_times':old}

def sample(keys,t,quaternion=False):
    if not keys:return np.array([0,0,0,1] if quaternion else [0,0,0],dtype=float)
    if len(keys)==1 or t>keys[-1][0]:return keys[0][1].copy()
    i=max(0,min(len(keys)-2,bisect.bisect_right([x[0] for x in keys],t)-1));ta,a=keys[i];tb,b=keys[i+1];u=max(0,min(1,(t-ta)/max(1e-9,tb-ta)))
    if quaternion:
        dot=float(np.dot(a,b))
        if dot<0:dot=-dot;b=-b
        if dot<.9995:
            theta=math.acos(max(-1,min(1,dot)));return (math.sin((1-u)*theta)*a+math.sin(u*theta)*b)/math.sin(theta)
        q=(1-u)*a+u*b;return q/max(np.linalg.norm(q),1e-12)
    return (1-u)*a+u*b

def palettes(mesh,anim,t):
    n=len(mesh['bones'])
    if len(anim['tracks'])<n:raise ValueError('animation has too few skeleton tracks')
    matrices=np.tile(np.eye(4),(n,1,1));seen=set()
    def visit(i,parent,scale):
        if i in seen or not 0<=i<n:raise ValueError('invalid bone hierarchy')
        seen.add(i);tr=anim['tracks'][i];q=sample(tr['rotations'],t,True);p=sample(tr['positions'],t)*scale
        x,y,z,w=q;m=np.eye(4)
        m[:3,:3]=[[1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w)],[2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w)],[2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)]]
        m[3,:3]=p;matrices[i]=m@parent
        for child in mesh['bones'][i]['children']:visit(child,matrices[i],scale*mesh['bones'][i]['scale'])
    visit(0,np.eye(4),1.)
    if len(seen)!=n:raise ValueError('disconnected skeleton')
    return matrices

def summary(mesh):
    return {k:mesh[k] for k in ('name','skeleton','slots','attractors')}|{'bones':[(b['name'],b['scale'],b['children']) for b in mesh['bones']],'materials':mesh['materials'],'groups':[{'name':g['name'],'pieces':[{'material':p['material'],'vertices':len(p['vertices']),'triangles':len(p['indices'])//3} for p in g['pieces']]} for g in mesh['groups']]}

def object_archive(blob):
    """Decode self-describing fields without constructing any native resource."""
    r=Reader(blob)
    if r.u()!=1:raise ValueError('unsupported ObjectArchive stream version')
    def nul():
        end=r.data.find(b'\0',r.offset,r.offset+4097)
        if end<0:raise ValueError('unterminated archive name')
        return r.take(end-r.offset+1)[:-1].decode('cp1252')
    names=[(nul(),nul()) for _ in range(r.count(255))];objects=[];root={}
    for instance in range(r.count(16384)+1):
        size=r.count(32*1024*1024);unknown=r.u();fields={};typename=''
        for _ in range(r.count(255)):
            key=r.unpack('B')[0];typ,element,n=r.unpack('III')
            if key>=len(names) or n>32*1024*1024:raise ValueError('invalid archive field')
            name=names[key][1];value=r.take(n)
            if name=='__class_id__':
                ci=struct.unpack('<I',value)[0]
                if ci>=len(names):raise ValueError('invalid archive class id')
                typename=names[ci][1]
            else:fields[name]=(typ,value)
        if instance:objects.append({'class':typename,'fields':fields})
        else:root=fields
    if len(blob)-r.offset>16:raise ValueError(f'archive has {len(blob)-r.offset} trailing bytes')
    return objects,root

def field(obj,name,fmt,default=None):
    v=obj['fields'].get(name)
    return struct.unpack('<'+fmt,v[1]) if v else default
def scalar(obj,name,default=0):return field(obj,name,'i',(default,))[0]
def raw(obj,name,default=b''):return obj['fields'].get(name,(0,default))[1]
def array(obj,name,fmt='i'):
    d=raw(obj,name);n=struct.calcsize('<'+fmt)
    if len(d)%n:raise ValueError('invalid archive array stride')
    return list(struct.iter_unpack('<'+fmt,d))
def transform(position=(0,0,0),rotation=(0,0,0,1),scale=1):
    x,y,z,w=rotation;m=np.eye(4)
    m[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w)],[2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w)],[2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y)]])*scale
    m[3,:3]=position;return m

def parse_static(blob):
    if struct.unpack_from('<I',blob)[0]==1010001:blob=blob[12:]
    r=Reader(blob);version=r.u()
    if version==1:r.take(max(r.u()//1009-1,0)*44)
    elif version==3:r.take(r.count(128)*36)
    else:raise ValueError('unsupported ABIFF wrapper')
    r.u();objects,root=object_archive(blob[r.offset:]);worlds={};parts=[]
    node_classes=('RRefFrame_t','RTriMesh_t','RLight_t','RMeshNode','FAFRefFrame_t','FAFCollisionSphere_c','FAFCollisionBox_c')
    children=set()
    for o in objects:
        if o['class'] in node_classes:children.update(x[0] for x in array(o,'chld'))
    def walk(i,parent,seen):
        if i<0 or i>=len(objects) or i in seen:raise ValueError('cyclic static visual graph')
        o=objects[i];w=transform(field(o,'local_pos','3f',(0,0,0)),field(o,'local_rot','4f',(0,0,0,1)),field(o,'scale','f',(1,))[0])@parent
        worlds[i]=w
        for child, in array(o,'chld'):walk(child,w,seen|{i})
    for i,o in enumerate(objects):
        if o['class'] in node_classes and i not in children:walk(i,np.eye(4),set())
    for i,o in enumerate(objects):
        if o['class']!='RTriMesh_t':continue
        di=scalar(o,'data',-1)
        if di<0:continue
        data=objects[di];dw=transform(field(data,'anim_pos','3f',(0,0,0)),field(data,'anim_rot','4f',(0,0,0,1)))@worlds.get(i,np.eye(4))
        for si, in array(data,'mesh'):
            sm=objects[si];desc=raw(sm,'vb_desc');fvf=struct.unpack_from('<I',desc,8)[0];texcount=(fvf>>8)&15;normal=bool(fvf&16);uv_offset=12+(12 if normal else 0)+(4 if fvf&64 else 0)+(4 if fvf&128 else 0);stride=uv_offset+texcount*8 if (fvf&14)==2 and 1<=texcount<=2 else 0
            if not stride:raise ValueError(f'unsupported static FVF {fvf:x}')
            vb=raw(sm,'vertices')
            if len(vb)<4 or struct.unpack_from('<I',vb)[0]!=len(vb)-4:raise ValueError('invalid static vertex payload length')
            v=np.frombuffer(vb[4:],dtype=np.uint8).reshape(-1,stride);out=np.empty((len(v),8),np.float32)
            for j in range(len(v)):
                p=np.array(struct.unpack_from('<3f',v[j],0)+(1,));n=np.array(struct.unpack_from('<3f',v[j],12)) if normal else np.array((0,1,0));out[j,:3]=(p@dw)[:3];out[j,3:6]=n@dw[:3,:3];out[j,6:8]=struct.unpack_from('<2f',v[j],uv_offset)
            out[:,3:6]/=np.maximum(1e-12,np.linalg.norm(out[:,3:6],axis=1,keepdims=True))
            tr=raw(objects[scalar(sm,'trilist')],'triangles');size=struct.unpack_from('<I',tr)[0]
            indices=np.frombuffer(tr[4:4+size],'<u2').copy()
            material=objects[scalar(sm,'material')];ds=scalar(material,'delta_state',-1);texture=None
            if ds>=0:
                bindings=array(objects[ds],'tch_text')
                if bindings:
                    creator=objects[scalar(objects[bindings[0][0]],'creator')]
                    if creator['class']=='AnarchyTexCreator_t':texture=(scalar(creator,'type'),scalar(creator,'inst'))
            parts.append({'vertices':out,'indices':indices,'texture':texture,'node':i,'material':material})
    return {'parts':parts,'objects':objects}
