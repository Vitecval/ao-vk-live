// Runs next to an isolated addon copy. Never bootstraps or launches a game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ddraw/rvk_settings.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

int main(int argc,char**) {
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
    if(argc>1){check(std::fabs(info("RVK_ICCGlossiness").value-.2f)<.001,"restart gloss");check(info("RVK_ICCArtwork").value==1,"restart artwork");std::puts("saved selections survived process restart");return 0;}
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
    set("RVK_TimeOfDay",14);set("RVK_TimeSpeed",20);
    check(effective("RVK_TimeOfDay")==-1 && effective("RVK_TimeSpeed")==1,"Enhanced Daylights must retain server clock");
    for(int v=0;v<=20;++v){set("RVK_GlobalShiny",v/20.f);check(std::fabs(info("RVK_GlobalShiny").value-v/20.f)<.0001,"global shiny slider");}
    set("RVK_ICCArtwork",1);set("RVK_ICCGlossiness",.2f);
    std::puts("settings passed: Ultra defaults, all presets clouds off, server clock, gloss sliders and no Living controls");
}
