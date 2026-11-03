"""Prepare body-sized pedestrian branches and flat ground rest spots.

All original scene triangles participate in clearance. No original geometry is
written here; the output contains authored waypoints and per-body permissions.
"""
import math
import numpy as np

def ground_height(h,cell,x,z):
    u,v=x/cell,z/cell;ix,iz=int(u),int(v)
    if not 0<=ix<h.shape[1]-1 or not 0<=iz<h.shape[0]-1:raise ValueError('waypoint outside terrain')
    u-=ix;v-=iz;a,b,c,d=map(float,(h[iz,ix],h[iz,ix+1],h[iz+1,ix],h[iz+1,ix+1]))
    if (abs(a-d)>cell or abs(b-c)>cell) and abs(a-d)<abs(b-c)-.01:
        return a+v*(c-a)+u*(d-c) if v>u else a+u*(b-a)+v*(d-b)
    return a+u*(b-a)+v*(c-a) if u+v<1 else d+(1-u)*(c-d)+(1-v)*(b-d)

def build_graph(low,high,heights,cell,lines,people,pf,geometry):
    if not people:return dict(nodes=[],edges=[])
    tiles={};floor_cache={}
    def query(lo,hi):
        indices=[]
        for x in range(int(lo[0]//24),int(hi[0]//24)+1):
            for z in range(int(lo[2]//24),int(hi[2]//24)+1):
                key=(x,z)
                if key not in tiles:tiles[key]=np.flatnonzero((high[:,0]>=x*24)&(low[:,0]<=(x+1)*24)&(high[:,2]>=z*24)&(low[:,2]<=(z+1)*24))
                indices.append(tiles[key])
        return indices[0] if len(indices)==1 else np.unique(np.concatenate(indices))
    def support(x,z,reference):
        key=(round(x,3),round(z,3),round(reference,2))
        if key in floor_cache:return floor_cache[key]
        values=[]
        try:
            y=ground_height(heights,cell,x,z)
            if abs(y-reference)<.55:values.append(y)
        except ValueError:pass
        ids=query([x,reference,z],[x,reference,z]);ids=ids[(low[ids,1]<reference+.55)&(high[ids,1]>reference-.55)&(low[ids,0]<=x)&(high[ids,0]>=x)&(low[ids,2]<=z)&(high[ids,2]>=z)]
        if len(ids):
            t=geometry[ids];a=t[:,0];b=t[:,1]-a;c=t[:,2]-a;den=b[:,0]*c[:,2]-b[:,2]*c[:,0]
            good=np.abs(den)>1e-7;t=t[good];a=a[good];b=b[good];c=c[good];den=den[good]
            u=((x-a[:,0])*c[:,2]-(z-a[:,2])*c[:,0])/den;v=(b[:,0]*(z-a[:,2])-b[:,2]*(x-a[:,0]))/den
            normal=np.cross(b,c);slope=np.abs(normal[:,1])/np.maximum(1e-8,np.linalg.norm(normal,axis=1))
            y=a[:,1]+u*b[:,1]+v*c[:,1];good=(u>=-.001)&(v>=-.001)&(u+v<=1.001)&(slope>.8)&(np.abs(y-reference)<.55)
            values.extend(y[good].tolist())
        answer=max(values)+.008 if values else None;floor_cache[key]=answer;return answer
    mc,ac=map(int,lines[0].split()[4:6]);points=[];scales=[]
    for line in lines[1+mc:1+mc+ac]:
        f=line.split()
        if f[0]!='0' or f[3]!='3':continue
        p=np.array(list(map(float,f[10:10+int(f[9])*3]))).reshape(-1,3)
        scales.append(float(f[4]))
        for q in [p[0],p[(pf+len(scales))%len(p)],p.mean(0)]:
            # Roots and two nearby destinations connect neighboring approved
            # areas, with every new segment checked independently below.
            for dx,dz in ((0,0),(2.4,-1.8)):
                x,z=float(q[0]+dx),float(q[2]+dz)
                y=support(x,z,q[1])
                if y is None:continue
                if not any(math.hypot(x-r[0],z-r[2])<.7 for r in points):points.append([x,y,z])
                if len(points)>=768:break
            if len(points)>=768:break
        if len(points)>=768:break
    if not points:return dict(nodes=[],edges=[])
    p=np.array(points);scale=max(scales,default=1);radius=max(r['horizontal_radius'] for r in people)*scale+.12
    body_height=max(r['animated_height'] for r in people)*scale+.10
    def clearance(a,b):
        lo=np.minimum(a,b);hi=np.maximum(a,b);footlo=lo-radius;foothi=hi+radius
        x0=max(0,int(footlo[0]//cell));x1=min(heights.shape[1],int(foothi[0]//cell)+2)
        z0=max(0,int(footlo[2]//cell));z1=min(heights.shape[0],int(foothi[2]//cell)+2)
        samples=[]
        for u in np.linspace(0,1,max(2,int(math.dist(a,b)/.6)+1)):
            q=np.array(a)*(1-u)+np.array(b)*u;y=support(q[0],q[2],q[1])
            if y is None or abs(y-q[1])>.06:return [],False
            for dx,dz in ((.26,0),(-.26,0),(0,.26),(0,-.26)):
                side=support(q[0]+dx,q[2]+dz,q[1])
                if side is None or abs(side-y)>.10:return [],False
            samples.append(y)
        ids=query(footlo,foothi);l,h=low[ids],high[ids]
        overlap=(h[:,1]>min(samples)+.09)&(l[:,1]<max(samples)+body_height)
        l,h=l[overlap],h[overlap]
        if len(l):
            dx=np.maximum.reduce([l[:,0]-hi[0],lo[0]-h[:,0],np.zeros(len(l))]);dz=np.maximum.reduce([l[:,2]-hi[2],lo[2]-h[:,2],np.zeros(len(l))])
            distance=float(np.hypot(dx,dz).min())
        else:distance=float('inf')
        flat=max(samples)-min(samples)<.06
        if flat:
            for angle in np.linspace(0,math.tau,8,endpoint=False):
                y=support(a[0]+math.cos(angle)*radius,a[2]+math.sin(angle)*radius,a[1])
                if y is None or abs(y-a[1])>.06:flat=False;break
        return [r['name'] for r in people if r['horizontal_radius']*scale+.12<distance],flat
    nodes=[]
    for i,q in enumerate(points):
        allowed,flat=clearance(q,q)
        nodes.append(dict(point=q,rest=allowed if flat and (i*37+pf)%5==0 else []))
    edges=[];seen=set()
    for i,q in enumerate(p):
        distances=np.linalg.norm(p-q,axis=1)
        for j in np.argsort(distances)[1:9]:
            j=int(j)
            if not 1.3<=distances[j]<=16 or (min(i,j),max(i,j)) in seen:continue
            seen.add((min(i,j),max(i,j)));allowed,_=clearance(q,p[j])
            if allowed:edges.append(dict(a=i,b=j,allowed=allowed))
            if len(edges)>=4096:break
        if len(edges)>=4096:break
    return dict(nodes=nodes,edges=edges,body_sweep_includes_activities=True,live_verified=False)
