// Run only beside an isolated renderer copy; never load the retail client.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ddraw/rvk_settings.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>

static void quoted(FILE* f,const char* s){
    std::fputc('"',f);for(const unsigned char* p=(const unsigned char*)(s?s:"");*p;++p){
        if(*p=='"'||*p=='\\')std::fputc('\\',f);
        if(*p<32)std::fprintf(f,"\\u%04x",unsigned(*p));else std::fputc(*p,f);
    }std::fputc('"',f);
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    auto lib=LoadLibraryW(L"ao-vk.dll");if(!lib)return 3;
    auto get=(int(__cdecl*)(uint32_t,RvkSettingInfo*))GetProcAddress(lib,"RvkSettings_Get");
    auto count=(uint32_t(__cdecl*)())GetProcAddress(lib,"RvkSettings_Count");
    auto set=(int(__cdecl*)(const char*,float))GetProcAddress(lib,"RvkSettings_Set");
    auto apply=(int(__cdecl*)(uint32_t))GetProcAddress(lib,"RvkPresets_Apply");
    auto names=(const char*(__cdecl*)(uint32_t))GetProcAddress(lib,"RvkPresets_Name");
    if(!get||!count||!set||!apply||!names)return 4;
    std::vector<RvkSettingInfo> settings(count());
    for(uint32_t i=0;i<settings.size();++i){settings[i].size=sizeof(RvkSettingInfo);if(!get(i,&settings[i]))return 5;}
    FILE* f=std::fopen(argv[1],"wb");if(!f)return 6;
    std::fprintf(f,"{\"rendererRevision\":62,\"settings\":[\n");
    for(size_t i=0;i<settings.size();++i){auto& s=settings[i];if(i)std::fputc(',',f);
        std::fputs("{\"key\":",f);quoted(f,s.name);std::fputs(",\"label\":",f);quoted(f,s.label);
        std::fputs(",\"section\":",f);quoted(f,s.section);
        std::fprintf(f,",\"type\":%u,\"min\":%.9g,\"max\":%.9g,\"step\":%.9g,\"defaultValue\":%.9g,\"parent\":",s.type,s.min,s.max,s.step,s.defaultValue);
        quoted(f,s.parent);std::fputs(",\"choices\":",f);quoted(f,s.choices);std::fputc('}',f);
    }
    // Determine the preset's exact write set using two distinct initial states.
    // Settings excluded by the renderer retain different values in the two runs.
    // This keeps personal options out without duplicating its preset logic.
    std::fputs("],\"presets\":[",f);
    for(uint32_t preset=0;preset<4;++preset){
        std::vector<float> low(settings.size()),high(settings.size());
        for(int seed=0;seed<2;++seed){
            for(auto& s:settings)set(s.name,seed?s.max:s.min);
            if(!apply(preset))return 7;
            for(uint32_t i=0;i<settings.size();++i){RvkSettingInfo s{};s.size=sizeof(s);get(i,&s);(seed?high:low)[i]=s.value;}
        }
        if(preset)std::fputc(',',f);std::fputs("{\"name\":",f);quoted(f,names(preset));std::fputs(",\"values\":{",f);bool first=true;
        for(size_t i=0;i<settings.size();++i)if(std::fabs(low[i]-high[i])<1e-5f){
            if(!first)std::fputc(',',f);first=false;quoted(f,settings[i].name);std::fprintf(f,":%.9g",low[i]);
        }std::fputs("}}",f);
    }
    std::fputs("]}\n",f);std::fclose(f);return 0;
}
