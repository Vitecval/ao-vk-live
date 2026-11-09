// Runs next to an isolated addon copy. Never bootstraps or launches a game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "ddraw/rvk_settings.h"
#include "native/ring_ranges.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

int main(int argc,char**) {
    // Exercise the actual N3 ring policy: scenery must never expand the small
    // object rings or reach zones that N3 has not loaded. Off is the old policy.
    const float original[5]={.10f,.15f,.30f,.40f,.55f};
    for(float detail:{1.f,2.f,4.f})for(float kept:{.20f,.4f,1.f}) {
        float off[5],on[5],previous=0;
        rnative::rings::ScaledFactors(original,kept,detail,1.f,off);
        rnative::rings::ScaledFactors(original,kept,detail,1.15f,on);
        for(int i=0;i<5;++i) {
            const float wanted=i<2 ? original[i]*detail:std::max(original[i],previous);
            const float old=std::max(original[i],std::min(wanted,kept*2));
            if(std::fabs(off[i]-old)>1e-6f || on[i]<off[i] ||
               on[i]>std::max(original[i],kept*2)+1e-6f ||
               (i<2 && on[i]!=off[i]) || (i>0 && on[i]<on[i-1]))return 9;
            previous=off[i];
        }
    }
    std::puts("scenery rings bounded; small-object budget and disabled policy preserved");
    for(float detail:{1.f,2.f,4.f})for(float kept:{.2f,.75f,1.f})for(float farScale:{1.f,1.5f,2.f,2.5f,3.f}) {
        float base[5],expanded[5];
        rnative::rings::ScaledFactors(original,kept,detail,1.15f,base);
        rnative::rings::ScaledFactors(original,kept,detail,1.15f,expanded,farScale);
        for(int i=0;i<5;++i)
            if(expanded[i]<base[i] || expanded[i]>std::max(original[i],kept*2)+1e-6f ||
               (i>0 && expanded[i]<expanded[i-1]) || (farScale==1 && expanded[i]!=base[i]))return 10;
    }
    float horizon[5];
    rnative::rings::ScaledFactors(original,.75f,2.f,1.15f,horizon,3.f);
    const float expected[5]={.4f,.6f,.9f,1.2f,1.5f};
    for(int i=0;i<5;++i)if(std::fabs(horizon[i]-expected[i])>1e-6f)return 11;
    std::puts("Far Scenery covers distant rock/building zones and stays inside the loaded world");
    HMODULE lib=LoadLibraryA("ao-vk.dll");
    if(!lib) {std::printf("load failed %lu\n",GetLastError());return 1;}
    auto set=reinterpret_cast<int(__cdecl*)(const char*,float)>(GetProcAddress(lib,"RvkSettings_Set"));
    auto get=reinterpret_cast<int(__cdecl*)(uint32_t,RvkSettingInfo*)>(GetProcAddress(lib,"RvkSettings_Get"));
    auto count=reinterpret_cast<uint32_t(__cdecl*)()>(GetProcAddress(lib,"RvkSettings_Count"));
    auto preset=reinterpret_cast<int(__cdecl*)(uint32_t)>(GetProcAddress(lib,"RvkPresets_Apply"));
    auto effective=reinterpret_cast<float(__cdecl*)(const char*)>(GetProcAddress(lib,"RvkSettings_Effective"));
    if(!set||!get||!count||!preset||!effective)return 2;
    auto check=[](bool ok,const char* why){if(!ok){std::printf("FAILED %s\n",why);std::exit(3);}};
    auto info=[&](const char* name){for(uint32_t i=0;i<count();++i){RvkSettingInfo s{};s.size=sizeof(s);if(get(i,&s)&&std::strcmp(s.name,name)==0)return s;}check(false,name);return RvkSettingInfo{};};
    if(argc>1)check(std::fabs(info("RVK_TerraRough").value-.35f)<.001,"restart terrain roughness");
    if(argc>1){check(std::fabs(info("RVK_ICCGlossiness").value-.2f)<.001,"restart gloss");check(info("RVK_ICCArtwork").value==1,"restart artwork");check(effective("RVK_TimeOfDay")==14 && effective("RVK_TimeSpeed")==1,"restart manual clock");check(effective("RVK_FarScenery")==1 && effective("RVK_SceneryDist")==3 && effective("RVK_ViewRange")==3,"restart far scenery");check(info("RVK_ViewRange").value==1.5f,"restart stored ordinary view range");std::puts("saved selections, Far Scenery and manual time survived process restart");return 0;}
    check(info("RVK_TerraRough").defaultValue==.6f && info("RVK_TerraRough").min==0 &&
          info("RVK_TerraRough").max==1 && info("RVK_TerraRough").step==.05f,"terrain roughness schema");
    for(int i=0;i<=20;++i){set("RVK_TerraRough",i*.05f);check(std::fabs(info("RVK_TerraRough").value-i*.05f)<.001,"terrain roughness steps");}
    set("RVK_TerraRough",.35f);
    check(info("RVK_FarScenery").defaultValue==0,"far scenery opt-in default");
    check(info("RVK_SceneryDist").min==1 && info("RVK_SceneryDist").max==3 &&
          std::strcmp(info("RVK_SceneryDist").parent,"RVK_FarScenery")==0,"far distance schema");
    set("RVK_Enhance",1);set("RVK_ViewRange",1.5f);set("RVK_FarScenery",1);
    for(int i=0;i<=8;++i){const float v=1+i*.25f;set("RVK_SceneryDist",v);check(effective("RVK_SceneryDist")==v,"far distance steps");}
    check(effective("RVK_ViewRange")==3 && info("RVK_ViewRange").value==1.5f,"far scenery effective camera/fog range");
    set("RVK_FarScenery",0);
    check(effective("RVK_SceneryDist")==1 && effective("RVK_ViewRange")==1.5f,"far scenery off restores ordinary reach");
    set("RVK_FarScenery",1);set("RVK_Enhance",0);
    check(effective("RVK_FarScenery")==0 && effective("RVK_ViewRange")==1,"original mode disables far scenery");
    set("RVK_Enhance",1);
    check(effective("RVK_FarScenery")==1,"restore far scenery preference");
    const char* cosmetics[]={"RVK_LivePeople","RVK_NPCVariety","RVK_ArmoredCivilians","RVK_LiveDrones","RVK_AirTraffic","RVK_PhasefrontTraffic","RVK_OrbitalCraft","RVK_LiveWild","RVK_LiveMachine","RVK_LiveAtmos"};
    for(const char* name:cosmetics)check(set(name,1)==0,"Living control should be absent");
    for(uint32_t i=0;i<count();++i){RvkSettingInfo s{};s.size=sizeof(s);if(get(i,&s))check(std::strcmp(s.section,"Living Rubi-Ka")!=0,"Living section should be absent");}
    check(info("RVK_ICCArtwork").defaultValue==0,"default artwork");
    const auto gloss=info("RVK_ICCGlossiness");
    check(gloss.min==0 && gloss.max==1 && std::fabs(gloss.step-.1f)<.0001 && gloss.defaultValue==.5f,"slider schema");
    for(int v=0;v<=10;++v){set("RVK_ICCGlossiness",v/10.f);check(std::fabs(info("RVK_ICCGlossiness").value-v/10.f)<.0001,"slider values");}
    set("RVK_ICCArtwork",1);set("RVK_ICCGlossiness",.2f);
    for(unsigned p=0;p<4;++p){preset(p);if(p<3)check(info("RVK_ICCArtwork").value==1 && std::fabs(info("RVK_ICCGlossiness").value-.2f)<.001,"independent ICC preferences");for(const char* name:cosmetics)check(set(name,1)==0,"preset restored Living controls");check(info("RVK_CloudsOn").value==0,"preset clouds default off");}
    check(info("RVK_ICCArtwork").value==0 && info("RVK_ICCGlossiness").value==.5f,"Ultra artwork/gloss");
    check(info("RVK_ModernTex").value==2 && info("RVK_Albedo").value==1,"Ultra HD on");
    check(info("RVK_DayCycle").value==2 && effective("RVK_TimeOfDay")==-1 && effective("RVK_TimeSpeed")==1,"Ultra server daylight");
    check(info("RVK_BuildWear").value==1 && info("RVK_GlobalShiny").value==.5f,"Ultra material defaults");
    check(info("RVK_FarScenery").value==1 && info("RVK_SceneryDist").value==3,"presets preserve optional far scenery");
    check(std::fabs(info("RVK_TerraRough").value-.35f)<.001,"presets preserve terrain roughness");
    set("RVK_TimeOfDay",14);set("RVK_TimeSpeed",20);
    check(effective("RVK_TimeOfDay")==14 && effective("RVK_TimeSpeed")==20,"manual clock must override Enhanced Daylights");
    check(info("RVK_DayCycle").value==2 && effective("RVK_Hdr")==1 && effective("RVK_PixelLight")==1,"manual clock must retain daylight lighting");
    set("RVK_TimeSpeed",1);set("RVK_TimeOfDay",14);
    set("RVK_DayCycle",0);set("RVK_DayCycle",2);
    check(effective("RVK_TimeOfDay")==14,"lighting selection must retain manual hour");
    set("RVK_TimeOfDay",-1);
    check(effective("RVK_TimeOfDay")==-1 && effective("RVK_TimeSpeed")==1,"manual reset must restore automatic clock");
    set("RVK_TimeOfDay",14);
    for(int v=0;v<=20;++v){set("RVK_GlobalShiny",v/20.f);check(std::fabs(info("RVK_GlobalShiny").value-v/20.f)<.0001,"global shiny slider");}
    set("RVK_ICCArtwork",1);set("RVK_ICCGlossiness",.2f);
    std::puts("settings passed: Ultra defaults, server clock default, manual override/reset/restart, gloss sliders and no Living controls");
}
