#version 450
// World-anchored, terrain-classified emitters. No camera-space snow overlay.
#include "rain_common.glsl"
layout(set=0,binding=3) uniform AmbientOrigins { vec4 points[128]; } A;
layout(location=0) out vec2 vCorner;
layout(location=1) out vec3 vColour;
layout(location=2) out float vAlpha;
layout(location=3) out vec4 vClip;
layout(location=4) out vec4 vPrevClip;
layout(location=5) out float vGlow;
layout(location=6) flat out float vShape;
vec3 Position(vec3 origin,float t,float seed,float kind,float variant) {
    // Different small paths rather than a synchronized sheet of sine waves.
    float speed=(.28+.55*fract(seed*2.731))*R.curtains.y, phase=t*speed;
    vec3 spread=vec3(sin(seed*7.17)*3.8,fract(seed*3.17)*1.5,cos(seed*5.37)*3.8);
    if(kind<.5 || kind>8.5) { // dust/pollen, with distinct drift and curl paths
        vec3 drift=vec3(sin(phase*.24+seed)*2.3,.8+.7*sin(phase*.31+seed)*R.curtains.z,cos(phase*.19+seed)*1.8);
        vec3 curl=vec3(sin(phase*.31+seed)*1.4+cos(phase*.11+seed)*.8,
                       1.1+.65*sin(phase*.21+seed*2.),cos(phase*.27+seed)*1.4);
        vec3 day=mix(drift,curl,step(.55,variant));
        vec3 insect=vec3(sin(phase*.83+seed)*1.1+.22*sin(phase*2.3+seed),
                         1.2+.8*sin(phase*.47+seed),cos(phase*.71+seed)*1.3);
        return origin+spread+(kind>8.5 ? mix(day,insect,R.time.z*.18):day);
    }
    if(kind>5.5 && kind<8.5) { // snow/ash settle, embers rise, all loop without teleports
        float rise=kind<7.5 && kind>6.5 ? 1.:-1.;
        return origin+spread+vec3(sin(phase*.4+seed)*1.5,2.+rise*1.7*sin(phase*.21+seed),cos(phase*.3+seed));
    }
    return origin+spread+vec3(sin(phase+seed)*(.5+variant),.5+1.2*(1.+sin(phase*.43+seed*2.)),cos(phase*.71+seed)*(.8+variant));
}
void main() {
    vShape=0.;
    uint id=uint(gl_VertexIndex)/6u, vertex=uint(gl_VertexIndex)%6u;
    uint perAnchor=max(uint(R.fall.w+.5),1u);
    vec4 anchor=A.points[id/perAnchor];
    uint random=RainHash((id%perAnchor)*7919u+floatBitsToUint(anchor.x)+floatBitsToUint(anchor.z));
    float seed=RainRand(random)*6.2831853;
    float variant=RainRand(random);
    vec3 p=Position(anchor.xyz,R.time.x,seed,anchor.w,variant),before=Position(anchor.xyz,R.time.y,seed,anchor.w,variant);
    if(anchor.w>9.5) {
        p=anchor.xyz;before=A.points[64u+id].xyz;
        float phase=mod(R.time.x,1.6);
        float flash=phase<.09 || (phase>.19 && phase<.28) ? 1.:.06;
        vColour=anchor.w<10.5 ? vec3(3.6,.04,.015):anchor.w<11.5 ? vec3(.03,3.,.12):vec3(3.,3.,2.8)*flash;
        vGlow=clamp(FL.terrainFar.z,0.,1.);vAlpha=.85;
        vCorner=vec2(0.);
        const vec2 corners[6]=vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
        vCorner=corners[vertex];
        float distanceToEye=distance(p,R.camera.xyz);
        vAlpha*=1.-smoothstep(1800.,2400.,distanceToEye);
        float radius=max(.09,distanceToEye*R.viewport.w*1.3);
        vec3 right=vec3(R.view[0][0],R.view[1][0],R.view[2][0]);
        vec3 up=vec3(R.view[0][1],R.view[1][1],R.view[2][1]);
        vec3 offset=(right*vCorner.x+up*vCorner.y)*radius;
        vClip=R.viewProj*vec4(p+offset,1.);vPrevClip=R.prevViewProj*vec4(before+offset,1.);
        // Ships themselves use MAT_SKY far-plane clamping; their attachments must match.
        vClip.z=min(vClip.z,vClip.w*.999998);vPrevClip.z=min(vPrevClip.z,vPrevClip.w*.999998);gl_Position=vClip;
        if(R.camera.w>.5)gl_Position.xy+=FL.taa.xy*gl_Position.w;
        return;
    }
    bool spores=anchor.w>1.5 && anchor.w<2.5;
    bool spirit=anchor.w>4.5 && anchor.w<5.5;
    bool snow=anchor.w>5.5 && anchor.w<6.5;
    bool embers=anchor.w>6.5 && anchor.w<7.5;
    bool ash=anchor.w>7.5 && anchor.w<8.5;
    bool woodland=anchor.w>8.5 && anchor.w<9.5;
    // Reuse the same bounded emitters: rounded motes, drifting seeds, flakes
    // and occasional translucent wisps rather than one repeated round sprite.
    vShape=snow || ash ? 2.:woodland ? (variant<.58 ? 1.:0.):
           spores ? (variant<.35 ? 2.:variant>.86 ? 3.:0.):
           anchor.w<.5 ? (variant<.36 ? 1.:variant>.88 ? 3.:0.):0.;
    if(snow || ash) {
        // A slow settling flutter, bounded and world anchored.
        p.y-=.35*sin(R.time.x*.35+seed);
        before.y-=.35*sin(R.time.y*.35+seed);
    }
    if(embers){p.y+=.6*sin(R.time.x*.7+seed);before.y+=.6*sin(R.time.y*.7+seed);}
    bool fleck=spores && (id%2u)==1u;
    bool firefly=anchor.w>3.5 && anchor.w<4.5;
    float vegetation=woodland ? 1.:0.;
    float night=R.time.z;
    vGlow=fleck || snow || ash ? 0.:spores || spirit || embers || firefly ? 1.:0.;
    float pulse=.35+.65*pow(.5+.5*sin(R.time.x*(1.3+seed*.1)+seed),2.);
    float distanceToEye=distance(p,R.camera.xyz);
    float opacity=(1.-smoothstep(36.,52.,distanceToEye))*smoothstep(.7,2.,distanceToEye);
    float density=R.curtains.x;
    if(firefly)density*=.165; // Density 2 has the former 1.5 firefly population.
    opacity*=1.-smoothstep(density-.02,density+.02,RainRand(random));
    // Automatic SL daylight is pollen, not luminous green confetti.
    if(woodland)vGlow=night*step(.925,variant);
    opacity*=mix(woodland ? .54:.43,pulse, vGlow)*R.colour.w;
    opacity*=.6+.4*pow(.5+.5*sin(R.time.x*.31+seed*3.),2.);
    if(anchor.w<.5 || snow || ash)opacity*=1.15;
    vec3 tint=mix(vec3(.56,.43,.27),vec3(.72,.78,.52),vegetation);
    if(fleck)tint=vec3(.1,.16,.08);
    if(snow)tint=vec3(.83,.91,1.);
    if(ash)tint=vec3(.42,.43,.47);
    if(woodland)tint=vec3(.78,.83,.66);
    // Sun/sky-lit particles need daylight contrast against dark foliage.
    // This is their own diffuse scattering, not a scene exposure adjustment.
    vec3 daylight=max(R.ambient.rgb,vec3(.70,.75,.80));
    vec3 lit=tint*mix(daylight,max(R.ambient.rgb,vec3(.16)),night);
    if(anchor.w<.5 || snow || ash)lit*=1.18;
    lit*=mix(.84,1.16,variant);
    vColour=mix(lit,(spores ? vec3(.7,1.,.75)*1.35:vec3(.65,.95,.28)*1.7),vGlow);
    if(spirit)vColour=vec3(.32,.65,1.)*1.35;
    if(embers)vColour=vec3(1.,.24,.035)*1.7;
    // Regional ambience only: no change to scene exposure or lighting.
    vColour*=R.colour.rgb;
    vAlpha=clamp(opacity,0.,.75);
    const vec2 corners[6]=vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vCorner=corners[vertex];
    float radius=spores ? (fleck ? .025:.018):mix(.030,.045,vGlow);
    if(woodland)radius=mix(.038,.027,vGlow);
    if(snow || ash)radius=.024;
    if(anchor.w<.5)radius=.036;
    if(spirit || embers)radius=.012;
    radius*=mix(.78,1.24,variant);
    // A small subpixel mote fades rather than turning into a large distant star.
    radius=max(radius,distanceToEye*R.viewport.w*.65);
    vec3 right=vec3(R.view[0][0],R.view[1][0],R.view[2][0]);
    vec3 up=vec3(R.view[0][1],R.view[1][1],R.view[2][1]);
    vec2 face=vCorner,previousFace=vCorner;
    if(vShape>0.5) {
        float angle=seed+R.time.x*(.15+.25*variant),s=sin(angle),c=cos(angle);
        face=mat2(c,-s,s,c)*face;
        angle=seed+R.time.y*(.15+.25*variant);s=sin(angle);c=cos(angle);
        previousFace=mat2(c,-s,s,c)*previousFace;
    }
    vec3 offset=(right*face.x+up*face.y)*radius;
    vec3 previousOffset=(right*previousFace.x+up*previousFace.y)*radius;
    vClip=R.viewProj*vec4(p+offset,1.);
    vPrevClip=R.prevViewProj*vec4(before+previousOffset,1.);
    gl_Position=vClip;
    if(R.camera.w>.5)gl_Position.xy+=FL.taa.xy*gl_Position.w;
}
