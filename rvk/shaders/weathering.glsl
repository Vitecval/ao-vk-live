// Localized weathering. Off has no optional texture reads or noise evaluation.
bool gWeatherReady=false;
vec4 gWeatherLayers=vec4(0); // base dirt, elongated streaks, oxidation, chipped coating
float gWeatherFade=0.0,gWeatherProtect=1.0,gWeatherAmount=0.0;
vec3 gWeatherSlope=vec3(0),gWeatherBase=vec3(0);
bool WeatherStage1(){return (uint(D.weather.y)&2097152u)!=0u;}
uint WeatherKind(){return uint(D.weather.y)&7u;}
void InitWeather(){
 if(gWeatherReady || D.weather.x<=0.0)return;
 gWeatherReady=true;
 vec3 n=normalize(vNormalW.xyz+vec3(0,1e-6,0));
 vec2 wall=abs(n.x)>abs(n.z)?vPosW.zy:vPosW.xy;
 float seed=SurfaceNoise(D.world[3].xz*.137+vec2(13.1,7.9)).x*17.0;
 wall.x+=seed;
 float level=clamp(D.weather.x,1.0,3.0);
 float amount=level<1.5?.38:level<2.5?.76:1.0;
 float maintained=clamp(D.weather.w,.35,1.0);
 float vertical=1.0-smoothstep(.42,.88,abs(n.y));
 gWeatherAmount=amount*maintained*vertical;
 if(gWeatherAmount<=0.0)return;
 vec2 uv=WeatherStage1()?vTex1.xy:(gParallax?gUv0:vTex0.xy);
 gWeatherBase=Sample(WeatherStage1()?1u:0u).rgb;
 float lum=dot(gWeatherBase,vec3(.3,.59,.11));
 // A local bright marking is protected, while an entire white wall can still age.
 float lod=max(WeatherStage1()?textureQueryLod(TEX1,uv).x:textureQueryLod(TEX0,uv).x,0.0);
 float broadLum=dot((WeatherStage1()?textureLod(TEX1,uv,lod+2.5):textureLod(TEX0,uv,lod+2.5)).rgb,vec3(.3,.59,.11));
 float marking=smoothstep(.09,.25,lum-broadLum)*smoothstep(.65,.94,lum);
 gWeatherProtect=1.0-marking;
 uint maskSlot=(uint(D.weather.y)>>4u)&131071u;
 if(maskSlot>0u){
  vec3 e=texture(sampler2D(texImages[maskSlot-1u],bindlessSamplers[D.sampIdx.x]),uv).rgb;
  gWeatherProtect*=1.0-smoothstep(.001,.018,max(e.r,max(e.g,e.b)));
 }
 gWeatherAmount*=gWeatherProtect;
 vec3 macro=SurfaceNoise(wall*vec2(.63,.34)+vec2(9.7,3.2));
 // Warp elongated runoff with non-aligned noise: broken trails rather than straight columns.
 vec2 runoff=wall+vec2(.21*macro.x,.6*macro.z);
 vec3 streams=SurfaceNoise(runoff*vec2(3.1,.28)+vec2(2.4,8.1));
 vec3 breakup=SurfaceNoise(wall*vec2(7.3,1.85)+vec2(5.1,6.3));
 vec3 run=SurfaceNoise(wall*vec2(1.25,.48)+vec2(7.1,4.4));
 vec3 grain=SurfaceNoise(wall*vec2(11.0,8.0));
 float height=max(vPosW.y-D.weather.z,0.0);
 float baseHeight=(level<1.5?.9:level<2.5?1.6:2.4)*(.7+.6*(macro.x+.5));
 float base=(1.0-smoothstep(.08,baseHeight,height))*smoothstep(.16,.66,macro.x+.5);
 float threshold=level<1.5?.64:level<2.5?.49:.39;
 float trail=streams.x*.76+breakup.x*.24+.5;
 float streak=pow(smoothstep(threshold-.06,threshold+.24,trail),1.5)*smoothstep(.22,.72,run.x+.5);
 float seam=0.0;
 if((D.mat.z&(MAT_PBR|MAT_AUTO))==MAT_PBR){
  vec3 orm=texture(sampler2D(texImages[D.mat.x],bindlessSamplers[D.mat.y]),uv).rgb;
  seam=smoothstep(.05,.4,1.0-orm.r);
 }
 float oxidation=smoothstep(.43,.72,run.x+.5)*max(streak,seam*.85);
 float footprint=max(length(dFdx(wall)),length(dFdy(wall)));
 float fine=1.0-smoothstep(.018,.13,footprint);
 float chips=smoothstep(.64,.87,grain.x+.5)*smoothstep(.42,.78,macro.x+.5)*fine;
 chips*=max(seam*.8,streak*.75);
 if(level<1.5)chips*=.15;
 gWeatherLayers=vec4(max(base*.9,seam*.8),streak,oxidation,chips)*gWeatherAmount;
 gWeatherFade=smoothstep(.38,.76,macro.x+.5)*gWeatherAmount;
 // Relief uses analytic procedural slopes and existing shading, never displacement.
 float sx=(.05*streams.y+.014*grain.y*fine)*gWeatherAmount;
 float sy=(.006*streams.z+.01*grain.z*fine)*gWeatherAmount;
 gWeatherSlope=abs(n.x)>abs(n.z)?vec3(0,sy,sx):vec3(sx,sy,0);
}
float WeatherMask(){InitWeather();return clamp(max(max(gWeatherLayers.x,gWeatherLayers.y),max(gWeatherLayers.z,gWeatherLayers.w)),0.0,1.0);}
vec3 WeatherColour(vec3 c){
 if(D.weather.x<=0.0)return c;
 InitWeather();if(gWeatherAmount<=0.0)return c;
 uint kind=WeatherKind();
 float lum=dot(c,vec3(.3,.59,.11));
 float scale=clamp(lum/max(dot(gWeatherBase,vec3(.3,.59,.11)),.04),0.0,2.0);
 // Fading and exposed coating are separate from dark dirt; clean paint is not globally darkened.
 vec3 faded=mix(c,vec3(lum)*vec3(1.08,1.04,.95)+vec3(.07)*scale,gWeatherFade*(kind==3u?.42:.13));
 vec3 dirt=kind==1u?c*vec3(.38,.34,.27):c*vec3(.28,.25,.20);
 vec3 aged=mix(faded,dirt,max(gWeatherLayers.x*.80,gWeatherLayers.y*.66));
 if(kind!=1u){
  float rust=gWeatherLayers.z*(kind==2u?.78:.64);
  vec3 oxide=mix(c*vec3(1.06,.55,.28),vec3(.32,.12,.044)*scale,.28);
  aged=mix(aged,oxide,rust);
  vec3 substrate=kind==3u?vec3(.20,.23,.24):vec3(.30,.27,.21);
  aged=mix(aged,mix(c*.7,substrate*scale,.35),gWeatherLayers.w*.46);
 }else{
  aged=mix(aged,vec3(.37,.31,.21)*scale,gWeatherLayers.y*.28);
  aged=mix(aged,c*1.17,gWeatherLayers.w*.34);
 }
 return max(aged,vec3(0));
}
vec3 WeatherNormal(vec3 n){InitWeather();float l=length(n);return gWeatherAmount>0.0 && l>1e-6?normalize(n+gWeatherSlope*l)*l:n;}
