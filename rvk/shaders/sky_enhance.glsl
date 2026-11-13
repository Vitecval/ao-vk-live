// Sky-only volume: deterministic samples, no temporal noise, shadows or exposure inputs.
float FairDensity(vec3 p) {
    float height=smoothstep(0.,.18,p.y)*(1.-smoothstep(.55,1.2,p.y));
    vec3 q=p*vec3(1.,1.2,1.);
    float mass=Noise3(q*.85)*.65+Noise3(q*1.9+7.1)*.35;
    float billow=.65*Noise3(q*5.4+13.7)+.35*Noise3(q*11.1+31.4);
    return max(mass-.49-(1.-height)*.25-(1.-billow)*.14,0.)*height*7.;
}

vec4 FairClouds(vec3 ray) {
    if(P.view.w<.5 || ray.y<=0.)return vec4(0.);
    float clock=P.night.y*6.2831853/7200.;
    vec3 drift=vec3(cos(clock),0.,sin(clock))*2.8+vec3(P.sky3.x*.013,0.,3.7);
    // Intersect a curved shell: clamping elevation repeats one cloud column down the horizon.
    const float radius=80.,base=1.4,top=2.6;
    float up=radius*ray.y;
    float start=-up+sqrt(up*up+2.*radius*base+base*base);
    float end=-up+sqrt(up*up+2.*radius*top+top*top),stepLength=(end-start)/48.;
    vec3 sun=normalize(P.sky2.xyz),lightDir=normalize(vec3(sun.x,max(sun.y,.18),sun.z));
    float day=1.-P.sky3.y,sunset=(1.-smoothstep(.03,.32,abs(sun.y)))*day;
    vec3 ambient=mix(vec3(.055,.085,.15),vec3(.38,.49,.66),day);
    vec3 sunlight=mix(vec3(.27,.36,.54),mix(vec3(1.12,1.10,1.04),vec3(1.25,.74,.44),sunset*.8),day);
    float silver=pow(max(dot(ray,sun),0.),12.);
    vec3 colour=vec3(0.);float through=1.;
    for(int i=0;i<48;++i) {
        float t=start+(float(i)+.5)*stepLength;
        vec3 at=ray*t;
        float height=length(at+vec3(0.,radius,0.))-radius-base;
        vec3 p=vec3(at.x,height,at.z)+vec3(drift.x,0.,drift.z);
        float density=FairDensity(p);
        if(density>.001) {
            float shadow=FairDensity(p+lightDir*.45)*.85;
            float lit=exp(-shadow*2.4);
            float alpha=1.-exp(-density*stepLength*3.2);
            vec3 scattering=ambient+sunlight*(lit*.78+silver*.35*exp(-shadow));
            colour+=through*alpha*scattering;through*=1.-alpha;
            if(through<.015)break;
        }
    }
    float alpha=1.-through;
    if(alpha<.001)return vec4(0.);
    float haze=1.-exp(-start*.065);
    vec3 hazeColour=mix(vec3(.08,.12,.21),vec3(.57,.68,.81),day);
    colour=mix(colour/alpha,hazeColour,haze*.6);
    return vec4(colour,alpha*smoothstep(0.,.065,ray.y)*mix(.65,1.,smoothstep(.02,.20,ray.y)));
}

// Revision 39's smooth hull and engine pods, restored from the prior shader.
float CraftEllipsoid(vec3 origin,vec3 ray,vec3 centre,vec3 size,out vec3 normal) {
    vec3 o=(origin-centre)/size,d=ray/size;
    float a=dot(d,d),b=dot(o,d),disc=b*b-a*(dot(o,o)-1.);
    normal=vec3(0.,1.,0.);
    if(disc<0.)return 1e20;
    float t=(-b-sqrt(disc))/a;
    if(t<=0.)return 1e20;
    normal=normalize((o+d*t)/size);
    return t;
}
vec3 SkyTraffic(vec3 background,vec3 ray) {
    if(P.sky2.w<.5 || ray.y<.035)return background;
    int count=P.sky2.w>1.5 ? 12:8;
    float day=1.-P.sky3.y,nearest=1e20;
    vec3 result=background;
    for(int i=0;i<count;++i) {
        float index=float(i),seed=index*1.618+P.sky3.x*.007;
        float angle=index*6.2831853/float(count)+P.sky3.x*.031+P.night.y*6.2831853/7200.*(i%2==0 ? 1.:-1.);
        vec3 centre=normalize(vec3(cos(angle),.19+.44*fract(seed*.731),sin(angle)))*(1450.+index*87.);
        float hullLength=i%4==0 ? 135.:i%3==0 ? 80.:50.;
        if(dot(ray,normalize(centre))<.990)continue;
        vec3 forward=vec3(-sin(angle),0.,cos(angle)),side=vec3(cos(angle),0.,sin(angle));
        vec3 origin=vec3(dot(-centre,forward),-centre.y,dot(-centre,side));
        vec3 direction=vec3(dot(ray,forward),ray.y,dot(ray,side));
        vec3 normal,n;
        float t=CraftEllipsoid(origin,direction,vec3(0.),hullLength*vec3(1.,.12,.37),normal);
        float bridge=CraftEllipsoid(origin,direction,hullLength*vec3(.13,.11,0.),hullLength*vec3(.42,.14,.21),n);
        bool cabin=bridge<t;
        if(cabin){t=bridge;normal=n;}
        if(i%3!=2)for(int pod=0;pod<2;++pod) {
            float engine=CraftEllipsoid(origin,direction,hullLength*vec3(-.20,-.035,(pod==0 ? -1.:1.)*.35),hullLength*vec3(.65,.045,.17),n);
            if(engine<t){t=engine;normal=n;cabin=false;}
        }
        if(t<nearest) {
            nearest=t;
            vec3 at=origin+direction*t;
            vec3 sun=normalize(vec3(dot(P.sky2.xyz,forward),P.sky2.y,dot(P.sky2.xyz,side)));
            float windows=(1.-smoothstep(.5,1.8,abs(at.y-hullLength*.035)))*step(.35,fract(at.x*.37));
            if(cabin)windows=max(windows,(1.-smoothstep(.25,1.,abs(at.y-hullLength*.15)))*step(.4,fract(at.x*.41)));
            float panel=.86+.14*step(.18,fract(at.x*.13));
            result=vec3(.12,.16,.21)*mix(.21,.65+.9*max(dot(normal,sun),0.),day)*panel;
            result+=vec3(.025,.34,.62)*windows*mix(1.4,.55,day);
        }
        for(int lamp=0;lamp<3;++lamp) {
            vec3 local=lamp<2 ? vec3(-hullLength*.72,0.,(lamp==0 ? -1.:1.)*hullLength*.35):vec3(hullLength*.21,hullLength*.24,0.);
            vec3 pos=centre+forward*local.x+vec3(0.,local.y,0.)+side*local.z;
            float angular=length(cross(ray,normalize(pos)));
            float radius=lamp<2 ? .00095:.00065;
            float pulse=lamp==2 ? mix(.08,1.,step(.88,fract(P.night.y*.65+index*.27))):1.;
            vec3 colour=lamp<2 ? vec3(.025,.56,1.):vec3(1.,.06,.025);
            result+=colour*exp(-angular*angular/(radius*radius))*pulse*mix(1.1,.55,day);
        }
    }
    return result;
}
