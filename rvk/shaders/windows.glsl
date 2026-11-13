// Reviewed window masks and glass blend. Room geometry is shared by the whole building.
vec3 gWindowGlow=vec3(0);
struct GlobalWindowPane {vec4 rect;vec4 aperture;vec4 params;};
layout(set=0,binding=13,std430) readonly buffer GlobalWindowPanes {GlobalWindowPane panes[];} gWindowPanes;
vec4 gWindowPattern=vec4(0,0,3,1);
uint gWindowDetail=3u;
#include "window_geometry.glsl"

vec3 WindowInterior(vec3 surface)
{
    vec2 uv = D.windows.z > .5 ? vTex1.xy : vTex0.xy;
    uint addressFlags=uint(D.windowHouseHi.w);
    bool globalAddress=D.windows.w>0.0;
    bool clampedOut=globalAddress&&(((addressFlags&32u)!=0u&&(uv.x<=0.0||uv.x>=1.0))||
                                   ((addressFlags&64u)!=0u&&(uv.y<=0.0||uv.y>=1.0)));
    if(globalAddress&&(addressFlags&128u)!=0u)uv.x=1-abs(mod(uv.x,2.0)-1);
    if(globalAddress&&(addressFlags&256u)!=0u)uv.y=1-abs(mod(uv.y,2.0)-1);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec3 px = dFdx(vPosW), py = dFdy(vPosW);
    if(clampedOut)return surface;
    vec2 f = fract(uv);
    uint packed=uint(D.windows.y),layoutId=packed&7u,atlas=packed>>3u;
    float paneLayout=float(layoutId);
    vec4 rect = vec4(0.0);
    uint pane = 0u;
    vec4 glassRect=vec4(0);float stencil=1.0;
    uint globalProfile=uint(D.windows.w);
    if(globalProfile!=0u){
        uint first=(globalProfile&65535u)-1u,count=((globalProfile>>16u)&255u)+1u;
        if(first+count>uint(gWindowPanes.panes.length()))return surface;
        for(uint i=0u;i<count;++i){
            GlobalWindowPane p=gWindowPanes.panes[first+i];
            if(all(greaterThan(f,p.rect.xy))&&all(lessThan(f,p.rect.zw))){
                rect=p.aperture;glassRect=p.rect;gWindowPattern=p.params;pane=i;break;
            }
        }
    }else{
    // Insets protect jambs, sills, blind housings and the arched cap.
    if (paneLayout < 1.5) {
        const vec4 panes[4] = vec4[4](vec4(.078,.205,.240,.690), vec4(.349,.205,.496,.690),
                                    vec4(.590,.205,.734,.690), vec4(.827,.205,.973,.690));
        for (uint i=0u; i<4u; ++i) {
            vec4 r=panes[i];
            if (all(greaterThan(f,r.xy)) && all(lessThan(f,r.zw))) {rect=r;pane=i;}
        }
    } else if (paneLayout < 2.5) {
        rect = f.x < .5 ? vec4(.145,.320,.353,.637) : vec4(.647,.320,.849,.637);
        pane = f.x < .5 ? 0u : 1u;
    } else if(paneLayout<3.5) rect=vec4(.436,.272,.570,.718);
    else if(paneLayout<4.5){
        const vec4 panes[4]=vec4[4](vec4(.05,.25,.21,.50),vec4(.30,.25,.46,.50),
                                  vec4(.55,.25,.71,.50),vec4(.80,.25,.96,.50));
        for(uint i=0u;i<4u;++i){vec4 r=panes[i];if(all(greaterThan(f,r.xy))&&all(lessThan(f,r.zw))){rect=r;pane=i;}}
    } else if(paneLayout<5.5)rect=vec4(.15,.29,.86,.765);
    else if(paneLayout<6.5){
        rect=f.x<.6?vec4(.377,.12,.434,.68):vec4(.884,.12,.941,.68);pane=f.x<.6?0u:1u;
    } else rect=vec4(.13,.34,.90,.52);
    }
    vec2 span = rect.zw-rect.xy;
    if (any(lessThanEqual(span,vec2(0.0)))) return surface;
    vec2 q = (f-rect.xy)/span;
    if (any(lessThanEqual(q,vec2(0.0))) || any(greaterThanEqual(q,vec2(1.0)))) return surface;
    vec2 foot = max(abs(dx),abs(dy))/span;
    float edge = min(min(q.x,1.0-q.x),min(q.y,1.0-q.y));
    float mask = smoothstep(0.0,max(.018,1.5*max(foot.x,foot.y)),edge);
    if(globalProfile!=0u){
        vec2 local=(f-glassRect.xy)/(glassRect.zw-glassRect.xy);
        vec2 gf=max(abs(dx),abs(dy))/(glassRect.zw-glassRect.xy);
        float ge=min(min(local.x,1-local.x),min(local.y,1-local.y));
        stencil=smoothstep(0.0,max(.01,1.2*max(gf.x,gf.y)),ge);
        uint shape=uint(gWindowPattern.x);
        if(shape==1u)stencil*=smoothstep(0.0,max(.025,2*max(gf.x,gf.y)),1-length((local-.5)*2));
        if(shape==2u&&local.y<.22){
            float arch=1-length(vec2((local.x-.5)*2,(local.y-.22)/.22));
            stencil*=smoothstep(0.0,max(.025,2*max(gf.x,gf.y)),arch);
        }
        if(shape==3u)stencil*=smoothstep(.015,max(.025,2*max(gf.x,gf.y)),local.y-.10-local.x*.86);
        if(shape==4u)stencil*=smoothstep(.015,max(.025,2*max(gf.x,gf.y)),local.y-abs(local.x-.5)*1.8);
        uint glazing=uint(gWindowPattern.y);
        if(glazing!=0u){
            // Only inside a reviewed window stencil. Preserve leadwork/bars in
            // stained glass; this never classifies a material as a window.
            vec3 raw=D.windows.z>.5?texture(TEX1,vTex1.xy).rgb:texture(TEX0,vTex0.xy).rgb;
            float peak=max(raw.r,max(raw.g,raw.b));
            if(glazing==1u)stencil*=1-smoothstep(.13,.23,peak);
            if(glazing==2u)stencil*=smoothstep(.15,.28,peak);
            if(glazing==3u)stencil*=smoothstep(.015,.065,raw.b-max(raw.r,raw.g));
        }
        mask*=stencil;
    }
    if(globalProfile==0u&&paneLayout>2.5 && paneLayout<3.5 && f.y<.330){
        float arch=1.0-length((f-vec2(.503,.330))/vec2(.067,.058));
        if(arch<=0.0)return surface;
        mask*=smoothstep(0.0,max(.025,2.0*max(foot.x,foot.y)),arch);
    }
    // No tiny shimmering rooms or contribution at far range / grazing angles.
    bool global=globalProfile!=0u;
    float pixelFoot=max(foot.x,foot.y);
    float fade = (1.0-smoothstep(global?.5:.10,global?1.5:.30,pixelFoot)) *
                 (1.0-smoothstep(global?650.0:110.0,global?1000.0:170.0,length(C.eyePos.xyz-vPosW)));
    gWindowDetail=pixelFoot>1.0/12.0?0u:pixelFoot>1.0/32.0?1u:pixelFoot>1.0/90.0?2u:3u;
    float det = dx.x*dy.y-dx.y*dy.x;
    if (abs(det)<1e-12 || fade<=0.0) return surface;
    vec3 pu=(px*dy.y-py*dx.y)/det, pv=(py*dx.x-px*dy.x)/det;
    vec3 normal=cross(pu,pv);
    float nl=length(normal);
    if (nl<1e-6 || min(length(pu),length(pv))<1e-6) return surface;
    normal/=nl;
    vec3 toEye=normalize(C.eyePos.xyz-vPosW);
    if(dot(normal,toEye)<0.0)normal=-normal;
    float cosV=dot(normal,toEye);
    fade*=smoothstep(.12,.35,cosV)*(1.0-smoothstep(.35,.65,abs(normal.y)));
    if (fade<=0.0) return surface;
    return WindowGeometry(surface,atlas!=0u?atlas-1u:0xffffffffu,foot,pu,pv,rect,f,normal,toEye,mask,fade);
}
