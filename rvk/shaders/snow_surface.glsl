// A shallow powder shell, anchored in world space. Both material passes and
// both sides of a tessellation edge evaluate exactly the same height.
float PowderCell(vec2 cell,vec2 local)
{
    uint codes=floatBitsToUint(D.terrain.y)-1u;
    ivec2 size=textureSize(sampler2D(texImages[codes],bindlessSamplers[D.sampIdx.w]),0);
    uvec4 data=uvec4(texelFetch(sampler2D(texImages[codes],bindlessSamplers[D.sampIdx.w]),
        clamp(ivec2(cell),ivec2(0),size-1),0)*255.+.5);
    if((data.a&8u)!=0u)return 0.;
    uint image=data.r|(data.g<<8),turn=data.b&3u;
    vec2 uv=turn==0u ? vec2(local.x,1.-local.y):turn==1u ? vec2(1.-local.y,1.-local.x):turn==2u ? vec2(1.-local.x,local.y):local.yx;
    float lod=min(2.,float(textureQueryLevels(sampler2D(texImages[nonuniformEXT(image)],bindlessSamplers[D.sampIdx.w]))-1));
    vec2 levelSize=vec2(textureSize(sampler2D(texImages[nonuniformEXT(image)],bindlessSamplers[D.sampIdx.w]),int(lod)));
    // Stay inside the mip's border texel centres. AO's base and lighting
    // passes may use different address modes; both must build the same shell.
    uv=(clamp(uv,0.,1.)*max(levelSize-1.,vec2(0.))+.5)/levelSize;
    vec3 paint=textureLod(sampler2D(texImages[nonuniformEXT(image)],bindlessSamplers[D.sampIdx.w]),uv,lod).rgb;
    return smoothstep(.46,.78,dot(paint,vec3(.3,.59,.11)));
}
float PowderCover(vec2 world)
{
    if(FL.snow.x<=0. || floatBitsToUint(D.terrain.y)==0u)return 0.;
    vec2 cell=world*FL.nativeHeight.yz;
    vec2 base=floor(cell-.5),t=cell-.5-base;
    vec2 blend=smoothstep(vec2(.28),vec2(.72),t);
    float cover=0.;
    for(int i=0;i<4;++i){
        vec2 offset=vec2(i&1,i>>1);
        vec2 weight=mix(1.-blend,blend,offset);
        if(weight.x*weight.y<.001)continue;
        vec2 local=1.-abs(1.-abs(cell-base-offset));
        cover+=PowderCell(base+offset,local)*weight.x*weight.y;
    }
    return cover*FL.snow.x;
}
float PowderHeight(vec2 world)
{
    return .025+.05*TerrainNoise(world*1.65)+.025*TerrainNoise(world*4.1+7.3);
}
float PowderGroundHeight(vec2 world)
{
    uint slot=floatBitsToUint(FL.nativeHeight.x)-1u;
    ivec2 size=textureSize(sampler2D(texImages[slot],bindlessSamplers[D.sampIdx.w]),0);
    vec2 at=world*FL.nativeHeight.yz,f=fract(at);ivec2 cell=ivec2(floor(at));
    vec4 h;
    for(int i=0;i<4;++i){
        uvec4 b=uvec4(texelFetch(sampler2D(texImages[slot],bindlessSamplers[D.sampIdx.w]),
            clamp(cell+ivec2(i&1,i>>1),ivec2(0),size-1),0)*255.+.5);
        h[i]=uintBitsToFloat((b.a<<24)|(b.r<<16)|(b.g<<8)|b.b);
    }
    return mix(mix(h.x,h.y,f.x),mix(h.z,h.w,f.x),f.y);
}
float PowderUp(vec2 world)
{
    if(floatBitsToUint(FL.nativeHeight.x)==0u)return 1.;
    vec2 e=.5/max(FL.nativeHeight.yz,vec2(1e-6));
    float gx=(PowderGroundHeight(world+vec2(e.x,0.))-PowderGroundHeight(world-vec2(e.x,0.)))/(2.*e.x);
    float gz=(PowderGroundHeight(world+vec2(0.,e.y))-PowderGroundHeight(world-vec2(0.,e.y)))/(2.*e.y);
    return inversesqrt(1.+gx*gx+gz*gz);
}
