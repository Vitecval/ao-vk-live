"""Authored recipes for original installed AO appearances, never extracted art.

Player clothing uses AO's named modular body slots. Robes, battlesuits and elite
armor keep their own original geometry/materials. Helmets replace the complete
head (including hair). Unsupported recipes are omitted individually.
"""
import math
import hashlib
import struct
import numpy as np
from PIL import Image
from ao_living_assets import parse_mesh, parse_animation
from prepare_local import build
import cache_v3 as cache


def recipes():
    jacket=dict(arms=42252,body=42253,legs=42250,feet=40903,hands={'type':1010011,'id':8773})
    blue=dict(arms=42247,body=42249,legs=42248,feet=40903,hands={'type':1010011,'id':8773})
    worker=dict(arms=215120,body=215121,legs=215124,feet=9615,hands=215123)
    suit=dict(arms=9606,body=9613,legs=9626,feet=9619,hands={'type':1010011,'id':8773})
    leather=dict(arms=9451,body=8732,legs=9453,feet=9450,hands=9452)
    ribbed=dict(arms=8815,body=8740,legs=8813,feet=8814,hands=8816)
    nomad=dict(arms=15812,body=15813,legs=15804,feet=15811,hands=15810)
    omni=dict(arms=8735,body=8738,legs=8746,feet=8742,hands=8744)
    # Original, distinct body geometry, head geometry, clothing and armor sets.
    rows=[
        ('jacket-male',5907,40682,jacket,0,'all'),
        ('jacket-thin',17534,40684,blue,0,'all'),
        ('jacket-stocky',17532,40686,jacket,0,'all'),
        ('jacket-female',5927,40629,blue,0,'all'),
        ('jacket-female-thin',17530,40631,jacket,0,'all'),
        ('jacket-female-stocky',17528,40632,blue,0,'all'),
        ('opifex-traveler',5914,40250,blue,0,'neutral clan'),
        ('opifex-traveler-female',5934,40211,jacket,0,'neutral clan'),
        ('nanomage-citizen',5921,40173,blue,0,'all'),
        ('nanomage-citizen-female',5941,40137,jacket,0,'all'),
        ('coat-male',17870,40687,{},0,'all'),
        ('coat-female',17919,40634,{},0,'all'),
        ('road-worker',17532,40683,worker,1,'all'),
        ('road-worker-female',5927,40633,worker,1,'all'),
        ('corporate-male',17534,40688,suit,1,'neutral omni'),
        ('corporate-female',17530,40630,suit,1,'neutral omni'),
        ('leather-adventurer',5907,40666,leather,2,'neutral clan'),
        ('ribbed-adventurer',5927,40632,ribbed,2,'neutral clan'),
        ('nomad-traveler',17534,20103,nomad,2,'neutral clan'),
        ('omni-security',5907,20104,omni,2,'omni'),
        ('omni-unicorn',246883,20106,{},3,'omni'),
        ('omni-elite',247037,20106,{},3,'omni'),
        ('battlesuit',41416,None,{},3,'all'),
    ]
    female={5927,5934,5941,17528,17530,17919}
    skins={
        'sm':dict(body=8767,legs=8768,arms=8774,hands=8773,feet=8761),
        'sf':dict(body=8859,legs=8881,arms=8856,hands=8865,feet=8862),
        'om':dict(body=8967,legs=8981,arms=8974,hands=8969,feet=8968),
        'of':dict(body=8889,legs=8904,arms=8898,hands=8892,feet=8890),
        'nm':dict(body=8991,legs=9005,arms=8999,hands=8993,feet=8992),
        'nf':dict(body=9217,legs=9230,arms=9223,hands=9219,feet=9218),
    }
    result=[]
    for n,m,h,t,role,faction in rows:
        breed={5914:'om',5934:'of',5921:'nm',5941:'nf'}.get(m,'sf' if m in female else 'sm')
        t=t.copy()
        if m not in (246883,247037,41416):
            if 'hands' not in t or isinstance(t['hands'],dict):t['hands']={'type':1010011,'id':skins[breed]['hands']}
            t.setdefault('feet',9615)
        result.append(dict(name='av-'+n,model=m,head=h,textures=t,skin=skins[breed],walk=10162 if m in female else 10191,
                           idle=10135 if m in female else 10173,dimension=512,appearance_role=role,factions=faction,human=True))
    return result


# These are actual item -> cast nano -> appearance template -> CIR chains,
# distinct from the similarly named showroom holograms. Hash-pin the complete
# chain during local preparation. Each CIR includes the original seated rider.
PHASEFRONTS=[
    dict(name='pf-wraith-desert',item=270646,nano=270645,visual=269462,model=269458,idle=269459,head=40682),
    dict(name='pf-wraith-black',item=270712,nano=270711,visual=270703,model=270681,idle=269459,head=40684),
    dict(name='pf-spectre-green',item=270837,nano=270836,visual=270812,model=270811,idle=270815,head=40686),
    dict(name='pf-banshee-yellow',item=270983,nano=270982,visual=270947,model=270907,idle=270938,head=40688),
]


def verify_recipe(db,item):
    model=parse_mesh(db.get(1010002,item['model']))
    for kind,key in ((1010003,'walk'),(1010003,'idle')):
        clip=parse_animation(db.get(kind,item[key]))
        if clip['skeleton']!=model['skeleton']:
            raise ValueError('Original model/animation skeleton identity mismatch')
    if item.get('head') and not any('head' in a['name'].lower() and 0<=a['bone']<len(model['bones']) for a in model['attractors']):
        raise ValueError('Original head attractor missing')
    if not set(item.get('textures',{})) <= {s[0] for s in model['slots']}:
        raise ValueError('Clothing references a material absent from original body')
    hashes={f'{k}:{item[key]}':hashlib.sha256(db.get(k,item[key])).hexdigest()
            for k,key in ((1010002,'model'),(1010003,'walk'),(1010003,'idle'))}
    if item.get('head'):hashes[f"1010001:{item['head']}"]=hashlib.sha256(db.get(1010001,item['head'])).hexdigest()
    return hashes


def phasefront_recipe(db,definition):
    item=dict(definition,walk=definition['idle'],dimension=512,grounded=False,posed_vehicle=True,
              textures=dict(arms=42252,body=42253,legs=42250,feet=40903,hands={'type':1010011,'id':8773}))
    product=db.get(1000020,item['item']);nano=db.get(1040005,item['nano']);visual=db.get(1040023,item['visual'])
    if struct.unpack_from('<I',product,len(product)-4)[0]!=item['nano']:
        raise ValueError('Phasefront item no longer casts the reviewed nano')
    if struct.pack('<I',item['visual']) not in nano or struct.unpack_from('<I',visual,24)[0]!=12 or struct.unpack_from('<I',visual,28)[0]!=item['model']:
        raise ValueError('Phasefront nano/appearance/model chain differs')
    family={'wraith':b'Wraith X2-45','spectre':b'Spectre','banshee':b'Banshee'}
    token=next(v for k,v in family.items() if k in item['name'])
    if token not in product or token not in nano:raise ValueError('Phasefront product identity differs')
    item['source_chain']={f'{k}:{i}':hashlib.sha256(b).hexdigest() for k,i,b in
                          ((1000020,item['item'],product),(1040005,item['nano'],nano),(1040023,item['visual'],visual))}
    return item


def shared_atlas(out,receipts,dimension=1024):
    """Share exact decoded material tiles, including normals, ORM and emission.

    Stable ordering and UV remapping preserve each geometry's original material
    correspondence. Pool sharing keeps dozens of rigs inside the 64 MiB budget.
    """
    tiles={};models=[]
    for receipt in receipts:
        m=cache.mesh(out/(receipt['name']+'.lrm'));dim=m['header'][5];side=dim//4
        field=12 if m['header'][6] else 6
        uv=m['vertices'][:,field:field+2];old=np.floor(uv*4).astype(int);indices=old[:,0]+4*old[:,1]
        mapping={}
        for tile in sorted(set(indices)):
            y,x=divmod(int(tile),4)
            blocks=tuple(p[y*side:(y+1)*side,x*side:(x+1)*side].copy() for p in m['maps'])
            key=hashlib.sha256(b''.join(p.tobytes() for p in blocks)).hexdigest()
            if key not in tiles:tiles[key]=(len(tiles),blocks)
            mapping[int(tile)]=tiles[key][0]
        models.append((receipt,m,uv.copy()*4-old,indices,mapping,field))
    grid=math.ceil(math.sqrt(len(tiles)));side=dimension//grid
    if side<40:raise ValueError('Appearance atlas texel budget exceeded')
    maps=[np.full((dimension,dimension),c,np.uint32) for c in (0xff000000,0xff8080ff,0xffffcd00,0xff000000)]
    for tile,blocks in tiles.values():
        y,x=divmod(tile,grid)
        for target,block in zip(maps,blocks):
            im=Image.fromarray(cache.rgba(block)).resize((side,side),Image.Resampling.LANCZOS)
            target[y*side:(y+1)*side,x*side:(x+1)*side]=cache.argb(np.asarray(im))
    for receipt,m,local,indices,mapping,field in models:
        uv=m['vertices'][:,field:field+2]
        for old,tile in mapping.items():
            uv[indices==old]=(local[indices==old]*side+np.array((tile%grid,tile//grid))*side)/dimension
        m['header'][5]=dimension;m['maps']=maps;cache.save(out/(receipt['name']+'.lrm'),m)
        receipt['shared_atlas']=dict(dimension=dimension,tiles=len(tiles),tile_pixels=side)
        receipt['sha256']=hashlib.sha256((out/(receipt['name']+'.lrm')).read_bytes()).hexdigest()
    return dict(tiles=len(tiles),dimension=dimension,tile_pixels=side)


def make_lods(mesh):
    """Conservative distant vertex clustering, retaining original vertices/UVs.

    Normal and UV bins protect hard edges/material seams. Collapsed triangles
    are reconnected through their representatives, never randomly discarded.
    Maximum silhouette displacement is bounded by a cluster's diagonal.
    """
    v=mesh['vertices'];span=float(np.ptp(v[:,:3],axis=0).max());original=mesh['indices'][0]
    levels=[original];errors=[0.]
    for fraction in (.004,.009):
        cell=max(.015,span*fraction)
        key=np.c_[np.floor(v[:,:3]/cell),np.floor(v[:,3:6]*8),np.floor(v[:,6:8]*mesh['header'][5]/3)].astype(np.int64)
        _,first,inverse=np.unique(key,axis=0,return_index=True,return_inverse=True)
        mapped=first[inverse[original]].reshape(-1,3)
        keep=(mapped[:,0]!=mapped[:,1])&(mapped[:,1]!=mapped[:,2])&(mapped[:,2]!=mapped[:,0])
        indices=mapped[keep].astype('<u2').ravel()
        if len(indices)<3:indices=original
        levels.append(indices);errors.append(math.sqrt(3)*cell)
    mesh['indices']=levels;mesh['header'][2:5]=[len(i) for i in levels]
    return dict(triangles=[len(i)//3 for i in levels],maximum_vertex_displacement_m=errors)


def prepare_expanded(db,client,out,errors):
    people=[];vehicles=[]
    for item in recipes():
        try:
            print('Preparing distinct original appearance: '+item['name'],flush=True)
            source=verify_recipe(db,item);r=build(db,client,item,out)
            r.update(role=item['appearance_role'],factions=item['factions'],source_resources=source)
            people.append(r)
        except Exception as error:errors[item['name']]=str(error)
    for definition in PHASEFRONTS:
        try:
            item=phasefront_recipe(db,definition);source=verify_recipe(db,item)
            print('Preparing original Phasefront with rider: '+item['name'],flush=True)
            r=build(db,client,item,out);r.update(source_resources=source,source_chain=item['source_chain'])
            # The original pose is baked into a rigid composite. Rider and head
            # use the exact CIR attractor and palette, not a guessed seat offset.
            m=cache.mesh(out/(item['name']+'.lrm'))
            from prepare_local import skin_points
            model=parse_mesh(db.get(1010002,item['model']));anim=parse_animation(db.get(1010003,item['idle']))
            from ao_living_assets import palettes
            palette=palettes(model,anim,0);v=m['vertices'];ids=v[:,14:16].copy().view('<i4')
            rigid=np.zeros((len(v),8),np.float32);rigid[:,:3]=skin_points(v,ids,palette)
            weight=v[:,16:17]
            rigid[:,3:6]=np.einsum('ni,nij->nj',v[:,9:12],palette[ids[:,0],:3,:3])*weight+np.einsum('ni,nij->nj',v[:,9:12],palette[ids[:,1],:3,:3])*(1-weight)
            rigid[:,3:6]/=np.maximum(np.linalg.norm(rigid[:,3:6],axis=1,keepdims=True),1e-9);rigid[:,6:8]=v[:,12:14]
            center=(rigid[:,:3].min(0)+rigid[:,:3].max(0))/2;rigid[:,:3]-=center
            r['radius']=float(np.linalg.norm(rigid[:,:3],axis=1).max());r['rider_baked_from_original_pose']=True
            r['navigation_anchors']=[(rigid[np.argmin(rigid[:,0]),:3]-.025*np.array([1,0,0])).tolist(),
                                     (rigid[np.argmax(rigid[:,0]),:3]+.025*np.array([1,0,0])).tolist()]
            r['radius']=max(r['radius'],max(np.linalg.norm(p)+.06 for p in r['navigation_anchors']))
            m['vertices']=rigid;m['header'][0]=0x324d524c;m['header'][6:10]=[0,0,0,0];m['extra']=b'';m['palettes']=b''
            cache.save(out/(item['name']+'.lrm'),m);vehicles.append(r)
        except Exception as error:errors[definition['name']]=str(error)
    if people:shared_atlas(out,people)
    if vehicles:shared_atlas(out,vehicles,512)
    for r in vehicles:
        m=cache.mesh(out/(r['name']+'.lrm'));r['lods']=make_lods(m);cache.save(out/(r['name']+'.lrm'),m)
        r['sha256']=hashlib.sha256((out/(r['name']+'.lrm')).read_bytes()).hexdigest()
    return people,vehicles
