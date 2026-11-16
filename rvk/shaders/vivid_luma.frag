#version 450
// VividTone 1.2.1 by Barbatos (MIT); native Vulkan adaptation.
// See licenses/Barbatos-VividTone.txt. No ReShade runtime required.
layout(set=0,binding=0) uniform sampler2D sourceImage;
layout(set=0,binding=1) uniform sampler2D previousLuminance;
layout(push_constant) uniform Push { vec4 adaptation; } P; // dt, rate, history valid, static
layout(location=0) out vec4 outColor;
void main(){
    float sum=0.0;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        vec3 c=texture(sourceImage,(vec2(x,y)+0.5)/8.0).rgb;
        sum+=log(max(dot(c,vec3(0.2126,0.7152,0.0722)),0.0001));
    }
    float current=exp(sum/64.0),value=current;
    // Initialize from the current scene, never undefined/black history.
    if(P.adaptation.z>0.5){
        float previous=texelFetch(previousLuminance,ivec2(0),0).r;
        float ease=1.0-exp(-clamp(P.adaptation.x,0.0,0.1)*mix(0.1,5.0,clamp(P.adaptation.y,0.0,1.0)));
        value=P.adaptation.w>0.5 ? previous:mix(previous,current,ease);
    }
    outColor=vec4(value,0.0,0.0,1.0);
}
