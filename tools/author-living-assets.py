"""Original evaluation assets. No source game art, downloads or copied game models.
Pillow and NumPy are authoring dependencies only; the runtime reads LRM2 directly.
"""
import sys, math, struct, json, hashlib
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'experiment/assets'; OUT.mkdir(parents=True,exist_ok=True)
S=512; T=128
atlas=Image.new('RGB',(S,S)); orm=Image.new('RGB',(S,S)); emission=Image.new('RGB',(S,S)); height=Image.new('L',(S,S),128)
draw=ImageDraw.Draw(atlas); od=ImageDraw.Draw(orm); ed=ImageDraw.Draw(emission); hd=ImageDraw.Draw(height)
try:font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',10)
except OSError:font=ImageFont.load_default()
colors=[(150,149,133),(39,47,49),(99,110,113),(159,128,60),(28,51,59),(47,53,53),(43,65,64),(99,69,27),(26,30,29),(177,176,154),(119,75,48),(51,67,65),(89,96,88),(65,70,65),(134,136,119),(25,28,28)]
for k,col in enumerate(colors):
    x=(k%4)*T;y=(k//4)*T;box=(x,y,x+127,y+127)
    draw.rectangle(box,fill=col);od.rectangle(box,fill=(237,150 if k in (0,3,9,10,14) else 103,185 if k in (1,2,5,12,13) else 12))
    # Designed panel breaks, edge wear and restrained brushed grain.
    if k not in (0,4,6,7,8,11,14,15):
        for inset in (3,7):
            draw.rectangle((x+inset,y+inset,x+127-inset,y+127-inset),outline=tuple(max(0,c-35) for c in col),width=1)
            hd.rectangle((x+inset,y+inset,x+127-inset,y+127-inset),outline=100,width=1)
        draw.line((x+9,y+8,x+117,y+8),fill=tuple(min(255,c+22) for c in col))
        for px,py in ((12,12),(115,12),(12,115),(115,115)):
            draw.ellipse((x+px-2,y+py-2,x+px+2,y+py+2),fill=(66,70,67));draw.line((x+px-1,y+py,x+px+1,y+py),fill=(25,28,27));hd.ellipse((x+px-2,y+py-2,x+px+2,y+py+2),fill=145)
    if k in (9,):
        draw.text((x+17,y+22),'RK // CIVIL',font=font,fill=(49,57,53))
        draw.text((x+17,y+35),'WORKS  06',font=font,fill=(49,57,53))
        draw.text((x+17,y+95),'SERVICE / 24',font=font,fill=(49,57,53))
        for j in range(20):
            px=x+20+j*4;draw.line((px,y+69,px,y+83),fill=(47,52,48),width=1+(j%3==0))
    if k==3:
        stripe=Image.new('RGB',(128,128),col);sd=ImageDraw.Draw(stripe)
        for j in range(-128,256,24):sd.polygon([(j,10),(j+9,10),(j+119,119),(j+110,119)],fill=(45,48,40))
        atlas.paste(stripe,(x,y))
    if k==5:
        for yy in range(14,119,9):
            draw.rounded_rectangle((x+14,y+yy,x+114,y+yy+4),radius=2,fill=(15,21,21));hd.rectangle((x+14,y+yy,x+114,y+yy+3),fill=70)
    if k==4:
        for yy in range(10,119,12):draw.line((x+10,y+yy,x+118,y+yy),fill=(35,65,72))
        draw.ellipse((x+30,y+30,x+98,y+98),outline=(86,107,110),width=3);draw.ellipse((x+46,y+46,x+82,y+82),fill=(13,28,35),outline=(52,77,86),width=2)
        od.rectangle(box,fill=(255,58,65))
    if k in (6,7):
        c=(75,156,141) if k==6 else (205,139,52)
        draw.rounded_rectangle((x+19,y+50,x+109,y+78),radius=5,fill=c);ed.rounded_rectangle((x+19,y+50,x+109,y+78),radius=5,fill=tuple(int(v*.6) for v in c))
    if k==11:
        draw.rectangle((x+12,y+12,x+116,y+116),fill=(14,24,22),outline=(78,90,80),width=3)
        for j in range(6):draw.line((x+22,y+30+j*10,x+35+(j*13)%66,y+30+j*10),fill=(88,138,112),width=2)
        ed.rectangle((x+15,y+15,x+113,y+113),fill=(3,8,5))
    if k==8:
        for j in range(6,127,6):draw.line((x+4,y+j,x+123,y+j),fill=(34,38,36));hd.line((x+4,y+j,x+123,y+j),fill=135)
# Surface grain is directional and subtle, rather than broad random grunge.
rng=np.random.default_rng(224);arr=np.array(atlas).astype(float)
grain=rng.normal(0,.65,(S,S,1));arr=np.uint8(np.clip(arr+grain,0,255));atlas=Image.fromarray(arr)
h=np.array(height,dtype=float)/255;gy,gx=np.gradient(h);n=np.stack((-gx*5,gy*5,np.ones_like(h)),axis=-1);n/=np.linalg.norm(n,axis=-1,keepdims=True)
normal=Image.fromarray(np.uint8(np.clip(n*.5+.5,0,1)*255))
images=[atlas,normal,orm,emission]
for img,name in zip(images,('albedo','normal','orm','emissive')):img.save(OUT/f'service-{name}.png')

def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def norm(v):
    l=math.sqrt(sum(x*x for x in v));return tuple(x/l for x in v)
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
class Model:
    def __init__(self):self.vertices=[];self.faces=[];self.bones=[]
    def quad(self,pts,tile,detail=0,bone=0):
        if len(pts)==3:pts=pts+[pts[-1]]
        n=norm(cross(sub(pts[1],pts[0]),sub(pts[2],pts[0])));start=len(self.vertices)
        for p,(u,v) in zip(pts,((.08,.08),(.92,.08),(.92,.92),(.08,.92))):self.vertices.append((p,n,((tile%4+u)/4,(tile//4+v)/4),bone))
        self.faces.extend([(start,start+1,start+2,detail),(start,start+2,start+3,detail)])
    def box(self,c,size,tile=1,detail=0,bone=0):
        x,y,z=c;a,b,d=[v/2 for v in size]
        p=[(x-a,y-b,z-d),(x+a,y-b,z-d),(x+a,y+b,z-d),(x-a,y+b,z-d),(x-a,y-b,z+d),(x+a,y-b,z+d),(x+a,y+b,z+d),(x-a,y+b,z+d)]
        for ids in ((0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)):self.quad([p[i] for i in ids],tile,detail,bone)
    def loft(self,sections,tile=0):
        # Explicitly designed octagonal body stations along Z, with chamfered panel edges.
        rings=[]
        for z,w,h,cy in sections:
            rings.append([(w*x,cy+h*y,z) for x,y in ((-.72,-1),(.72,-1),(1,-.68),(1,.68),(.72,1),(-.72,1),(-1,.68),(-1,-.68))])
        for j in range(len(rings)-1):
            for k in range(8):self.quad([rings[j][k],rings[j][(k+1)%8],rings[j+1][(k+1)%8],rings[j+1][k]],tile if k in (0,2,4,6) else 2)
        for ring,rev in ((rings[0],True),(rings[-1],False)):
            center=tuple(sum(p[i] for p in ring)/8 for i in range(3))
            for k in range(8):self.quad([center,ring[(k+1)%8],ring[k]] if rev else [center,ring[k],ring[(k+1)%8]],tile)
    def ring(self,c,r1,r2,height,tile=2,segments=32,detail=0,bone=0):
        x,y,z=c
        for i in range(segments):
            a=i*math.tau/segments;b=(i+1)*math.tau/segments
            p=lambda r,h,t:(x+r*math.cos(t),y+h,z+r*math.sin(t))
            for pts in ([p(r2,-height/2,a),p(r2,-height/2,b),p(r2,height/2,b),p(r2,height/2,a)], [p(r1,height/2,a),p(r1,height/2,b),p(r1,-height/2,b),p(r1,-height/2,a)], [p(r1,height/2,a),p(r2,height/2,a),p(r2,height/2,b),p(r1,height/2,b)], [p(r1,-height/2,b),p(r2,-height/2,b),p(r2,-height/2,a),p(r1,-height/2,a)]):self.quad(pts,tile,detail,bone)
    def bolt(self,c):self.ring(c,.002,.012,.012,2,8,2)
    def rotor(self,c,r,bone=0):
        for j in range(7):
            a=j*math.tau/7
            pts=[]
            for xx,zz in ((.04,-.018),(r*.91,-.055),(r,.018),(.07,.032)):
                pts.append(add(c,(xx*math.cos(a)-zz*math.sin(a),.008,xx*math.sin(a)+zz*math.cos(a))))
            self.quad(pts,1,1,bone)
    def save(self,name,rig=False):
        assert len(self.vertices)<20000
        ordered=sorted(self.faces,key=lambda f:f[3])
        lods=[[v for f in ordered if f[3]<=d for v in f[:3]] for d in (2,1,0)]
        frames=90 if rig else 0;bones=4 if rig else 0
        with (OUT/f'{name}.lrm').open('wb') as f:
            f.write(struct.pack('<8IfI',0x324d524c,len(self.vertices),*[len(x) for x in lods],S,bones,frames,30. if rig else 0.,0))
            for p,n,uv,b in self.vertices:
                if rig:
                    pivot=((0,0,0),(0,-.26,.31),(-.64,0,0),(.64,0,0))[b];local=sub(p,pivot)
                    f.write(struct.pack('<14f2if',*local,*local,*p,*n,*uv,b,b,1.))
                else:f.write(struct.pack('<8f',*p,*n,*uv))
            for idx in lods:f.write(struct.pack('<'+'H'*len(idx),*idx))
            if rig:
                for frame in range(frames):
                    t=frame/30
                    for b,pivot in enumerate(((0,0,0),(0,-.26,.31),(-.64,0,0),(.64,0,0))):
                        angle=0 if b==0 else .32*math.sin(t*math.tau/3) if b==1 else t*math.tau*3
                        c=math.cos(angle);s=math.sin(angle);f.write(struct.pack('<12f',c,0,-s,0,1,0,s,0,c,*pivot))
            for img in images:
                rgba=np.array(img.convert('RGBA')); argb=(rgba[:,:,3].astype('uint32')<<24)|(rgba[:,:,0].astype('uint32')<<16)|(rgba[:,:,1].astype('uint32')<<8)|rgba[:,:,2].astype('uint32')
                f.write(argb.astype('<u4').tobytes())
        # Editable OBJ source (bind pose, authored UVs and normals), same material atlas.
        with (OUT/f'{name}.obj').open('w') as f:
            f.write('mtllib service.mtl\nusemtl service\n')
            for p,n,uv,b in self.vertices:f.write('v %g %g %g\n'%p)
            for p,n,uv,b in self.vertices:f.write('vt %g %g\n'%(uv[0],1-uv[1]))
            for p,n,uv,b in self.vertices:f.write('vn %g %g %g\n'%n)
            for face in self.faces:f.write('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in face[:3])+'\n')
        return {'name':name,'vertices':len(self.vertices),'triangles':[len(x)//3 for x in lods],'bones':bones,'frames':frames,'fps':30 if rig else 0,'texture_dimensions':[S,S],'quality':'original evaluation asset; production art approval pending'}

d=Model();d.loft([(-.55,.22,.13,.01),(-.40,.33,.20,0),(.27,.31,.19,0),(.47,.20,.11,-.02)])
d.box((0,.205,-.06),(.39,.025,.58),9);d.box((0,-.18,-.13),(.36,.055,.40),5)
d.box((0,.235,-.27),(.24,.025,.10),11)
for side in (-1,1):
    d.box((side*.29,.06,-.06),(.035,.16,.40),14)
    d.box((side*.29,.157,-.06),(.03,.025,.40),2)
    d.box((side*.29,-.04,-.06),(.03,.025,.40),1)
d.box((0,-.31,.31),(.21,.14,.17),1,bone=1);d.box((0,-.31,.405),(.16,.10,.025),4,bone=1)
d.box((0,-.24,.10),(.07,.14,.08),2)
for side in (-1,1):
    d.box((side*.43,0,-.04),(.24,.08,.16),2);d.box((side*.39,.055,-.03),(.20,.025,.08),3)
    d.ring((side*.64,0,0),.225,.285,.15,0,40);d.ring((side*.64,.09,0),.225,.26,.018,2,40)
    d.ring((side*.64,0,0),.012,.055,.19,1,24);d.rotor((side*.64,0,0),.21,2 if side<0 else 3)
    for a in range(0,360,60):
        v=math.radians(a);d.box((side*.64+.24*math.cos(v),.11,.24*math.sin(v)),(.035,.025,.035),2,1)
    d.box((side*.24,.095,.34),(.08,.035,.12),6)
    d.box((side*.325,.02,-.18),(.015,.10,.24),5)
    for z in (-.3,-.12,.08,.27):d.bolt((side*.22,.224,z))
    d.box((side*.19,-.25,-.35),(.055,.14,.10),8)
    d.box((side*.19,-.33,-.35),(.08,.03,.15),2)
inventory=[d.save('civil-service-drone',True)]

c=Model();c.loft([(-3,.68,.55,.1),(-2.7,.85,.67,0),(1.6,.80,.60,0),(2.65,.46,.32,-.06),(3.15,.14,.14,-.08)],10)
c.box((0,.63,1.4),(1.1,.03,1.2),4);c.box((0,-.58,-.3),(1.5,.06,2.8),1)
for side in (-1,1):
    # Swept load-bearing outriggers, airfoils and integral nacelles, with service access panels.
    c.quad([(side*.7,.05,-1.7),(side*2.1,-.10,-2.4),(side*1.8,-.03,-.5),(side*.7,.15,.7)],0)
    c.quad([(side*.7,-.10,.7),(side*1.8,-.20,-.5),(side*2.1,-.28,-2.4),(side*.7,-.20,-1.7)],1)
    for z in (-1.9,-.8):
        c.box((side*1.75,-.12,z),(.43,.38,.65),0);c.box((side*1.75,-.12,z-.34),(.29,.23,.025),5)
    c.box((side*.82,.12,-.7),(.04,.38,2.4),9)
    c.box((side*1.97,.04,-2.18),(.06,.045,.14),7 if side<0 else 6)
    for z in (-2,-1.3,-.6,.1,.7):c.box((side*.72,.63,z),(.17,.08,.13),2,1)
c.box((0,.73,-1.8),(.06,.38,.7),1);c.box((0,.93,-1.8),(.4,.05,.7),0)
inventory.append(c.save('frontier-cargo-lifter'))

f=Model();f.ring((0,0,0),.018,.13,.10,2,32);f.rotor((0,0,0),.68)
for j in range(7):
    a=j*math.tau/7;f.box((.1*math.cos(a),.07,.1*math.sin(a)),(.02,.025,.02),2,2)
inventory.append(f.save('industrial-extractor-rotor'))
(OUT/'service.mtl').write_text('newmtl service\nKd 1 1 1\nmap_Kd service-albedo.png\nmap_Bump service-normal.png\nPr .6\nPm .2\n')
(OUT/'inventory.json').write_text(json.dumps(inventory,indent=2))
(OUT/'PROVENANCE.md').write_text('''# Living Rubi-Ka asset provenance

These three models and four atlas textures were originally authored for this
experiment using tools/author-living-assets.py. No downloaded model, game asset,
AI concept image, production texture-worker art or Parallax Windows resource was used.
The authoring script is the editable design source; bind-pose OBJ exports accompany
the LRM2 runtime files. The service drone includes four independent rigid bones,
90 palette frames, scanner oscillation and two rotor animations. LODs omit small
hardware and then rotor blades; all use one shared material per asset.

Generated geometry and texture artwork: CC0-1.0 dedication, to the extent permitted
by law. This does not grant rights to Anarchy Online trademarks or original game art.
Source code retains the repository MIT license. Pillow/NumPy are authoring tools,
not runtime or ZIP dependencies. Fonts are rasterized from the local Windows
Consolas font and the font file is not redistributed.

Art status: evaluation assets, pending visual review. No citizen, clothing,
walking/idle animation or wildlife asset is included. No claim of production
quality or AO-authored asset compatibility is made.
''')
def scene(path,actors):
    path.write_text('LRSCENE2 4294967294 1 1 3 '+str(len(actors))+' controlled-laboratory-v1\n'+'\n'.join(x['name']+'.lrm' for x in inventory)+'\n'+'\n'.join(actors)+'\n')
scene(OUT/'fixture.lrs',[
    '1 0 1 0 1 .30 0 90 .025 4 -1.8 2 1.4 -1.5 2.15 3 -.2 2.2 3 .3 2 1.4',
    '1 0 2 0 .9 .24 1.3 90 .018 4 2.5 2.6 5 3.5 2.7 6 4.4 2.7 4.5 3.5 2.6 3.5',
    '2 1 1 0 1.4 7 7 500 0 4 -45 22 60 55 24 75 65 27 125 -50 25 120',
    '4 2 1 1 1 1.9 0 65 0 1 1.9 .88 2.4',
    '5 2 1 2 1 0 0 40 0 1 2.9 .3 2.4',
])
# No live scene.lrs: no production coordinates have been validated.
print(json.dumps(inventory,indent=2))
