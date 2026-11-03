"""Extend preserved V5 placements without changing their routes or lifetimes."""
import math
import numpy as np


def city_faction(config,pf):
    theme=config['cities'].get(str(pf),{}).get('theme','neutral')
    if theme=='entertainment':return 'omni' # Omni-1 Entertainment keeps its own palette theme.
    return theme if theme in ('omni','clan') else 'neutral'


def center(lines):
    h=lines[0].split();mc,ac=map(int,h[4:6]);points=[]
    for line in lines[1+mc:1+mc+ac]:
        fields=line.split()
        if fields[0]=='0':points.append(list(map(float,fields[10:13])))
    return np.mean(points,axis=0).tolist() if points else None


def extend(lines,people,vehicles,large,airspace,config,pf,variety=False):
    h=lines[0].split();mc,ac=map(int,h[4:6]);names=lines[1:1+mc];actors=lines[1+mc:1+mc+ac]
    tail=lines[1+mc+ac:];appearances=[]
    if variety:
        faction=city_faction(config,pf)
        pool=[r for r in people if r['factions']=='all' or faction in r['factions'].split()]
        if len(pool)<12:raise ValueError('Fewer than 12 compatible original city appearances')
        # Replace just the nine legacy clothing references. Existing non-people
        # actors, route coordinates, phases and parent ordering are preserved.
        names=[r['name']+'.lrm' for r in pool]+names[9:]
        delta=len(pool)-9;updated=[]
        for line in actors:
            fields=line.split();old=int(fields[1]);fields[1]=str(0 if fields[0]=='0' else old+delta)
            # Parent indices are actor indices and must never be remapped here.
            updated.append(' '.join(fields))
        actors=updated;appearances=[(i,r['role']) for i,r in enumerate(pool)]
    def add_flight(receipt,route,motion,phase,tier,pause):
        name=receipt['name']+'.lrm'
        if name not in names:names.append(name)
        start,end=route['start'],route['end'];speed=route['speed'];length=math.dist(start,end)
        duration=length/speed
        if not 2<duration<120:raise ValueError('Aerial traversal duration outside renderer budget')
        fields=[2,names.index(name),tier,motion,1,speed,phase,700 if motion==8 else 360,0,2,*start,*end,
                duration,pause,0,0,0,0,0,-1,0]
        actors.append(' '.join(map(str,fields)))
        parent=len(actors)-1
        for side,anchor in enumerate(receipt.get('navigation_anchors',[])):
            lens='lens-'+('port' if side==0 else 'starboard')+'.lrm'
            if lens not in names:names.append(lens)
            fields=[4,names.index(lens),tier,0,3.5 if motion==8 else 1,0,0,700 if motion==8 else 360,0,1,*anchor,
                    16,0,-math.pi/2 if side==0 else math.pi/2,0,3 if motion==8 else .35,0xae1c12 if side==0 else 0x149848,0,parent,0]
            actors.append(' '.join(map(str,fields)))
    if 'phasefront' in airspace.get('routes',{}) and vehicles:
        route=airspace['routes']['phasefront']
        # Four original variants share a corridor with staggered deterministic
        # schedules. The combined renderer cap bounds overlaps with ordinary air.
        for i,r in enumerate(vehicles):add_flight(r,route,7,(pf*37+i*97)%420,1 if i<3 else 2,420)
    if large and 'large' in airspace.get('routes',{}):
        add_flight(large,airspace['routes']['large'],8,(pf*79)%900,1,900)
        add_flight(large,airspace['routes']['large'],8,(pf*79+450)%900,2,900)
    h[0]='LRSCENE8';h[4]=str(len(names));h[5]=str(len(actors));h[6]='v8-ground-skimming-static-review-live-check-required'
    result=[' '.join(h),*names,*actors,*tail,'APPEARANCES '+str(len(appearances))]
    result+=[' '.join(map(str,row)) for row in appearances]
    if variety:
        masks=[]
        for i,line in enumerate(actors):
            if line.split()[0]!='0':continue
            allowed=airspace.get('appearance_clearance',{}).get(str(i),[])
            mask=sum(1<<j for j,r in enumerate(pool) if r['name'] in allowed)
            masks.append((i,mask))
        if not any(mask for i,mask in masks):raise ValueError('No expanded body clears the existing city routes')
        result+=['ROSTER '+str(len(masks)),*[' '.join(map(str,row)) for row in masks]]
    graph=airspace.get('walk_graph',dict(nodes=[],edges=[]))
    normal={'av-jacket-male','av-jacket-female','av-jacket-stocky'}
    def mask(allowed):
        if not variety:return 0xffffffff if normal<=set(allowed) else 0
        return sum(1<<i for i,r in enumerate(pool) if r['name'] in allowed)
    edges=[(e['a'],e['b'],mask(e['allowed'])) for e in graph['edges']]
    edges=[e for e in edges if e[2]]
    result+=['WALKGRAPH '+str(len(graph['nodes']))+' '+str(len(edges))]
    result+=[' '.join(map(str,[*n['point'],mask(n['rest'])])) for n in graph['nodes']]
    result+=[' '.join(map(str,e)) for e in edges]
    lanes=airspace.get('flight_lanes',[])
    result+=['FLIGHTLANES '+str(len(lanes))]
    result+=[' '.join(map(str,[*lane['start'],*lane['end'],int(lane.get('low_flight',False))])) for lane in lanes]
    return result,names
