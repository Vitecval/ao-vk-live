#version 450
// Adapted from VividTone 1.2.1 by Barbatos, MIT.
// Attribution and license: licenses/Barbatos-VividTone.txt.
layout(set=0,binding=0) uniform sampler2D sourceImage;
layout(set=0,binding=1) uniform sampler2D sceneLuminance;
layout(push_constant) uniform Push { vec4 tone; vec4 settings; } P;
// tone: intensity, gamma strength, exposure strength, shadow lift. settings.x: contrast.
layout(location=0) out vec4 outColor;
float Luma(vec3 c){return dot(c,vec3(0.2126,0.7152,0.0722));}
void main(){
    vec4 original=texelFetch(sourceImage,ivec2(gl_FragCoord.xy),0);
    vec3 c=pow(abs(original.rgb),vec3(mix(1.0,4.0,P.tone.y))),before=c;
    float sceneLum=texelFetch(sceneLuminance,ivec2(0),0).r;
    float bright=smoothstep(0.15,0.5,sceneLum);
    c*=exp2(3.0*P.tone.z);
    float lum=Luma(c);
    vec3 dark=c+P.tone.w*(1.0-bright)*pow(abs(1.0-lum),2.0)*0.1;
    vec3 light=mix(c,pow(abs(c/0.5),vec3(1.0+2.0*P.settings.x*bright))*0.5,0.5);
    c=mix(dark,light,bright);
    float key=clamp((0.5/clamp(sceneLum,0.01,2.0))*mix(1.5,0.8,bright),0.1,5.0);
    lum=Luma(c);
    float scaled=lum*key;
    float mapped=scaled/(1.0+scaled/mix(1.6,2.4,bright));
    vec3 mappedColor=c*(mapped/max(lum,0.0001));
    mappedColor=mix(vec3(mapped),mappedColor,mix(1.2,0.9,bright));
    c=mix(before,mappedColor,0.5);
    lum=Luma(c);
    float under=clamp(1.0-lum*2.0,0.0,1.0)*(1.0-bright*0.5);
    float over=clamp(lum*2.0-1.0,0.0,1.0)*(1.0+bright*0.5);
    c=pow(abs(c),vec3(mix(2.5,1.5,bright)))*under+c*(1.0-under-over)+pow(abs(c),vec3(mix(0.6,0.8,bright)))*over;
    outColor=vec4(mix(original.rgb,clamp(c,0.0,1.0),P.tone.x),original.a);
}
