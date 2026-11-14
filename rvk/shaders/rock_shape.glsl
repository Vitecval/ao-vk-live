// A continuous WORLD-space field, shared by every rock. Closed UV/normal seams
// deform identically; actual mesh borders are pinned in the evaluation stage.
vec3 RockWarp(vec3 p)
{
    float a=TerrainNoise(p.xz*.63+vec2(p.y*.19,3.7));
    float b=TerrainNoise(p.zy*.71+vec2(p.x*.17,11.2));
    float strata=sin(p.y*4.7+TerrainNoise(p.xz*.18)*3.0);
    return vec3((a-.5)*1.5+.15*strata,(TerrainNoise(p.xz*.9+vec2(p.y*.27))-.5)*.24,(b-.5)*1.5+.15*strata);
}
vec3 RockPoint(vec3 bary,vec3 a,vec3 b,vec3 c,uint borders,vec3 eye,vec3 na,vec3 nb,vec3 nc)
{
    vec3 p=bary.x*a+bary.y*b+bary.z*c;
    float area=length(cross(b-a,c-a));
    vec3 heights=area/max(vec3(length(c-b),length(c-a),length(b-a)),vec3(1e-5));
    float pin=1.0;
    for(int e=0;e<3;++e)if((borders&(1u<<e))!=0u)pin*=smoothstep(0.0,.35,bary[e]*heights[e]);
    float crease=1.0;
    for(int e=0;e<3;++e)if((borders&(8u<<e))!=0u)crease*=smoothstep(0.0,.35,bary[e]*heights[e]);
    vec3 pull=vec3(0.0);
    if(dot(na,na)>1e-8){na=normalize(na);pull-=bary.x*dot(p-a,na)*na;}
    if(dot(nb,nb)>1e-8){nb=normalize(nb);pull-=bary.y*dot(p-b,nb)*nb;}
    if(dot(nc,nc)>1e-8){nc=normalize(nc);pull-=bary.z*dot(p-c,nc)*nc;}
    pull*=.45;pull*=min(1.0,(D.tess.w*.65)/max(length(pull),1e-6));
    float fade=1.0-smoothstep(.5*D.tess.z,D.tess.z,distance(p,eye));
    return p+(RockWarp(p)*D.tess.w+pull*crease)*(D.tess.y*pin*fade);
}
