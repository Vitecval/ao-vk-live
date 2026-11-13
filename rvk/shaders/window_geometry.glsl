// A shared, building-sized virtual floor plan. Rays begin at the ACTUAL glass
// point and intersect 3D room/furniture primitives in the building's coordinates.
// A small window clips this space; no complete room picture is fitted to it.

bool RoomBox(vec3 ro,vec3 rd,vec3 lo,vec3 hi,out float first,out float last,out vec3 normal)
{
    vec3 safe=mix(vec3(-1e-7),vec3(1e-7),greaterThanEqual(rd,vec3(0)));
    safe=mix(safe,rd,greaterThan(abs(rd),vec3(1e-7)));
    vec3 a=(lo-ro)/safe,b=(hi-ro)/safe,enter=min(a,b),exit=max(a,b);
    first=max(enter.x,max(enter.y,enter.z));last=min(exit.x,min(exit.y,exit.z));
    normal=enter.x>=max(enter.y,enter.z)?vec3(-sign(safe.x),0,0):
           enter.y>=enter.z?vec3(0,-sign(safe.y),0):vec3(0,0,-sign(safe.z));
    return last>=max(first,0.0);
}

bool RoomShape(vec3 ro,vec3 rd,vec3 lo,vec3 hi,uint shape,out float t,out vec3 n)
{
    if(shape==2u){ // tapered fabric lamp shade: finite cone side plus two caps
        vec3 c=(lo+hi)*.5,p=ro-c;
        float halfH=(hi.y-lo.y)*.5,r0=(hi.x-lo.x)*.5,slope=-r0*.35/(2.0*halfH);
        float radius=r0+slope*(p.y+halfH);
        float a=dot(rd.xz,rd.xz)-slope*slope*rd.y*rd.y;
        float b=dot(p.xz,rd.xz)-radius*slope*rd.y,q=b*b-a*(dot(p.xz,p.xz)-radius*radius);
        t=1e20;n=vec3(0);
        if(q>=0.0&&abs(a)>1e-8){
            float root=sqrt(q);
            for(int k=0;k<2;++k){
                float at=(-b+(k==0?-root:root))/a;
                vec3 hit=p+rd*at;
                if(at>.002&&at<t&&abs(hit.y)<=halfH){
                    float r=r0+slope*(hit.y+halfH);
                    t=at;n=normalize(vec3(hit.x,-slope*r,hit.z));
                }
            }
        }
        if(abs(rd.y)>1e-7)for(int k=0;k<2;++k){
            float y=k==0?-halfH:halfH,at=(y-p.y)/rd.y;
            vec2 xz=p.xz+rd.xz*at;float r=r0+slope*(y+halfH);
            if(at>.002&&at<t&&dot(xz,xz)<=r*r){t=at;n=vec3(0,k==0?-1:1,0);}
        }
        return t<1e19;
    }
    if(shape==4u){ // a few broad leaves, not a single oval plant blob
        vec3 size=hi-lo;t=1e20;n=vec3(0);
        const vec3 centres[3]=vec3[3](vec3(.29,.38,.40),vec3(.55,.68,.53),vec3(.73,.38,.59));
        const vec3 radii[3]=vec3[3](vec3(.29,.19,.27),vec3(.16,.30,.17),vec3(.26,.18,.24));
        for(int i=0;i<3;++i){
            vec3 c=lo+size*centres[i],r=max(size*radii[i],vec3(.001));
            vec3 p=(ro-c)/r,d=rd/r;
            float a=dot(d,d),b=dot(p,d),q=b*b-a*(dot(p,p)-1.0);
            if(q<0.0)continue;
            float at=(-b-sqrt(q))/a;
            if(at>.002&&at<t){t=at;n=normalize((ro+rd*at-c)/(r*r));}
        }
        return t<1e19;
    }
    if(shape==1u){ // ellipsoid pillows and lamp shades, with curved normals
        vec3 c=(lo+hi)*.5,r=max((hi-lo)*.5,vec3(.001));
        vec3 p=(ro-c)/r,d=rd/r;
        float a=dot(d,d),b=dot(p,d),q=b*b-a*(dot(p,p)-1.0);
        if(q<0.0){t=1e20;n=vec3(0);return false;}
        t=(-b-sqrt(q))/a;
        if(t<=.002){t=1e20;n=vec3(0);return false;}
        n=normalize((ro+rd*t-c)/(r*r));return true;
    }
    float leave;
    if(!RoomBox(ro,rd,lo,hi,t,leave,n)||t<=.002){t=1e20;return false;}
    return true;
}

// Material tiles: plaster warm/cool, wood, metal, quilt, rug, books, terminal.
// 8/9 are geometric lamp emitters; 10 is cream pillow fabric.
// Structural doors are shared by all panes, including unfurnished rooms.
uint gWindowDoorWall=0u;
bool gWindowDoorOpen=false;
float gWindowDoorInset=0.0,gWindowHabit=0.0;
vec3 RoomTurnPoint(vec3 p,vec3 s,uint turn);
void RoomProp(vec3 s,uint preset,int i,out vec3 lo,out vec3 hi,out uint mat,out uint shape)
{
    lo=hi=vec3(0);mat=2u;shape=0u;
    // Subtle retains the same door while omitting optional furnishing detail.
    if((D.windows.x<1.5 || gWindowDetail<3u) && i>=16)i+=8;
    if(i>=24){
        uint wall=gWindowDoorWall;
        vec3 space=(wall&1u)!=0u?s.zyx:s;
        float x=space.x*.5-.45,z=space.z-gWindowDoorInset;
        bool office=preset==2u||(preset>=6u&&preset<=8u);
        if(i==24){lo=vec3(x,.02,z-(gWindowDoorOpen?.88:.085));hi=lo+vec3(gWindowDoorOpen?.045:.90,2.10,gWindowDoorOpen?.88:.045);mat=office?13u:12u;}
        if(i==25){lo=vec3(x-.075,0,z-.13);hi=lo+vec3(.075,2.22,.11);mat=office?3u:2u;}
        if(i==26){lo=vec3(x+.90,0,z-.13);hi=lo+vec3(.075,2.22,.11);mat=office?3u:2u;}
        if(i==27){lo=vec3(x-.075,2.10,z-.13);hi=lo+vec3(1.05,.12,.11);mat=office?3u:2u;}
        if(i==28){lo=vec3(x+(gWindowDoorOpen?.06:.72),.95,z-(gWindowDoorOpen?.73:.16));hi=lo+vec3(.12,.035,.04);mat=3u;}
        if(gWindowDoorOpen){
            if(i==29){lo=vec3(0,0,z-.07);hi=vec3(x-.075,space.y,z+.07);mat=office?1u:0u;}
            if(i==30){lo=vec3(x+.975,0,z-.07);hi=vec3(space.x,space.y,z+.07);mat=office?1u:0u;}
            if(i==31){lo=vec3(x-.075,2.22,z-.07);hi=vec3(x+.975,space.y,z+.07);mat=office?1u:0u;}
        }
        vec3 a=RoomTurnPoint(lo,space,wall),b=RoomTurnPoint(hi,space,wall);
        lo=min(a,b);hi=max(a,b);return;
    }
    float sx=s.x,sz=s.z-gWindowDoorInset,k=clamp(min(sx,sz)/4.5,.65,1.05);
    float bx=max(.25,sx-2.4*k),bz=max(.25,sz-2.6*k);
    bool office=preset==2u||(preset>=6u&&preset<=8u);
    bool sparse=preset==4u;
    if(office){
        float deskZ=max(.4,sz-1.5*k);
        if(i==0){lo=vec3(.35,.82,deskZ);hi=lo+vec3(1.8*k,.12,.8*k);}
        if(i==1){lo=vec3(.4,.04,deskZ+.08);hi=lo+vec3(.5*k,.77,.65*k);mat=3u;}
        if(i==2){lo=vec3(1.02*k,.98,deskZ+.56*k);hi=lo+vec3(.72*k,.48,.09);mat=3u;}
        if(i==3){lo=vec3(1.06*k,1.02,deskZ+.548*k);hi=lo+vec3(.64*k,.39,.012);mat=7u;}
        if(i==4){lo=vec3(1.03*k,.43,deskZ-.64*k);hi=lo+vec3(.58*k,.12,.58*k);mat=4u;}
        if(i==5){lo=vec3(1.03*k,.52,deskZ-.69*k);hi=lo+vec3(.58*k,.66,.09);mat=4u;}
        if(i==6){lo=vec3(max(.2,sx-.95*k),0,.35);hi=lo+vec3(.7*k,1.75,.65*k);mat=3u;}
        if(i==7){lo=vec3(.25,1.78,sz-.28);hi=lo+vec3(min(2.5*k,sx-.5),.09,.23);}
        if(i==8){lo=vec3(.35,1.87,sz-.25);hi=lo+vec3(min(2.1*k,sx-.7),.36,.20);mat=6u;}
        if(i==9){lo=vec3(max(.2,sx-.95*k),.08,sz*.42);hi=lo+vec3(.7*k,2.0,.75*k);mat=3u;}
        if(i==10){lo=vec3(sx*.5-.5,s.y-.20,sz*.5-.18);hi=lo+vec3(1,.08,.36);mat=9u;}
        if(i==11){lo=vec3(2.02*k,.02,deskZ+.61*k);hi=lo+vec3(.07,.80,.07);mat=3u;}
        if(i==12){lo=vec3(1.25*k,.06,deskZ-.39*k);hi=lo+vec3(.08,.38,.08);mat=3u;}
        if(i==13){lo=vec3(1.0*k,.03,deskZ-.43*k);hi=lo+vec3(.58*k,.05,.09);mat=3u;}
        if(i==14){lo=vec3(sx*.5,0,sz*.58);hi=lo+vec3(.08,1.7,max(.3,sz*.35));mat=1u;}
        if(i==15){lo=vec3(.3,.75,.35);hi=lo+vec3(1.4*k,.12,.7*k);mat=3u;}
        if(i==16){lo=vec3(.35,.03,.39);hi=lo+vec3(.4,.71,.6);mat=3u;}
        if(i==17){lo=vec3(.6,.87,.65);hi=lo+vec3(.35,.17,.25);mat=3u;}
        if(i==18){lo=vec3(sx*.5-1.1,.008,sz*.5-1.2);hi=lo+vec3(2.2,.012,2.4);mat=5u;}
        if(i==19){lo=vec3(.25,2.42,sz-.28);hi=lo+vec3(min(2.5*k,sx-.5),.07,.23);}
        if(i==20){lo=vec3(.4,2.49,sz-.24);hi=lo+vec3(.75,.25,.18);mat=6u;}
        if(i==21 && preset==7u){lo=vec3(sx*.5,.05,.6);hi=lo+vec3(.8,.7,.7);mat=3u;}
        if(i==22 && preset==7u){lo=vec3(sx*.5+.12,.75,.72);hi=lo+vec3(.65,.5,.6);mat=2u;}
        if(i==23){lo=vec3(1.27*k,.93,deskZ+.66*k);hi=lo+vec3(.22,.09,.17);mat=3u;}
        return;
    }
    if(!sparse){
        if(i==0){lo=vec3(bx,.12,bz);hi=lo+vec3(1.55*k,.28,2.2*k);}
        if(i==1){lo=vec3(bx-.02,.4,bz-.015);hi=lo+vec3(1.59*k,.25,2.18*k);mat=4u;}
        if(i==2){lo=vec3(bx,.26,bz+2.12*k);hi=lo+vec3(1.55*k,1.05,.12);}
        if(i==3){lo=vec3(bx+.10*k,.62,bz+1.65*k);hi=lo+vec3(.61*k,.22,.43*k);mat=10u;shape=1u;}
        if(i==4){lo=vec3(bx+.84*k,.62,bz+1.65*k);hi=lo+vec3(.61*k,.22,.43*k);mat=10u;shape=1u;}
    }
    float tx=max(.2,bx-.75*k),tz=max(.35,sz-1.07*k);
    if(i==5){lo=vec3(tx,.03,tz);hi=lo+vec3(.57*k,.55,.65*k);}
    if(i==6){lo=vec3(tx+.265*k,.58,tz+.31*k);hi=lo+vec3(.035,.47,.035);mat=3u;}
    if(i==7){lo=vec3(tx+.07*k,.93,tz+.11*k);hi=lo+vec3(.42*k,.35,.42*k);mat=8u;shape=2u;}
    if(!sparse){
        if(i==8){lo=vec3(.32,.83,1.60);hi=lo+vec3(1.55*k,.11,.73*k);}
        if(i==9){lo=vec3(.36,.03,1.64);hi=lo+vec3(.46*k,.79,.63*k);}
        if(i==10){lo=vec3(1.76*k,.02,2.14*k);hi=lo+vec3(.07,.81,.07);mat=3u;}
        if(i==11){lo=vec3(.99*k,.99,2.08*k);hi=lo+vec3(.64*k,.40,.08);mat=3u;}
        if(i==12){lo=vec3(1.03*k,1.03,2.066*k);hi=lo+vec3(.56*k,.32,.012);mat=7u;}
        if(i==13){lo=vec3(.98*k,.43,1.08);hi=lo+vec3(.56*k,.11,.51*k);mat=4u;}
        if(i==14){lo=vec3(.98*k,.52,1.03);hi=lo+vec3(.56*k,.60,.08);mat=4u;}
        if(i==15){lo=vec3(1.20*k,.04,1.30);hi=lo+vec3(.07,.39,.07);mat=3u;}
    }
    if(i==16){lo=vec3(.27,1.72,sz-.27);hi=lo+vec3(min(2.35*k,sx-.54),.09,.24);}
    if(i==17){lo=vec3(.38,1.81,sz-.23);hi=lo+vec3(min(1.95*k,sx-.76),.35,.19);mat=6u;}
    if(i==18){lo=vec3(.27,2.36,sz-.27);hi=lo+vec3(min(2.35*k,sx-.54),.07,.24);}
    if(i==19){lo=vec3(max(.2,sx-1.4*k),.02,1.55);hi=lo+vec3(.7*k,1.55,.67*k);}
    if(i==20){lo=vec3(sx*.5-1.2,.008,sz*.5-1.6);hi=lo+vec3(2.4,.012,3.2);mat=5u;}
    if(i==21&&(preset==3u||preset==9u)){lo=vec3(max(.3,bx-.12),0,sz*.57);hi=lo+vec3(.08,2.1,max(.2,sz*.39));mat=0u;}
    if(i==21&&preset!=3u&&preset!=9u){lo=vec3(sx*.5-.46,1.51,sz-.105);hi=lo+vec3(.92,1.03,.015);mat=11u;}
    if(i==22){lo=vec3(sx*.5-.42,s.y-.18,sz*.5-.17);hi=lo+vec3(.84,.07,.34);mat=9u;}
    if(i==23){lo=vec3(sx*.5-.52,1.45,sz-.09);hi=lo+vec3(1.04,1.15,.06);}
}

vec3 RoomTurnPoint(vec3 p,vec3 s,uint turn)
{
    return turn==1u?vec3(s.z-p.z,p.y,p.x):turn==2u?vec3(s.x-p.x,p.y,s.z-p.z):
           turn==3u?vec3(p.z,p.y,s.x-p.x):p;
}
vec3 RoomTurnDir(vec3 p,uint turn)
{
    return turn==1u?vec3(-p.z,p.y,p.x):turn==2u?vec3(-p.x,p.y,-p.z):turn==3u?vec3(p.z,p.y,-p.x):p;
}

#include "window_life.glsl"

vec3 RoomMaterial(uint image,uint material,vec3 p,vec3 n,vec3 lo,vec3 hi,float lod)
{
    if(material==11u){ // a small framed abstract landscape, not an interior card
        vec2 uv=(p.xy-lo.xy)/(hi.xy-lo.xy);
        float ridge=.32+.11*sin(uv.x*8.0)+.07*sin(uv.x*17.0);
        vec3 sky=mix(vec3(.43,.39,.29),vec3(.15,.23,.30),uv.y);
        return mix(vec3(.18,.20,.13),sky,smoothstep(ridge-.015,ridge+.015,uv.y));
    }
    uint tile=material==14u?4u:material==15u?3u:material==16u||material==17u?0u:material==8u?0u:material==9u||material==13u?3u:material==10u?4u:material==12u?2u:min(material,7u);
    vec2 uv=abs(n.y)>.6?p.xz:abs(n.x)>.6?p.zy:p.xy;
    uv*=material==4u||material==10u?1.5:material==5u?1.1:.65;
    if(material==6u||material==7u){
        vec3 local=(p-lo)/max(hi-lo,vec3(.001));
        uv=abs(n.z)>.6?local.xy:abs(n.x)>.6?local.zy:local.xz;
        uv=clamp(uv,vec2(.015),vec2(.985));
    }else uv=fract(uv);
    float pad=min(.12,exp2(lod)*.75/256.0);
    uv=(vec2(tile%4u,tile/4u)+clamp(uv,vec2(pad),vec2(1.0-pad)))/vec2(4,2);
    vec3 c=textureLod(sampler2D(texImages[nonuniformEXT(image)],bindlessSamplers[D.sampIdx.w]),uv,lod).rgb;
    if(material==10u)c=mix(vec3(.72,.66,.53),c,.15);
    if(material==8u)c=vec3(.78,.66,.44);
    if(material==9u)c=vec3(.58,.56,.47);
    if(material==12u||material==13u){
        vec3 local=(p-lo)/max(hi-lo,vec3(.001));
        vec2 panel=abs(n.x)>.6?local.zy:local.xy;
        float edge=min(min(panel.x-.12,.88-panel.x),min(panel.y-.10,.90-panel.y));
        float rail=1.0-smoothstep(.008,.020,abs(panel.y-.54));
        c*=mix(.68,1.0,smoothstep(0.0,.025,edge))*(1.0-.30*rail);
    }
    if(material==14u)c=mix(vec3(.17,.13,.10),c,.26)*(0.82+.18*cos(p.y*95.0));
    if(material==15u)c=vec3(.16,.18,.20)*(.35+.65*mix(smoothstep(.22,.42,fract(p.z*24.0)),.5,smoothstep(1.0,3.0,lod)));
    if(material==16u){c=vec3(.60,.56,.46);if(n.y>.6)c*=.18;}
    if(material==17u)c=vec3(.075,.19,.065)*(.75+.25*sin(p.y*29.0+p.x*9.0));
    return max(c,vec3(.015));
}

float RoomShadow(vec3 p,vec3 light,vec3 s,uint preset,int ignore,int count,bool furnished)
{
    vec3 d=light-p;float distance=length(d);d/=max(distance,.001);
    for(int i=0;i<count;++i){
        if(i==ignore)continue;
        if(!furnished&&i<count-5)continue;
        vec3 lo,hi;uint mat,shape;RoomProp(s,preset,i,lo,hi,mat,shape);
        if(hi.x<=lo.x||mat==8u||mat==9u||mat==7u)continue;
        float t;vec3 n;
        if(RoomShape(p,d,lo,hi,shape,t,n)&&t<distance-.04)return .10;
    }
    return 1.0;
}

vec3 WindowGeometry(vec3 surface,uint image,vec2 footprint,vec3 pu,vec3 pv,vec4 rect,vec2 f,
                    vec3 normalW,vec3 toEye,float mask,float fade)
{
    if(D.windowHouseLo.w<.5 || image==0xffffffffu)return surface;
    mat3 world=mat3(D.world);
    if(abs(determinant(world))<1e-8)return surface;
    // Native models can have a mirrored/upside-down local basis. Use WORLD
    // gravity for furniture, preserving the house's horizontal orientation.
    vec3 forward=vec3(world[2].x,0,world[2].z);
    if(dot(forward,forward)<1e-8)forward=vec3(-world[0].z,0,world[0].x);
    if(dot(forward,forward)<1e-8)return surface;
    forward=normalize(forward);
    mat3 roomFrame=mat3(cross(vec3(0,1,0),forward),vec3(0,1,0),forward);
    mat3 toRoom=transpose(roomFrame);
    vec3 origin=toRoom*(vPosW-D.world[3].xyz),direction=toRoom*(-toEye);
    vec3 houseLo=vec3(1e20),houseHi=vec3(-1e20);
    for(uint corner=0u;corner<8u;++corner){
        vec3 model=mix(D.windowHouseLo.xyz,D.windowHouseHi.xyz,
            vec3((corner&1u)!=0u?1:0,(corner&2u)!=0u?1:0,(corner&4u)!=0u?1:0));
        vec3 p=toRoom*(world*model);houseLo=min(houseLo,p);houseHi=max(houseHi,p);
    }
    bool global=D.windows.w>0.0;
    bool corporate=global&&(uint(D.windowHouseHi.w)&1024u)!=0u;
    if(global){
        vec3 normal=toRoom*normalW;
        bool planar=any(lessThan(houseHi-houseLo,vec3(.1)));
        for(int axis=0;axis<3;++axis)if(houseHi[axis]-houseLo[axis]<.1){
            float plane=(houseHi[axis]+houseLo[axis])*.5;
            float depth=axis==1?3.6:7.5;
            houseLo[axis]=plane-(normal[axis]>=0?depth:0);
            houseHi[axis]=plane+(normal[axis]>=0?0:depth);
        }
        if(planar){
            for(int axis=0;axis<3;axis+=2)if(houseHi[axis]-houseLo[axis]<6.0){
                float centre=(houseHi[axis]+houseLo[axis])*.5;
                houseLo[axis]=centre-3.0;houseHi[axis]=centre+3.0;
            }
            if(houseHi.y-houseLo.y<3.6){houseLo.y-=.8;houseHi.y=houseLo.y+3.6;}
        }
    }
    vec3 houseSize=houseHi-houseLo;
    if(any(lessThan(houseSize,vec3(.1))))return surface;
    // One apartment across the connected house footprint. No arbitrary X/Z
    // cell can put a wall or another light state between adjacent panes.
    bool apartments=global&&(uint(D.windowHouseHi.w)&1u)!=0u;
    // Dense facades share a metric apartment grid anchored to the full model,
    // never to individual panes. Corner rays use the same cell and furniture.
    vec3 cells=apartments?max(round(houseSize/vec3(7.5,3.6,7.5)),vec3(1)):
                         vec3(1,max(round(houseSize.y/5.0),1.0),1);
    vec3 roomSize=houseSize/cells;
    vec3 enter=origin+direction*.008;
    vec3 cell=clamp(floor((enter-houseLo)/roomSize),vec3(0),cells-1.0);
    // Select a floor below the sill. A wide/tall aperture must not be normalized
    // into a tiny full room or arbitrarily cut by an estimated floor boundary.
    vec3 bottomA=toRoom*(vPosW+pv*(rect.y-f.y)-D.world[3].xyz);
    vec3 bottomB=toRoom*(vPosW+pv*(rect.w-f.y)-D.world[3].xyz);
    float bottom=min(bottomA.y,bottomB.y),top=max(bottomA.y,bottomB.y);
    cell.y=clamp(floor((bottom-houseLo.y-.25)/roomSize.y),0.0,cells.y-1.0);
    vec3 roomLo=houseLo+cell*roomSize;
    // Roofs/basements can offset a regular floor grid. Never place an estimated
    // ceiling across the actual opening; align below its sill when necessary.
    if(top>roomLo.y+roomSize.y-.15){
        roomLo.y=max(houseLo.y,floor((bottom-.65)*2.0+.5)*.5);
        roomSize.y=min(max(roomSize.y,top-roomLo.y+.25),houseHi.y-roomLo.y);
    }
    // The cell's volume/content never changes with aperture size or material.
    // A real partition/ceiling behind part of an opening remains an occluder.
    vec3 centreW=roomFrame*(roomLo+roomSize*.5)+D.world[3].xyz;
    uvec2 identity=uvec2(ivec2(floor(centreW.xz*4.0)));
    uint hash=SurfaceHash(identity^uvec2(uint(int(floor(centreW.y*4.0))),0x591be2a7u));
    uint content=SurfaceHash(uvec2(hash,hash^0x6e624eb7u));
    float variety=float(hash&255u)/255.0,use=float(content&255u)/255.0;
    float activity=RoomActivity(hash);
    gWindowHabit=float((content>>4u)&1023u)/1023.0;
    bool strong=D.windows.x>1.5;
    uint category=use<.65?0u:use<.90?1u:2u;
    if(global&&(uint(D.windowHouseHi.w)&16u)!=0u)category=1u;
    float occupied=step(strong?.45:.50,variety),clear=step(strong?.72:.80,variety),hero=step(strong?.92:.96,variety);
    if(global&&(uint(D.windowHouseHi.w)&4u)!=0u){
        // Closed coverings retain their actual texture. Only the reviewed dark
        // gaps receive a soft backlight; no room picture replaces the shutter.
        float night=clamp(FL.phase.w,0.0,1.0);
        float lit=smoothstep(.48,.84,variety);
        vec3 tint=category==1u?vec3(.45,.65,1.0):vec3(1.0,.65,.32);
        vec3 leak=tint*lit*(strong?.20:.10)*mix(.06,1.0,night)*activity*mask*fade;
        gWindowGlow=leak*.10*night;
        return surface+leak;
    }
    if(global&&(uint(D.windowHouseHi.w)&2048u)!=0u){
        // Keep the original blind on the covered part of the aperture. Lift
        // it in WORLD up, even on native mirrored/upside-down UV layouts.
        // All adjoining panes share the room's deterministic opening height.
        float apertureHeight=max(top-bottom,.001);
        float lift=strong?mix(.30,.56,variety):mix(.18,.34,variety);
        float height=(toRoom*(vPosW-D.world[3].xyz)).y;
        float edge=max(.012,apertureHeight*.015);
        float opening=1.0-smoothstep(bottom+apertureHeight*lift-edge,
                                     bottom+apertureHeight*lift+edge,height);
        mask*=opening;
        if(mask<=0.0)return surface;
    }
    if(gWindowDetail==0u){
        // Sub-pixel furnishings cannot be resolved. Keep the same apartment's
        // stable light state, without tracing primitives or shadow rays.
        float night=clamp(FL.phase.w,0.0,1.0);
        vec3 light=corporate?vec3(.26,.29,.34):category==1u?vec3(.30,.40,.55):vec3(.55,.30,.12);
        float level=(strong?.30:.17)+clear*.55+hero*.35;
        vec3 colour=light*level*mix(.18,1.0,night)*activity;
        float amount=mix(strong?.58:.32,strong?.94:.75,night)*mask*fade;
        gWindowGlow=colour*.08*night*amount;
        return mix(surface,colour,amount);
    }
    // Quality changes light/detail, never swaps the apartment's base layout.
    bool sceneHero=variety>=.92;
    uint preset=category==0u?(sceneHero?5u:(content>>16u)%5u):category==1u?(sceneHero?8u:6u+((content>>16u)&1u)):9u;
    // Every supported opening belongs to an inhabited dwelling. Quiet rooms
    // remain dim; random occupancy no longer removes their contents and light.
    bool furnished=true;
    uint turn=(content>>24u)&3u;
    // Prefer an internal partition, with a centred door on the selected wall.
    // A one-room house uses a side wall, leaving the lamp and bed clear of it.
    uint doorWall=cell.x<cells.x-1.0?3u:cell.x>0.0?1u:cell.z<cells.z-1.0?0u:cell.z>0.0?2u:1u;
    gWindowDoorWall=(doorWall+turn)&3u;
    origin=RoomTurnPoint(enter-roomLo,roomSize,turn);direction=RoomTurnDir(direction,turn);
    if((turn&1u)!=0u)roomSize=roomSize.zyx;
    // A real partition and open door reserve a hallway inside the existing
    // house footprint. No adjacent space extends beyond these room bounds.
    gWindowDoorOpen=((content>>9u)&3u)==0u && roomSize.z>4.8 && roomSize.x>2.3;
    gWindowDoorInset=gWindowDoorOpen?1.45:0.0;
    if(gWindowDoorOpen)gWindowDoorWall=0u;
    float first,last;vec3 n;
    if(!RoomBox(origin,direction,vec3(0),roomSize,first,last,n)||last<=0.0)return surface;
    bool outside=first>0.0;
    float hit=outside?first:last;vec3 at=origin+direction*hit;
    // Exit normal points into the room. Furniture uses outward surface normals.
    vec3 wallDist=abs(at-roomSize*.5)-roomSize*.5;
    if(!outside)n=wallDist.x>=max(wallDist.y,wallDist.z)?vec3(at.x<roomSize.x*.5?1:-1,0,0):
      wallDist.y>=wallDist.z?vec3(0,at.y<roomSize.y*.5?1:-1,0):vec3(0,0,at.z<roomSize.z*.5?1:-1);
    uint material=abs(n.y)>.5?(n.y>0?2u:0u):(category==1u?1u:0u);
    if(abs(n.y)<.5 && at.y<.14)material=2u;
    vec3 objectLo=vec3(0),objectHi=roomSize;int object=-1;
    int count=strong&&gWindowDetail==3u?32:24;
    for(int i=0;i<count;++i){
        if(!furnished&&i<count-5)continue;
        vec3 lo,hi;uint mat,shape;RoomProp(roomSize,preset,i,lo,hi,mat,shape);
        if(hi.x<=lo.x)continue;
        float t;vec3 face;
        if(RoomShape(origin,direction,lo,hi,shape,t,face)&&t<hit){
            hit=t;n=face;material=mat;object=i;objectLo=lo;objectHi=hi;
        }
    }
    int lifeCount=gWindowDetail==1u?6:strong&&gWindowDetail==3u?15:10;
    for(int i=0;i<lifeCount;++i){
        vec3 lo,hi;uint mat,shape;RoomLifeProp(roomSize,preset,i,lo,hi,mat,shape);
        if(hi.x<=lo.x)continue;
        float t;vec3 face;
        if(RoomShape(origin,direction,lo,hi,shape,t,face)&&t<hit){
            hit=t;n=face;material=mat;object=64+i;objectLo=lo;objectHi=hi;
        }
    }
    // Curtains sit behind the glass, attached to this actual aperture. Their
    // style/opening belongs to the apartment; they never scale the room.
    if(((content>>5u)&7u)<3u && (!global||(uint(D.windowHouseHi.w)&(4u|2048u))==0u)){
        vec3 tangent=normalize(cross(vec3(0,1,0),normalW));
        vec3 centre=vPosW+pu*((rect.x+rect.z)*.5-f.x)+pv*((rect.y+rect.w)*.5-f.y);
        float width=max(abs(dot(pu,tangent))*(rect.z-rect.x)+abs(dot(pv,tangent))*(rect.w-rect.y),.05);
        float curtainT=.16/max(dot(normalW,toEye),.12);
        vec3 atW=vPosW-toEye*curtainT;
        float u=dot(atW-centre,tangent)/width+.5;
        float v=(atW.y-D.world[3].y-bottom)/max(top-bottom,.05);
        float left=.10+.18*gWindowHabit,right=.09+.16*fract(gWindowHabit*7.13);
        if(((content>>15u)&7u)==0u)left+=.27;
        if(v>.025&&v<.97&&u>0.0&&u<1.0&&(u<left||u>1.0-right)&&curtainT-.008<hit){
            hit=curtainT-.008;material=14u;object=90;objectLo=vec3(0);objectHi=roomSize;
            vec3 fold=normalW+tangent*((gWindowDetail>=2u?.22:.055)*sin(u*width*38.0));n=toRoom*normalize(fold);n=RoomTurnDir(n,turn);
        }
    }
    at=origin+direction*hit;
    if(object>=0 && material!=7u && material!=8u && material!=9u && material!=11u && material!=14u && material!=15u && material!=17u){
        vec3 bevel=min(vec3(material==4u?.05:.022),(objectHi-objectLo)*.15);
        vec3 rounded=at-clamp(at,objectLo+bevel,objectHi-bevel);
        if(dot(rounded,rounded)>1e-8)n=normalize(rounded);
    }
    float metresPerPixel=max(length(pu)*footprint.x*(rect.z-rect.x),length(pv)*footprint.y*(rect.w-rect.y));
    float lod=clamp(log2(max(metresPerPixel*96.0,1.0)),0.0,4.0);
    vec3 albedo=RoomMaterial(image,material,at,n,objectLo,objectHi,lod);
    if(corporate){
        // Reuse the small material atlas, with a separate graphite/steel
        // palette. Office presets contain desks, cabinets and terminals.
        float value=dot(albedo,vec3(.2126,.7152,.0722));
        if(material==0u||material==1u)albedo=mix(vec3(.075,.085,.10),vec3(value)*vec3(.40,.44,.50),.70);
        else if(material==2u||material==12u)albedo=vec3(value)*vec3(.28,.32,.38);
        else if(material==4u||material==5u||material==10u||material==14u)albedo=vec3(value)*vec3(.32,.37,.44);
        else if(material==8u||material==9u)albedo=vec3(.82,.87,.94);
        else if(material==11u)albedo=vec3(.045,.065,.09);
    }
    if(object==5||object==9||((preset>=6u&&preset<=8u)&&object==1)){
        vec3 uv=(at-objectLo)/max(objectHi-objectLo,vec3(.001));
        float drawer=1.0-smoothstep(.012,.028,abs(fract(uv.y*3.0)-.5));
        if(abs(n.z)>.6)albedo*=1.0-.45*drawer;
    }
    float warmth=float((content>>8u)&255u)/255.0;
    vec3 lamp=corporate?vec3(.92,.96,1.0):category==1u?mix(vec3(.82,.90,1),vec3(.71,.84,1),warmth):mix(vec3(1,.61,.29),vec3(1,.78,.49),warmth);
    float night=clamp(FL.phase.w,0.0,1.0);
    float illumination=(strong?.8:.6)+clear*(strong?1.4:1.1)+hero*(strong?1.2:.8);
    illumination*=mix(.45,1.0,night)*(category==1u?.82:1.0)*activity;
    float k=clamp(min(roomSize.x,roomSize.z)/4.5,.65,1.05);
    float bx=max(.25,roomSize.x-2.4*k),tx=max(.2,bx-.75*k),tz=max(.35,roomSize.z-gWindowDoorInset-1.07*k);
    vec3 light=category==1u||preset==2u?vec3(roomSize.x*.5,roomSize.y-.35,roomSize.z*.5):vec3(tx+.28*k,1.12,tz+.32*k);
    vec3 toLight=light-at;float r2=dot(toLight,toLight);vec3 L=normalize(toLight);
    float visible=1.0;
    if((!global||gWindowDetail>=2u)&&illumination>0.0&&dot(n,L)>0.0){
        visible=RoomShadow(at+n*.008,light,roomSize,preset,object,count,furnished);
        if(strong&&gWindowDetail==3u){
            visible+=RoomShadow(at+n*.008,light+vec3(.18,.08,-.10),roomSize,preset,object,count,furnished);
            visible+=RoomShadow(at+n*.008,light+vec3(-.16,.05,.13),roomSize,preset,object,count,furnished);
            visible+=RoomShadow(at+n*.008,light+vec3(.08,-.09,.18),roomSize,preset,object,count,furnished);
            visible*=.25;
        }
    }
    // Keep the bright, warm lamp pool requested during live review.
    float direct=max(dot(n,L),0.0)*3.5/(.7+r2*.5)*visible;
    vec3 fillPoint=vec3(roomSize.x*.5,roomSize.y-.4,roomSize.z*.45);
    vec3 fill=fillPoint-at;
    float fillLight=max(dot(n,normalize(fill)),0.0)*.50/(1.0+dot(fill,fill)*.35);
    float corner=min(min(at.x,roomSize.x-at.x),min(at.z,roomSize.z-at.z));
    float ao=.64+.36*smoothstep(0.0,.35,max(corner,0.0));
    vec3 quietFill=(corporate?vec3(.055,.067,.085):category==1u?vec3(.13,.16,.20):vec3(.18,.15,.12))*(strong?1.0:.70);
    vec3 colour=albedo*(mix(vec3(.075,.085,.10),quietFill,night)*ao+
                       lamp*illumination*(direct+fillLight));
    {
        // Indirect light belongs to the apartment, not its aperture. Connected
        // large/small panes therefore see the same softly illuminated room.
        // Deterministic variation leaves quiet homes dim, without flicker.
        float lived=smoothstep(.25,.82,variety);
        float bounce=(strong?.38:.20)*mix(.35,1.0,lived)*mix(.12,1.0,night)*(corporate?.40:1.0)*mix(.30,1.0,activity);
        float directionality=.70+.30*max(n.y,0.0);
        colour+=albedo*lamp*bounce*ao*directionality;
    }
    // Broad ceiling bounce and a near-window fixture remain readable from
    // ordinary street angles. Hall light is a second source behind the door.
    vec3 nearLight=vec3(roomSize.x*.46,roomSize.y-.20,max(.42,(roomSize.z-gWindowDoorInset)*.20));
    vec3 nearRay=nearLight-at;
    float nearPool=max(dot(n,normalize(nearRay)),0.0)/(1.0+dot(nearRay,nearRay)*.16);
    float ceilingPool=abs(n.y)>.6&&n.y<0.0?(.10+.16*exp(-length(at.xz-nearLight.xz)*.28)):0.0;
    colour+=albedo*lamp*(nearPool*.30+ceilingPool)*(strong?1.0:.65)*illumination;
    if(gWindowDoorOpen){
        vec3 hallLight=vec3(roomSize.x*.5,roomSize.y-.20,roomSize.z-.72);
        vec3 hallRay=hallLight-at;
        float hallVisible=gWindowDetail>=2u?RoomShadow(at+n*.008,hallLight,roomSize,preset,object,count,true):.5;
        float hall=max(dot(n,normalize(hallRay)),0.0)*hallVisible/(.8+dot(hallRay,hallRay)*.35);
        colour+=albedo*mix(lamp,vec3(.78,.85,1),corporate?.25:.12)*hall*(.25+.65*activity)*mix(.3,1.0,night);
    }
    if(material==14u)colour+=albedo*lamp*illumination*.14;
    vec3 H=normalize(L+normalize(-direction));
    float spec=pow(max(dot(n,H),0.0),material==3u||material==13u?48.0:18.0)*(material==3u||material==13u?.32:material==2u||material==12u?.07:.012);
    colour+=lamp*spec*illumination*visible/(1.0+r2*.45);
    vec3 emission=vec3(0);
    if(material==7u)emission=albedo*vec3(.24,.43,.60)*illumination*.40;
    if(material==8u)emission=lamp*illumination*.70;
    if(material==9u)emission=lamp*illumination*(category==1u?.85:.22);
    colour+=emission;
    // Light on the inner aperture edges/sill only; never an exterior wall pass.
    vec2 apertureUV=clamp((f-rect.xy)/max(rect.zw-rect.xy,vec2(.001)),0.0,1.0);
    float rim=exp(-apertureUV.y*28.0)*.10+(exp(-apertureUV.x*38.0)+exp(-(1.0-apertureUV.x)*38.0))*.025;
    colour+=lamp*illumination*rim*(strong?1.0:.6);
    float cosV=max(dot(normalW,toEye),0.0),fresnel=.07+.62*pow(1.0-cosV,5.0);
    float transmission=mix(strong?.58:.32,strong?(global?.97:.94):(global?.86:.77),night)*(1.0-fresnel)*mask*fade*(global?1.0:mix(.63,1.0,occupied));
    gWindowGlow=(emission*.38+colour*.075)*(strong?1:.5)*night*mask*fade;
    vec3 sheen=mix(vec3(.10,.15,.21),vec3(.035,.055,.085),night)*(.06+.30*pow(1.0-cosV,3.0))*(strong?1:.65);
    return mix(surface,colour,transmission)+gPbrSpec*transmission+sheen*mask*fade;
}
