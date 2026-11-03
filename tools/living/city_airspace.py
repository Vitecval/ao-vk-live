"""Prepare deterministic straight corridors against every placed original mesh.

The renderer follows these same line segments. Geometry is decoded once during
local preparation; unknown placement meshes fail the entire city's airspace.
No per-frame pathfinding, database scanning or game collision calls are needed.
"""
import hashlib
import math
import numpy as np
from ao_living_assets import parse_static
from survey_world import placements
from aerial_clearance import validate


def terrain_triangles(heights,cell,start,end,radius):
    low=np.minimum(start,end)-radius;high=np.maximum(start,end)+radius
    x0=max(0,int(math.floor(low[0]/cell)));x1=min(heights.shape[1]-1,int(math.ceil(high[0]/cell)))
    z0=max(0,int(math.floor(low[2]/cell)));z1=min(heights.shape[0]-1,int(math.ceil(high[2]/cell)))
    if x0>=x1 or z0>=z1:raise ValueError('Aerial corridor outside original terrain')
    z,x=np.mgrid[z0:z1,x0:x1];p=np.stack((x*cell,heights[z,x],z*cell),-1)
    a=p.copy();b=np.stack(((x+1)*cell,heights[z,x+1],z*cell),-1)
    c=np.stack(((x+1)*cell,heights[z+1,x+1],(z+1)*cell),-1)
    d=np.stack((x*cell,heights[z+1,x],(z+1)*cell),-1)
    ad=np.abs(a[:,:,1]-c[:,:,1]);bd=np.abs(b[:,:,1]-d[:,:,1])
    across=((ad>cell)|(bd>cell))&(ad<bd-.01)
    first=np.where(across[:,:,None,None],np.stack((a,b,c),-2),np.stack((a,b,d),-2))
    second=np.where(across[:,:,None,None],np.stack((a,c,d),-2),np.stack((b,c,d),-2))
    return np.concatenate((first.reshape(-1,3,3),second.reshape(-1,3,3)))


def possible_collision(triangles,start,end,envelope):
    low=triangles.min(1)-envelope;high=triangles.max(1)+envelope
    entry=np.zeros(len(triangles));exit=np.ones(len(triangles));step=end-start
    for axis in range(3):
        if abs(step[axis])<1e-12:
            exit=np.where((start[axis]>=low[:,axis])&(start[axis]<=high[:,axis]),exit,-1)
        else:
            a=(low[:,axis]-start[axis])/step[axis];b=(high[:,axis]-start[axis])/step[axis]
            entry=np.maximum(entry,np.minimum(a,b));exit=np.minimum(exit,np.maximum(a,b))
    return bool(np.any(entry<=exit))


def appearance_clearance(low,high,heights,cell,lines,people):
    """Conservative swept body cylinders over the exact Catmull convex hulls.

    A control-point hull contains the entire cubic segment; no sampling gap or
    animation pose can escape the character's recorded horizontal/vertical bound.
    Terrain-following roots use min/max heights across the whole swept footprint.
    """
    mc,ac=map(int,lines[0].split()[4:6]);result={}
    for actor,line in enumerate(lines[1+mc:1+mc+ac]):
        f=line.split()
        if f[0]!='0':continue
        scale=float(f[4]);maximum=max(r['horizontal_radius'] for r in people)*scale+.08
        height=max(r['animated_height'] for r in people)*scale+.08
        n=int(f[9]);p=np.array(list(map(float,f[10:10+n*3]))).reshape(n,3)
        boxes=[]
        for i in range(n if n>1 else 1):
            if n==1:control=p
            else:
                a,b,c,d=[p[j%n] for j in (i-1,i,i+1,i+2)]
                control=np.array([b,b+(c-a)/6,c-(d-b)/6,c])
            lo,hi=control.min(0),control.max(0)
            if f[3]=='3' and f[-1]=='0':
                x0=max(0,int((lo[0]-maximum)//cell));x1=min(heights.shape[1],int((hi[0]+maximum)//cell)+2)
                z0=max(0,int((lo[2]-maximum)//cell));z1=min(heights.shape[0],int((hi[2]+maximum)//cell)+2)
                region=heights[z0:z1,x0:x1]
                if not region.size:lo[1],hi[1]=-100000,100000
                else:lo[1],hi[1]=float(region.min())+.008,float(region.max())+.008
            boxes.append((lo,hi))
        whole_low=np.min([b[0] for b in boxes],axis=0)-maximum
        whole_high=np.max([b[1] for b in boxes],axis=0)+maximum
        candidate=(high[:,0]>=whole_low[0])&(low[:,0]<=whole_high[0])&(high[:,2]>=whole_low[2])&(low[:,2]<=whole_high[2])
        l,h=low[candidate],high[candidate];clearance=float('inf')
        for lo,hi in boxes:
            # The native support surface is allowed under the sole. Walls,
            # roofs, equipment and tree geometry inside the body are not.
            overlap=(h[:,1]>lo[1]+.08)&(l[:,1]<hi[1]+height)
            if not np.any(overlap):continue
            a,b=l[overlap],h[overlap]
            dx=np.maximum.reduce([a[:,0]-hi[0],lo[0]-b[:,0],np.zeros(len(a))])
            dz=np.maximum.reduce([a[:,2]-hi[2],lo[2]-b[:,2],np.zeros(len(a))])
            clearance=min(clearance,float(np.hypot(dx,dz).min()))
        result[str(actor)]=[r['name'] for r in people if r['horizontal_radius']*scale+.08<clearance]
    return result


def corridors(db,client,pf,heights,cell,center,phase_radius,large_radius,lines=None,people=()):
    objects=placements(client/f'cd_image/data/statels/{pf}.pf');decoded={};triangles=[];hashes={}
    for obj in objects:
        i=obj['id']
        if i==0:continue # null visual identity: no mesh exists or can be drawn
        if i not in decoded:
            blob=db.get(1010001,i);decoded[i]=parse_static(blob)
            hashes[str(i)]=hashlib.sha256(blob).hexdigest()
        for part in decoded[i]['parts']:
            v=np.c_[part['vertices'][:,:3],np.ones(len(part['vertices']))]@obj['matrix']
            triangles.append(v[part['indices'].reshape(-1,3),:3])
    if not triangles:raise ValueError('No original placement geometry decoded')
    geometry=np.concatenate(triangles).astype(np.float32)
    if len(geometry)>8000000:raise ValueError('City geometry exceeds validation budget')
    geometry_low=geometry.min(1);geometry_high=geometry.max(1)
    extent=np.array([(heights.shape[1]-1)*cell,(heights.shape[0]-1)*cell])
    if np.any(extent<100):raise ValueError('City terrain too small for aerial corridors')
    roof=float(geometry[:,:,1].max());bound_top=max(float(heights.max())+600,roof+200)
    bounds=((0,float(heights.min())-100,0),(extent[0],bound_top,extent[1]))
    result={};rejected={}
    for label,radius,phase in (('phasefront',phase_radius,True),('large',large_radius,False)):
        if not radius:continue
        failures=[];margin=3. if phase else 10.
        for axis in (0,2):
            if label in result:break
            for offset,half_length in [(offset,length) for length in (160,110,80,55) for offset in (0,35,-35,70,-70,110,-110)]:
                c=np.array(center,dtype=float);c[2 if axis==0 else 0]+=offset
                half=min(half_length if phase else 220,float(extent[0 if axis==0 else 1])*.30)
                start=c.copy();end=c.copy();start[axis]-=half;end[axis]+=half
                guard=radius+margin+1
                for point in (start,end):
                    point[0]=np.clip(point[0],guard,extent[0]-guard)
                    point[2]=np.clip(point[2],guard,extent[1]-guard)
                if np.linalg.norm(end-start)<100:continue
                terrain=terrain_triangles(heights,cell,start,end,radius+margin)
                if phase:
                    # Ground variation plus the 20–50 m centerline constraint is
                    # certified over all intersected terrain triangles below.
                    top=float(terrain[:,:,1].min())+49
                    bottom=float(terrain[:,:,1].max())+21
                    if bottom>top:continue
                    y=top
                else:y=float(terrain[:,:,1].max())+radius+35
                lo=np.minimum(start,end)-(radius+margin);hi=np.maximum(start,end)+(radius+margin)
                nearby=geometry[np.all(geometry_high[:,(0,2)]>=lo[[0,2]],axis=1)&np.all(geometry_low[:,(0,2)]<=hi[[0,2]],axis=1)]
                if not phase and len(nearby):y=max(y,float(nearby[:,:,1].max())+radius+35)
                start[1]=end[1]=y
                if possible_collision(nearby,start,end,radius+margin) or possible_collision(terrain,start,end,radius+margin):
                    failures.append('swept envelope intersects original geometry');continue
                # Exact terrain coverage and altitude checks use the preserved
                # continuous validator. Full geometry was checked vectorially.
                ok,reason=validate(start.tolist(),end.tolist(),radius,margin,bounds,[],terrain.tolist(),
                                   complete=True,outdoor=True,identity_matches=True,phasefront=phase)
                if ok:
                    result[label]=dict(start=start.tolist(),end=end.tolist(),radius=radius,margin=margin,
                                       speed=11. if phase else 8.,complete_static_clearance=True,live_verified=False)
                    break
                failures.append(reason)
        if label not in result:rejected[label]=sorted(set(failures)) or ['No corridor meets terrain coverage and altitude requirements']
    allowed=appearance_clearance(geometry_low,geometry_high,heights,cell,lines,people) if lines and people else {}
    lanes=[]
    if phase_radius:
        radius=phase_radius;margin=3.
        for axis in (0,2):
            for offset,half in [(o,n) for n in (140,110,80,55) for o in (0,35,-35,70,-70,110,-110)]:
                c=np.array(center,dtype=float);c[2 if axis==0 else 0]+=offset
                start,end=c.copy(),c.copy();start[axis]-=half;end[axis]+=half
                guard=radius+margin+1
                for point in (start,end):
                    point[0]=np.clip(point[0],guard,extent[0]-guard);point[2]=np.clip(point[2],guard,extent[1]-guard)
                if np.linalg.norm(end-start)<100:continue
                terrain=terrain_triangles(heights,cell,start,end,radius+margin)
                lo=np.minimum(start,end)-(radius+margin);hi=np.maximum(start,end)+(radius+margin)
                nearby=geometry[np.all(geometry_high[:,(0,2)]>=lo[[0,2]],axis=1)&np.all(geometry_low[:,(0,2)]<=hi[[0,2]],axis=1)]
                for altitude in (14,23,33,43,48):
                    y=float(terrain[:,:,1].min())+altitude
                    if y-float(terrain[:,:,1].max())<11:continue
                    start[1]=end[1]=y
                    if possible_collision(nearby,start,end,radius+margin) or possible_collision(terrain,start,end,radius+margin):continue
                    ok,reason=validate(start.tolist(),end.tolist(),radius,margin,bounds,[],terrain.tolist(),complete=True,outdoor=True,identity_matches=True,phasefront=False)
                    if ok:lanes.append(dict(start=start.tolist(),end=end.tolist(),radius=radius,margin=margin,minimum_agl=y-float(terrain[:,:,1].max())-.55,maximum_agl=altitude+.55,complete_static_clearance=True))
                if len(lanes)>=(8 if axis==0 else 16):break
            if len(lanes)>=16:break
        lanes=lanes[:16]
        if not lanes and 'phasefront' in result:
            # A city may only have the previously certified broad edge corridor.
            # It still receives variable scheduling, speed and both directions.
            lanes=[dict(result['phasefront'],minimum_agl=20.45,maximum_agl=49.55)]
    from human_routes import build_graph
    graph=build_graph(geometry_low,geometry_high,heights,cell,lines,people,pf,geometry) if lines else dict(nodes=[],edges=[])
    return dict(playfield=pf,placements=len(objects),decoded_models=len(decoded),triangles=len(geometry),appearance_clearance=allowed,
                original_mesh_hashes=hashes,routes=result,blockers=rejected,flight_lanes=lanes,walk_graph=graph,live_verified=False)
