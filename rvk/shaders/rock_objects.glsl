// Original mineral meshes share the cliff's photographic PBR set. Geometry,
// alpha and silhouettes stay original; filtered world-space detail has no
// view-dependent UV selection and fades before its fine structure can shimmer.
bool gObjectRockReady=false;
float gObjectRockAmount=0.0;
vec3 gObjectRockColour=vec3(1.0),gObjectRockSlope=vec3(0.0),gObjectRockOrm=vec3(1.0);
void InitObjectRock()
{
    if(gObjectRockReady)return;
    gObjectRockReady=true;
    if((D.mat.z&MAT_ENHANCED_ROCK)==0u || FL.rockObjects.x<=0.0 ||
       floatBitsToUint(FL.terrainRock.x)==0u || abs(FL.terrainRock.w)<.001)return;
    vec3 p=vPosW/6.0,dx=dFdx(p),dy=dFdy(p);
    float n2=dot(vNormalW.xyz,vNormalW.xyz);if(n2<1e-8)return;
    vec3 weights=pow(abs(vNormalW.xyz*inversesqrt(n2)),vec3(4.0));
    weights/=max(dot(weights,vec3(1.0)),1e-6);
    float footprint=max(length(dx),length(dy));
    float distanceFade=1.0-smoothstep(50.0,140.0,length(vPosW-C.eyePos.xyz));
    float filterFade=1.0-smoothstep(.025,.10,footprint);
    vec3 base=Sample(0u).rgb;
    float moss=smoothstep(.06,.25,(base.g-max(base.r,base.b))/max(dot(base,vec3(.3,.59,.11)),.04));
    gObjectRockAmount=clamp(FL.rockObjects.x,0.0,1.0)*distanceFade*filterFade*(1.0-.9*moss);
    if(gObjectRockAmount<=.001)return;
    vec3 colour=vec3(0.0),orm=vec3(0.0),slope=vec3(0.0);float total=0.0;
    for(int face=0;face<3;++face){
        float weight=weights[face];if(weight<.002)continue;
        vec2 uv=face==0 ? vec2(p.z,-p.y):face==1 ? p.xz:vec2(p.x,-p.y);
        vec2 ux=face==0 ? vec2(dx.z,-dx.y):face==1 ? dx.xz:vec2(dx.x,-dx.y);
        vec2 uy=face==0 ? vec2(dy.z,-dy.y):face==1 ? dy.xz:vec2(dy.x,-dy.y);
        colour+=weight*textureGrad(sampler2D(texImages[TerrainRockImage(0)],bindlessSamplers[D.sampIdx.w]),uv,ux,uy).rgb;
        orm+=weight*textureGrad(sampler2D(texImages[TerrainRockImage(2)],bindlessSamplers[D.sampIdx.w]),uv,ux,uy).rgb;
        vec3 normal=textureGrad(sampler2D(texImages[TerrainRockImage(1)],bindlessSamplers[D.sampIdx.w]),uv,ux,uy).xyz*2.0-1.0;
        vec2 tilt=normal.xy/max(normal.z,.35);
        slope+=weight*(face==0 ? vec3(0.0,-tilt.y,tilt.x):face==1 ? vec3(tilt.x,0.0,tilt.y):vec3(tilt.x,-tilt.y,0.0));
        total+=weight;
    }
    gObjectRockColour=colour/max(total,.001);
    gObjectRockOrm=orm/max(total,.001);
    gObjectRockSlope=slope/max(total,.001);
}
vec3 ObjectRockColour(vec3 colour)
{
    InitObjectRock();if(gObjectRockAmount<=.001)return colour;
    const vec3 luma=vec3(.3,.59,.11);
    float detail=clamp(dot(gObjectRockColour,luma)/max(dot(FL.terrainRock2.rgb,luma),.02),.40,1.90);
    return colour*mix(1.0,detail,.75*gObjectRockAmount);
}
vec3 ObjectRockNormal(vec3 normal)
{
    InitObjectRock();float len=length(normal);
    if(gObjectRockAmount<=.001 || len<1e-6)return normal;
    vec3 unit=normal/len;
    vec3 tangent=gObjectRockSlope-unit*dot(gObjectRockSlope,unit);
    return normalize(unit+tangent*(.42*gObjectRockAmount*clamp(FL.terrainRock2.w,0.0,2.0)))*len;
}
