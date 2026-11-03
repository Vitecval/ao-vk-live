"""Read-only placement/terrain survey. Derived geometry stays in private cache."""
import argparse,struct,json,math,importlib.util,hashlib
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from ao_living_assets import Database,Reader,parse_static,transform

def placements(path):
    # Post-18.7 meshes exceed the historical 300000 cutoff. Skip non-mesh records by identity,
    # never by that obsolete numeric range (which hid ICC doors and industrial equipment).
    dbpath=path.parent.parent/'db'
    mesh_ids={i for k,i in Database(dbpath).index if k==1010001} if (dbpath/'ResourceDatabase.idx').exists() else set()
    r=Reader(path.read_bytes());offsets=[];last=0
    while True:
        v=r.u()
        if v<=last:r.offset-=4;break
        if v>len(r.data):raise ValueError('placement offset outside file')
        offsets.append(v);last=v
    out=[]
    def group():
        for _ in range(r.unpack('H')[0]):
            p=r.unpack('3f');flags,identity=r.unpack('II');scale,extra=r.unpack('BB')
            if identity>300000 and identity not in mesh_ids:r.take(6);continue
            overrides=[];remaining=extra
            while remaining:
                mask=r.u();count=mask.bit_count()
                if not count or count>remaining:raise ValueError(f'invalid texture override mask/count at {r.offset-4}: {mask:#x}, remaining {remaining}, record extra {extra}')
                overrides.extend((mask,*r.unpack(f'{count}I')));remaining-=count
            overrides=tuple(overrides)
            base=scale/100+.1;rot=flags>>7
            if flags&1:
                steps,yaw=divmod(rot,630);yaw*=.009973309934139252;base*=steps%211/100+.5
                w=transform((0,0,0),(0,math.sin(yaw/2),0,math.cos(yaw/2)),base);w[1,:3]+=w[2,:3]*(-(steps//211/100-1.25))
            else:
                x,v=(rot%180,rot//180) if rot<0x163f500 else (180,rot-0x163f500)
                pitch=(x-90)*math.tau/360;yaw=(v%360)*math.tau/360;roll=(v//360)*math.tau/360
                qx=(math.sin(pitch/2),0,0,math.cos(pitch/2));qy=(0,math.sin(yaw/2),0,math.cos(yaw/2));qz=(0,0,math.sin(roll/2),math.cos(roll/2))
                w=transform((0,0,0),qz)@transform((0,0,0),qx)@transform((0,0,0),qy);w[:3,:3]*=base
            w[3,:3]=p;out.append(dict(id=identity,position=p,matrix=w,overrides=overrides,flags=flags))
    first_start=r.offset;first_size=r.u();group()
    # Remaining first-block records are non-mesh local objects (lights/markers).
    # Use the declared boundary instead of interpreting their packed bytes as mesh overrides.
    first_end=first_start+4+first_size
    if not r.offset<=first_end<=len(r.data):raise ValueError('first placement block boundary mismatch')
    r.offset=first_end
    for offset in offsets[1:]:
        r.offset=offset;r.take(r.unpack('H')[0]*2)
        for _ in range(5):group()
    return out

def ground(db,pf,source):
    spec=importlib.util.spec_from_file_location('ground',source);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    class Adapter:
        def execute(self,query,args):
            self.data=db.get(1000009,args[0])[12:];return self
        def fetchone(self):return (self.data,)
    return mod.load(Adapter(),pf)

def main():
    p=argparse.ArgumentParser();p.add_argument('--client',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--pf',type=int,default=566);p.add_argument('--ground-source',required=True,type=Path);a=p.parse_args()
    if a.output.resolve().is_relative_to(a.client.resolve()):raise ValueError('output must be outside game installation')
    a.output.mkdir(parents=True,exist_ok=True);db=Database(a.client/'cd_image/data/db');names=db.names();g=ground(db,a.pf,a.ground_source);H=g['heights'];cell=g['cell']
    placed=placements(a.client/f'cd_image/data/statels/{a.pf}.pf');cache={};errors={};triangles=[];records=[]
    for obj in placed:
        ident=obj['id']
        if ident not in cache and ident not in errors:
            try:cache[ident]=parse_static(db.get(1010001,ident))
            except Exception as e:errors[ident]=str(e)
        entry={k:v for k,v in obj.items() if k!='matrix'};entry['name']=names.get((1010001,ident),'');entry['parsed']=ident in cache
        if ident in cache:
            points=[]
            for part in cache[ident]['parts']:
                v=np.c_[part['vertices'][:,:3],np.ones(len(part['vertices']))]@obj['matrix'];points.append(v[:,:3]);triangles.append(v[part['indices'].reshape(-1,3),:3])
            if points:
                allp=np.concatenate(points);entry['bounds']=[allp.min(0).tolist(),allp.max(0).tolist()]
        records.append(entry)
    tr=np.concatenate(triangles) if triangles else np.empty((0,3,3));np.savez_compressed(a.output/f'world-{a.pf}.npz',heights=H,triangles=tr,cell=cell)
    report=dict(playfield=a.pf,dimensions=g['size'],cell=cell,index_sha256=db.index_sha256,placements=len(placed),unique_meshes=len(cache)+len(errors),decoded_meshes=len(cache),triangles=len(tr),errors=errors,objects=records)
    (a.output/f'survey-{a.pf}.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='objects'},indent=2))
    # Survey image: north at top; existing geometry projected over height-shaded ground.
    scale=2;extent=cell*(H.shape[0]-1);shade=np.clip(110+H*.8,0,210).astype('uint8');im=Image.fromarray(shade).convert('RGB').resize((int(extent*scale),)*2);draw=ImageDraw.Draw(im)
    for rec in records:
        if 'bounds' in rec:
            lo,hi=np.array(rec['bounds']);box=[lo[0]*scale,(extent-hi[2])*scale,hi[0]*scale,(extent-lo[2])*scale]
            if max(box)-min(box)<10000:draw.rectangle(box,outline=(170,110,70))
        else:
            x,y,z=rec['position'];draw.ellipse((x*scale-2,(extent-z)*scale-2,x*scale+2,(extent-z)*scale+2),fill='red')
    for x in range(0,int(extent),50):
        draw.line((x*scale,0,x*scale,extent*scale),fill=(65,75,80));draw.line((0,x*scale,extent*scale,x*scale),fill=(65,75,80));draw.text((x*scale+3,3),str(x),fill='white');draw.text((3,(extent-x)*scale-14),str(x),fill='white')
    im.save(a.output/f'survey-{a.pf}.png')
if __name__=='__main__':main()
