#version 450
#include "rain_common.glsl"
layout(location=0) in vec2 vCorner;
layout(location=1) in vec3 vColour;
layout(location=2) in float vAlpha;
layout(location=3) in vec4 vClip;
layout(location=4) in vec4 vPrevClip;
layout(location=5) in float vGlow;
layout(location=0) out vec4 outScene;
layout(location=1) out vec4 outGlow;
layout(location=2) out vec4 outLocal;
layout(location=3) out vec4 outMotion;
layout(location=4) out vec4 outAlbedo;
void main() {
    float coverage=pow(max(0.,1.-dot(vCorner,vCorner)),2.)*vAlpha;
    if(coverage<.001)discard;
    vec3 colour=vColour*RainFog(length(vClip.w));
    outScene=vec4(colour*coverage,coverage);
    vec3 glow=colour*coverage*vGlow;
    outGlow=vec4(glow,dot(glow,vec3(.3,.59,.11))*gl_FragCoord.w);
    // Tiny transparent motes neither replace world normals nor claim reflective
    // surfaces. Keep their local response and supply their own motion.
    outLocal=vec4(0.);
    vec2 now=vClip.xy/vClip.w,previous=vPrevClip.xy/max(vPrevClip.w,1e-6);
    vec2 motion=(now-previous)*.5*R.viewport.xy*vec2(1.,-1.);
    outMotion=vec4(motion*coverage,0.,coverage);
    outAlbedo=vec4(0.);
}
