"""Occasional ~10-foot passes using a full swept, bank-expanded vehicle box."""
import math
import numpy as np

def swept_box_collision(triangles,start,end,horizontal,vertical):
    """Triangle SAT against a conservative box sheared along the flight slope.

    Unlike triangle AABBs, this does not turn a large sloping ground triangle
    into an imaginary solid wall. The entire continuous swept craft fits inside.
    """
    delta=end-start;length=float(np.hypot(delta[0],delta[2]));ux,uz=delta[0]/length,delta[2]/length
    slope=delta[1]/length;middle=(start+end)/2;t=triangles.astype(float)-middle
    along=t[:,:,0]*ux+t[:,:,2]*uz;side=-t[:,:,0]*uz+t[:,:,2]*ux
    t=np.stack((along,t[:,:,1]-along*slope,side),axis=-1)
    half=np.array([length/2+horizontal,vertical+horizontal*abs(slope),horizontal])
    keep=np.all(t.min(1)<=half,axis=1)&np.all(t.max(1)>=-half,axis=1);t=t[keep]
    if not len(t):return False
    edges=np.roll(t,-1,axis=1)-t
    axes=[np.cross(edges[:,0],edges[:,1])]
    for i in range(3):
        for basis in np.eye(3):axes.append(np.cross(edges[:,i],basis))
    separated=np.zeros(len(t),dtype=bool)
    for axis in axes:
        projected=np.einsum('nki,ni->nk',t,axis);radius=np.abs(axis)@half
        separate=(projected.min(1)>radius+1e-6)|(projected.max(1)<-radius-1e-6)
        separated|=separate
        if separated.all():return False
    return bool(np.any(~separated))

def low_corridors(geometry,heights,cell,center,graph,radius,half_height,diagnostics=None):
    from city_airspace import possible_collision,terrain_triangles
    from human_routes import ground_height
    if radius<=0 or half_height<=0:return []
    stats=diagnostics if diagnostics is not None else {}
    def reject(reason):stats[reason]=stats.get(reason,0)+1
    # All variants, rider/head and navigation lenses fit this yaw-independent
    # horizontal bound. Include the exact runtime steering, bank, bob and margin.
    horizontal=radius+.25+.4
    vertical=half_height+radius*math.sin(.025)+.08+.35
    envelope=np.array([horizontal,vertical,horizontal])
    low=geometry.min(1);high=geometry.max(1)
    extent=np.array([(heights.shape[1]-1)*cell,(heights.shape[0]-1)*cell])
    candidates=[np.array(center,dtype=float)]
    nodes=graph.get('nodes',[])
    for n in sorted(nodes,key=lambda n:math.dist(n['point'],center))[::max(1,len(nodes)//80)]:candidates.append(np.array(n['point'],dtype=float))
    for dx,dz in ((35,0),(-35,0),(0,35),(0,-35),(70,0),(-70,0),(0,70),(0,-70),(110,0),(-110,0),(0,110),(0,-110)):
        p=np.array(center,dtype=float);p[0]+=dx;p[2]+=dz;candidates.append(p)
    for dx in (-160,-110,-70,0,70,110,160):
        for dz in (-160,-110,-70,0,70,110,160):
            p=np.array(center,dtype=float);p[0]+=dx;p[2]+=dz;candidates.append(p)
    lanes=[];seen=set()
    for c in candidates:
        key=(round(c[0]/8),round(c[2]/8))
        if key in seen:continue
        seen.add(key)
        # A regional broad phase retains every triangle potentially touched by
        # any of this center's short passes, avoiding repeated full-city scans.
        reach=42+horizontal
        ids=(high[:,0]>=c[0]-reach)&(low[:,0]<=c[0]+reach)&(high[:,2]>=c[2]-reach)&(low[:,2]<=c[2]+reach)
        nearby=geometry[ids]
        for angle in (0,math.pi/2,math.pi/4,-math.pi/4):
            direction=np.array([math.cos(angle),0,math.sin(angle)])
            for length in (80,60,44,28):
                start=c-direction*length/2;end=c+direction*length/2
                if any(np.any(p[[0,2]]<horizontal+1) or np.any(p[[0,2]]>extent-horizontal-1) for p in (start,end)):reject('bounds');continue
                terrain=terrain_triangles(heights,cell,start,end,horizontal)
                # Complete native grid cells cover the whole swept footprint,
                # including both triangles in each cell; there is no sample gap.
                a=ground_height(heights,cell,start[0],start[2]);b=ground_height(heights,cell,end[0],end[2])
                if abs(b-a)/length>.12:reject('slope');continue
                along=(terrain[:,:,0]-start[0])*direction[0]+(terrain[:,:,2]-start[2])*direction[2]
                residual=terrain[:,:,1]-(a+along*(b-a)/length)
                bottom=float(residual.min());top=float(residual.max())
                if top-bottom>1.2:reject('ground_variation');continue
                offset=(bottom+top)/2+3.048;start[1]=a+offset;end[1]=b+offset
                if possible_collision(nearby,start,end,envelope) and swept_box_collision(nearby,start,end,horizontal,vertical):reject('scenery');continue
                if swept_box_collision(terrain,start,end,horizontal,vertical):reject('terrain');continue
                stats['accepted']=stats.get('accepted',0)+1
                lanes.append(dict(start=start.tolist(),end=end.tolist(),low_flight=True,minimum_agl=offset-top-.08,maximum_agl=offset-bottom+.08,
                                  complete_static_clearance=True,envelope=envelope.tolist(),steering_limit=.25,bob_limit=.08,bank_limit=.025))
                break
            if len(lanes)>=4:return lanes
    return lanes
