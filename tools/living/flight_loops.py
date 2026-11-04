"""Closed flight plans, certified over complete cubic curves and whole craft."""
import math
import numpy as np
from low_flight import swept_box_collision

def controls(points):
    p=np.asarray(points,float);result=[]
    def slope(previous,current,next):
        a=current-previous;b=next-current
        return 0. if a*b<=0 else math.copysign(min(abs((a+b)/2),3*abs(a),3*abs(b)),a)
    for i in range(len(p)):
        a,b,c,d=[p[j%len(p)] for j in (i-1,i,i+1,i+2)]
        first=(c-a)/2;second=(d-b)/2
        first[1]=slope(a[1],b[1],c[1]);second[1]=slope(b[1],c[1],d[1])
        result.append(np.array([b,b+first/3,c-second/3,c]))
    return result

def certify(points,geometry,terrain,horizontal,vertical,extent,geometry_bounds=None):
    curves=controls(points);envelope=np.array([horizontal,vertical,horizontal])
    all_controls=np.concatenate(curves)
    if np.any(all_controls[:,(0,2)].min(0)-horizontal<0) or np.any(all_controls[:,(0,2)].max(0)+horizontal>extent):return False
    bounds_low=all_controls.min(0)-envelope;bounds_high=all_controls.max(0)+envelope
    if geometry_bounds is not None:
        gl,gh=geometry_bounds;keep=np.all(gh>=bounds_low,axis=1)&np.all(gl<=bounds_high,axis=1);geometry=geometry[keep]
    g=np.concatenate((geometry,terrain));lo=g.min(1);hi=g.max(1)
    keep=np.all(hi>=bounds_low,axis=1)&np.all(lo<=bounds_high,axis=1);g,lo,hi=g[keep],lo[keep],hi[keep]
    def clear(c,depth=0):
        ids=np.all(hi>=c.min(0)-envelope,axis=1)&np.all(lo<=c.max(0)+envelope,axis=1)
        if not np.any(ids):return True
        chord=c[3]-c[0];length=np.linalg.norm(chord)
        if length<1e-6:return False
        u=np.clip((c-c[0])@chord/(length*length),0,1)
        deviation=float(np.linalg.norm(c-(c[0]+u[:,None]*chord),axis=1).max())
        if deviation<.12 and np.hypot(chord[0],chord[2])>.01:
            return not swept_box_collision(g[ids],c[0],c[3],horizontal+deviation,vertical+deviation)
        if depth>=14:return False
        a=(c[:-1]+c[1:])/2;b=(a[:-1]+a[1:])/2;m=(b[0]+b[1])/2
        return clear(np.array([c[0],a[0],b[0],m]),depth+1) and clear(np.array([m,b[1],a[2],c[3]]),depth+1)
    return all(clear(c) for c in curves)

def near_surface_loops(geometry,heights,cell,center,graph,horizontal,vertical,existing=(),target=12):
    from city_airspace import terrain_triangles
    from human_routes import ground_height
    extent=np.array([(heights.shape[1]-1)*cell,(heights.shape[0]-1)*cell]);gl=geometry.min(1);gh=geometry.max(1)
    def signature(points):
        p=[tuple(np.round(v[[0,2]],1)) for v in np.asarray(points)]
        return min(tuple(q[i:]+q[:i]) for q in (p,list(reversed(p))) for i in range(len(q)))
    seen={signature(r['points']) for r in existing};centers=[];tiles=set();nodes=graph.get('nodes',[])
    hub=np.array(nodes[0]['point'] if nodes else center,float)
    for node in sorted(nodes,key=lambda n:math.hypot(n['point'][0]-hub[0],n['point'][2]-hub[2])):
        c=np.array(node['point'],float);key=(round(c[0]/12),round(c[2]/12),round(c[1]/3))
        if np.linalg.norm(c[[0,2]]-hub[[0,2]])>160:continue
        if key not in tiles:tiles.add(key);centers.append(c)
        if len(centers)>=80:break
    routes=[]
    for pattern,(span,aspect,rotation) in enumerate(((16,.85,0),(22,.7,math.pi/4),(28,.65,math.pi/2),(36,.6,math.pi/8),(44,.55,-math.pi/4))):
        for c in centers:
            floor=float(c[1]-.008);p=[]
            for i in range(8):
                t=i*math.pi/4;x,z=math.cos(t)*span,math.sin(t)*span*aspect
                p.append(c+np.array([x*math.cos(rotation)-z*math.sin(rotation),0,x*math.sin(rotation)+z*math.cos(rotation)]))
            p=np.array(p);key=signature(p)
            if key in seen:continue
            hull=np.concatenate(controls(p))
            if np.any(hull[:,(0,2)].min(0)-horizontal<0) or np.any(hull[:,(0,2)].max(0)+horizontal>extent):continue
            # Keep the complete low loop above streets/platforms, rather than
            # classifying a high return leg as a low visit from one short dip.
            ids=np.all(gh[:,(0,2)]>=hull[:,(0,2)].min(0),axis=1)&np.all(gl[:,(0,2)]<=hull[:,(0,2)].max(0),axis=1)&(gh[:,1]>=floor-2.5)&(gl[:,1]<=floor+2.5)
            triangles=geometry[ids];a=triangles[:,0];b=triangles[:,1]-a;d=triangles[:,2]-a;den=b[:,0]*d[:,2]-b[:,2]*d[:,0]
            normal=np.cross(b,d);slope=np.abs(normal[:,1])/np.maximum(np.linalg.norm(normal,axis=1),1e-8);good=(np.abs(den)>1e-7)&(slope>.8)
            a,b,d,den=a[good],b[good],d[good],den[good]
            supported=True;floor_samples=[]
            for curve in controls(p):
                for t in (0,.25,.5,.75):
                    q=(1-t)**3*curve[0]+3*(1-t)**2*t*curve[1]+3*(1-t)*t*t*curve[2]+t**3*curve[3];x,z=q[[0,2]]
                    values=[]
                    terrain_y=ground_height(heights,cell,x,z)
                    if abs(terrain_y-floor)<=2.5:values.append(terrain_y)
                    if len(a):
                        u=((x-a[:,0])*d[:,2]-(z-a[:,2])*d[:,0])/den;v=(b[:,0]*(z-a[:,2])-b[:,2]*(x-a[:,0]))/den;y=a[:,1]+u*b[:,1]+v*d[:,1]
                        valid=(u>=-.001)&(v>=-.001)&(u+v<=1.001)&(np.abs(y-floor)<=2.5);values.extend(y[valid].tolist())
                    if not values:supported=False;break
                    floor_samples.append(max(values))
                if not supported:break
            if not supported:continue
            terrain=terrain_triangles(heights,cell,hull.min(0),hull.max(0),horizontal)
            for altitude in (3.5,5,7,9,11):
                p[:,1]=floor+altitude
                if p[0,1]-max(floor_samples)<3:continue
                if certify(p,geometry,terrain,horizontal,vertical,extent,(gl,gh)):
                    seen.add(key);routes.append(dict(points=p.tolist(),low_flight=True,near_surface_loop=True,category='street-level',sampled_minimum_surface_clearance=float(p[0,1]-max(floor_samples)),sampled_maximum_surface_clearance=float(p[0,1]-min(floor_samples)),complete_static_clearance=True,envelope=[horizontal,vertical,horizontal]));break
            if len(routes)>=target:return routes
    # Narrow streets may permit a long low pass but not a complete low oval.
    # Give those passes a fully checked climb/return, anchored to the same
    # real pedestrian surface rather than the terrain underneath a platform.
    corridors=set()
    for c in centers:
        added=0
        for length in (72,52,36,28):
            floor=float(c[1]-.008)
            for angle in (0,math.pi/4,math.pi/2,-math.pi/4):
                if added>=3:break
                corridor=(round(c[0]/12),round(c[2]/12),round(angle/(math.pi/4))%4)
                if corridor in corridors:continue
                direction=np.array([math.cos(angle),0,math.sin(angle)]);side=np.array([-direction[2],0,direction[0]])
                start=c-direction*length/2;end=c+direction*length/2
                lo=np.minimum(start,end)-horizontal;hi=np.maximum(start,end)+horizontal
                if np.any(lo[[0,2]]<0) or np.any(hi[[0,2]]>extent):continue
                ids=np.all(gh[:,(0,2)]>=lo[[0,2]],axis=1)&np.all(gl[:,(0,2)]<=hi[[0,2]],axis=1)&(gh[:,1]>=floor-2.5)&(gl[:,1]<=floor+2.5)
                triangles=geometry[ids];a=triangles[:,0];b=triangles[:,1]-a;d=triangles[:,2]-a;den=b[:,0]*d[:,2]-b[:,2]*d[:,0]
                normal=np.cross(b,d);slope=np.abs(normal[:,1])/np.maximum(np.linalg.norm(normal,axis=1),1e-8);valid=(np.abs(den)>1e-7)&(slope>.8);a,b,d,den=a[valid],b[valid],d[valid],den[valid]
                floors=[]
                for u in np.linspace(0,1,13):
                    q=start*(1-u)+end*u;x,z=q[[0,2]];values=[];terrain_y=ground_height(heights,cell,x,z)
                    if abs(terrain_y-floor)<=2.5:values.append(terrain_y)
                    if len(a):
                        u0=((x-a[:,0])*d[:,2]-(z-a[:,2])*d[:,0])/den;v=(b[:,0]*(z-a[:,2])-b[:,2]*(x-a[:,0]))/den;y=a[:,1]+u0*b[:,1]+v*d[:,1]
                        good=(u0>=-.001)&(v>=-.001)&(u0+v<=1.001)&(np.abs(y-floor)<=2.5);values.extend(y[good].tolist())
                    if not values:break
                    floors.append(max(values))
                if len(floors)!=13:continue
                accepted=False
                for sign in (1,-1):
                    if accepted:break
                    turn=side*80*sign
                    for altitude in (3.5,5,7,9,11):
                        if floor+altitude-max(floors)<3:continue
                        p=np.array([start,c,end,end+direction*8,end+direction*65+turn*.5,c+turn,start-direction*65+turn*.5,start-direction*8]);p[:,1]=floor+altitude
                        hull=np.concatenate(controls(p));ids=np.all(gh[:,(0,2)]>=hull[:,(0,2)].min(0)-horizontal,axis=1)&np.all(gl[:,(0,2)]<=hull[:,(0,2)].max(0)+horizontal,axis=1)
                        sky=max(float(gh[ids,1].max()) if np.any(ids) else floor,floor+40)+vertical+12
                        p[[3,7],1]+=16;p[[4,5,6],1]=sky;key=signature(p)
                        if key in seen:continue
                        hull=np.concatenate(controls(p))
                        if np.any(hull[:,(0,2)].min(0)-horizontal<0) or np.any(hull[:,(0,2)].max(0)+horizontal>extent):continue
                        terrain=terrain_triangles(heights,cell,hull.min(0),hull.max(0),horizontal)
                        if certify(p,geometry,terrain,horizontal,vertical,extent,(gl,gh)):
                            seen.add(key);corridors.add(corridor);added+=1;routes.append(dict(points=p.tolist(),low_flight=True,near_surface_pass=True,category='street-flyby',low_leg_length=length,sampled_minimum_surface_clearance=float(floor+altitude-max(floors)),sampled_maximum_surface_clearance=float(floor+altitude-min(floors)),complete_static_clearance=True,envelope=[horizontal,vertical,horizontal]));accepted=True;break
                if len(routes)>=target:return routes
            if added>=3:break
    return routes

def build_loops(geometry,heights,cell,center,lanes,radius,half_height,large_radius=0,graph=None):
    from city_airspace import terrain_triangles
    extent=np.array([(heights.shape[1]-1)*cell,(heights.shape[0]-1)*cell])
    phase=[];large=[]
    gl=geometry.min(1);gh=geometry.max(1);world_roof=float(gh[:,1].max())
    horizontal=radius+.25+.4;vertical=half_height+radius*math.sin(.025)+.08+.35
    def check(points,h,v):
        p=np.asarray(points,float);lo=p.min(0);hi=p.max(0)
        # Full native cells beneath the complete Bezier control hull.
        c=np.concatenate(controls(p));terrain=terrain_triangles(heights,cell,c.min(0),c.max(0),h)
        return certify(p,geometry,terrain,h,v,extent,(gl,gh))
    low=[l for l in lanes if l.get('low_flight')]
    for lane in low:
        a=np.array(lane['start']);b=np.array(lane['end']);d=b-a;d[1]=0;d/=np.linalg.norm(d);side=np.array([-d[2],0,d[0]]);middle=(a+b)/2
        accepted=False
        for sign in (1,-1):
            if accepted:break
            for lead,rise,width in ((8,16,100),(18,20,140),(32,16,180),(6,28,120)):
                turn=side*width*sign
                footprint=np.array([a-d*(lead+80),b+d*(lead+80),middle+turn])
                ids=np.all(gh[:,(0,2)]>=footprint[:,(0,2)].min(0)-horizontal,axis=1)&np.all(gl[:,(0,2)]<=footprint[:,(0,2)].max(0)+horizontal,axis=1)
                roof=float(gh[ids,1].max()) if np.any(ids) else max(a[1],b[1])
                sky=max(roof+vertical+20,max(a[1],b[1])+55)
                points=[a,middle,b,b+d*lead+np.array([0,rise,0]),b+d*(lead+80)+turn*.5,middle+turn,a-d*(lead+80)+turn*.5,a-d*lead+np.array([0,rise,0])]
                for i in (4,5,6):points[i][1]=sky
                if check(points,horizontal,vertical):
                    phase.append(dict(points=np.asarray(points).tolist(),low_flight=True,complete_static_clearance=True,envelope=[horizontal,vertical,horizontal]));accepted=True;break
    # Longer city flyovers provide varied uninterrupted routes even where a
    # particular near-ground corridor cannot support a complete approach/exit.
    for pattern,offset in enumerate(((0,0),(65,0),(-65,0),(0,65),(0,-65))):
        if len(phase)>=12:break
        c=np.array(center,float);c[[0,2]]+=offset
        for span in (160-pattern*15,110-pattern*5,75):
            guard=span*1.05+horizontal+2;c[[0,2]]=np.clip(c[[0,2]],guard,extent-guard)
            p=np.array([c+np.array([math.cos(i*math.pi/4)*span,0,math.sin(i*math.pi/4)*span*.65]) for i in range(8)])
            ids=np.all(gh[:,(0,2)]>=p[:,(0,2)].min(0)-horizontal,axis=1)&np.all(gl[:,(0,2)]<=p[:,(0,2)].max(0)+horizontal,axis=1)
            roof=float(gh[ids,1].max()) if np.any(ids) else float(heights.max())
            p[:,1]=max(roof+vertical+8,float(center[1])+18)
            if check(p,horizontal,vertical):
                phase.append(dict(points=p.tolist(),low_flight=False,complete_static_clearance=True,envelope=[horizontal,vertical,horizontal]));break
    # Distinct ground tracks, not copies of one oval at different altitudes.
    # Prefer neighbourhood-height flyovers before using roof-clearing fallbacks.
    from human_routes import ground_height
    def signature(points):
        p=[tuple(np.round(v[[0,2]],1)) for v in np.asarray(points)]
        return min(tuple(p[i:]+p[:i]) for i in range(len(p)))
    seen={signature(p['points']) for p in phase}
    for pattern in range(96):
        if len(phase)>=24:break
        angle=pattern*2.399963229728653;offset=0 if pattern<4 else 25+((pattern*17)%100)
        c=np.array(center,float)+np.array([math.cos(angle)*offset,0,math.sin(angle)*offset])
        span=44+(pattern%7)*11;aspect=.45+(pattern%4)*.12;rotation=(pattern%16)*math.pi/8
        p=[]
        for i in range(8):
            t=i*math.pi/4;wobble=1+.14*math.sin(2*t+pattern*.7)
            x,z=math.cos(t)*span*wobble,math.sin(t)*span*aspect*wobble
            p.append(c+np.array([x*math.cos(rotation)-z*math.sin(rotation),0,x*math.sin(rotation)+z*math.cos(rotation)]))
        p=np.array(p);hull=np.concatenate(controls(p));lo=hull[:,(0,2)].min(0)-horizontal-1;hi=hull[:,(0,2)].max(0)+horizontal+1
        p[:,(0,2)]+=np.maximum(-lo,0)+np.minimum(extent-hi,0)
        key=signature(p)
        if key in seen:continue
        ground=np.array([ground_height(heights,cell,v[0],v[2]) for v in p]);altitude=(12,18,26,34)[pattern%4]
        accepted=False
        for lift in (0,8,16,28,48):
            p[:,1]=ground+altitude+lift
            if check(p,horizontal,vertical):accepted=True;break
        if not accepted:
            hull=np.concatenate(controls(p));ids=np.all(gh[:,(0,2)]>=hull[:,(0,2)].min(0)-horizontal,axis=1)&np.all(gl[:,(0,2)]<=hull[:,(0,2)].max(0)+horizontal,axis=1)
            terrain=terrain_triangles(heights,cell,hull.min(0),hull.max(0),horizontal)
            p[:,1]=max(float(gh[ids,1].max()) if np.any(ids) else -1e6,float(terrain[:,:,1].max()))+vertical+8
            accepted=check(p,horizontal,vertical)
        if accepted:
            seen.add(key);phase.append(dict(points=p.tolist(),low_flight=False,category='neighbourhood',complete_static_clearance=True,envelope=[horizontal,vertical,horizontal]))
    if graph:phase.extend(near_surface_loops(geometry,heights,cell,center,graph,horizontal,vertical,phase))
    if large_radius:
        for offset in ((0,0),(40,40)):
            c=np.array(center,float);c[[0,2]]+=offset;span=min(180,float(extent.min())*.24)
            guard=span*1.05+large_radius+4;c[[0,2]]=np.clip(c[[0,2]],guard,extent-guard)
            p=np.array([c+np.array([math.cos(i*math.pi/4)*span,0,math.sin(i*math.pi/4)*span]) for i in range(8)])
            p[:,1]=max(world_roof,float(heights.max()))+large_radius+35
            if check(p,large_radius+3,large_radius+3):large.append(dict(points=p.tolist(),low_flight=False,complete_static_clearance=True,envelope=[large_radius+3]*3))
    return phase,large
