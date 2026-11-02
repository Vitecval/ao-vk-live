// Small reusable primitives, shared in apartment coordinates. No room cards.
float RoomActivity(uint hash)
{
    if(FL.windowLife.y<.5 || ((hash>>12u)&3u)==0u)return 1.0;
    // Divisors of 4096 keep the UTC wrap continuous. 17/34-minute periods,
    // with roughly 25/50-second fades, stable after reopening the client.
    float period=((hash>>14u)&1u)!=0u?2048.0:1024.0;
    float phase=FL.windowLife.x/period+float((hash>>16u)&1023u)/1024.0;
    return mix(.22,1.0,smoothstep(-.08,.08,sin(phase*6.28318530718)));
}

void RoomLifeProp(vec3 s,uint preset,int i,out vec3 lo,out vec3 hi,out uint mat,out uint shape)
{
    lo=hi=vec3(0);mat=2u;shape=0u;
    bool office=preset==2u||(preset>=6u&&preset<=8u);
    float sx=s.x,sz=s.z-gWindowDoorInset;
    float k=clamp(min(sx,sz)/4.5,.65,1.05),tableX=max(.25,sx*.42-.6);
    // Ceiling features stay physically attached to the shared room.
    if(i==0){lo=vec3(.04,s.y-.15,sz*.32);hi=lo+vec3(sx-.08,.12,.10);mat=office?3u:2u;}
    if(i==1){lo=vec3(.04,s.y-.15,sz*.70);hi=lo+vec3(sx-.08,.12,.10);mat=office?3u:2u;}
    if(i==2){lo=vec3(.22,s.y-.105,.10);hi=lo+vec3(.055,.055,max(.1,sz-.2));mat=3u;shape=1u;}
    if(i==3){lo=vec3(sx*.46-.50,s.y-.23,max(.42,sz*.20));hi=lo+vec3(1.0,.065,.28);mat=9u;}
    if(i==4){lo=vec3(sx*.70-.25,s.y-.045,sz*.48);hi=lo+vec3(.50,.03,.38);mat=15u;}
    if(i==5){lo=vec3(tableX,.65,.55);hi=lo+vec3(1.22,.11,.50);mat=office?3u:2u;}
    if(i==6){lo=vec3(tableX+.10,.76,.66);hi=lo+vec3(.17,office?.12:.20,.17);mat=office?6u:16u;shape=office?0u:2u;}
    if(i==7){lo=vec3(tableX+.04,.91,.60);hi=lo+vec3(.31,.40,.30);mat=office?3u:17u;shape=office?0u:4u;}
    if(i==8){lo=vec3(tableX+.94,.76,.72);hi=lo+vec3(.105,.13,.105);mat=16u;shape=2u;}
    if(i==9){lo=vec3(tableX+.42,.76,.68);hi=lo+vec3(.34,.10,.25);mat=6u;}
    if(i==10){
        if(office){float z=max(.4,sz-1.5*k);lo=vec3(1.02*k,.64,z-.72*k);hi=lo+vec3(.36,.48,.055);}
        else{lo=vec3(max(.25,sx-2.4*k)+.19,.655,max(.25,sz-2.6*k)+.35);hi=lo+vec3(.76,.045,.66);}
        mat=14u;
    }
    if(i==11){lo=vec3(sx-.055,1.42,sz*.35);hi=lo+vec3(.025,.62,.44);mat=11u;}
    if(i==12&&gWindowDoorOpen){lo=vec3(sx*.5-.33,s.y-.22,s.z-gWindowDoorInset*.55);hi=lo+vec3(.66,.06,.22);mat=9u;}
    if(i==13){lo=vec3(max(.25,sx-1.35),.02,.56);hi=lo+vec3(.75,1.15,.58);mat=office?3u:2u;}
    if(i==14&&gWindowDoorOpen){lo=vec3(.30,.02,s.z-.55);hi=lo+vec3(.65,1.55,.40);mat=office?3u:2u;}
    // Sparse personal belongings, stable across modes, panes and runs.
    if(i>=6&&i<=11&&fract(gWindowHabit*float(17+i)*1.618)>.76)lo=hi=vec3(0);
}
