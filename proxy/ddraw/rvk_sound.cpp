#include "rvk_sound.h"
#include "rvk_backend.h"
#include "rvk_settings.h"
#include "../../rvk/sound_policy.h"
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace rvkproxy {
namespace {
using rvk::sound::Bed;
constexpr size_t kFrames=4096;
struct State {
    std::atomic<Bed> wanted{Bed::None};std::atomic<bool> quit{false},failed{false};
    std::atomic<float> volume{.6f};
    HANDLE wake=nullptr,thread=nullptr;HMODULE module=nullptr;
    std::filesystem::path directory;
};
State* state=nullptr;
struct Output {
    HWAVEOUT handle=nullptr;
    std::array<std::array<int16_t,kFrames*2>,2> buffers{};
    std::array<WAVEHDR,2> headers{};
    bool Open(HANDLE event) {
        WAVEFORMATEX format{WAVE_FORMAT_PCM,2,48000,192000,4,16,0};
        if(waveOutOpen(&handle,WAVE_MAPPER,&format,DWORD_PTR(event),0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){handle=nullptr;return false;}
        for(size_t i=0;i<2;++i){headers[i].lpData=reinterpret_cast<LPSTR>(buffers[i].data());headers[i].dwBufferLength=DWORD(buffers[i].size()*2);
            if(waveOutPrepareHeader(handle,&headers[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){Close();return false;}}
        return true;
    }
    bool Busy(size_t i)const{return (headers[i].dwFlags&WHDR_INQUEUE)!=0;}
    bool Busy()const{return Busy(0)||Busy(1);}
    void Close(){if(!handle)return;waveOutReset(handle);for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(handle,&h,sizeof(h));waveOutClose(handle);handle=nullptr;headers={};}
    ~Output(){Close();}
};
bool Read(const std::filesystem::path& path,std::vector<int16_t>& pcm) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;
    const auto size=file.tellg();if(size<44||size>std::streamoff(64u<<20))return false;
    std::vector<uint8_t> bytes(size_t(size),uint8_t{});file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),size);
    return bool(file)&&rvk::sound::Decode(bytes,pcm);
}
DWORD WINAPI Run(void* opaque) {
    auto& s=*static_cast<State*>(opaque);
    {
        Output output;std::vector<int16_t> pcm;Bed playing=Bed::None,failed=Bed::None;
        size_t cursor=0;float fade=0,volume=.6f;bool draining=false;
        try {
            while(!s.quit.load()) {
                const Bed wanted=s.wanted.load();
                if(wanted==Bed::None)failed=Bed::None;
                if(playing==Bed::None&&wanted!=Bed::None&&wanted!=failed) {
                    const char* name=rvk::sound::File(wanted);
                    if(!name||!Read(s.directory/name,pcm)||!output.Open(s.wake)) {
                        output.Close();pcm.clear();failed=wanted;
                        RvkLog("sound rework: ambience unavailable; original audio retained (%s)",name?name:"unsupported");
                    } else {playing=wanted;cursor=0;fade=0;draining=false;RvkLog("sound rework: quiet regional ambience %s; original music/effects unchanged",name);}
                }
                if(playing!=Bed::None) {
                    if(draining&&!output.Busy()) {output.Close();std::vector<int16_t>().swap(pcm);playing=Bed::None;draining=false;continue;}
                    for(size_t i=0;i<2&&!draining;++i)if(!output.Busy(i)) {
                        const bool leave=wanted!=playing;
                        const float targetVolume=s.volume.load();
                        for(size_t f=0;f<kFrames;++f) {
                            fade=std::clamp(fade+(leave?-1.f:1.f)/(48000.f*.4f),0.f,1.f);
                            volume+=std::clamp(targetVolume-volume,-1.f/4800.f,1.f/4800.f);
                            for(unsigned c=0;c<2;++c)output.buffers[i][f*2+c]=rvk::sound::Output(rvk::sound::Sample(pcm,cursor,c),fade,volume);
                            cursor=rvk::sound::Next(cursor,pcm.size()/2);
                        }
                        if(waveOutWrite(output.handle,&output.headers[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){failed=playing;output.Close();playing=Bed::None;draining=false;pcm.clear();RvkLog("sound rework: output unavailable; original audio retained");break;}
                        if(leave&&fade==0)draining=true;
                    }
                }
                WaitForSingleObject(s.wake,playing==Bed::None?100:20);
            }
        }catch(...){s.failed=true;RvkLog("sound rework: playback stopped; original audio retained");}
    }
    s.quit=true;
    const HMODULE module=s.module;FreeLibraryAndExitThread(module,0);
}
bool Start() {
    auto next=std::make_unique<State>();
    next->directory=std::filesystem::path(rvk_settings::IniPath()).parent_path()/"randy-vk"/"audio"/"ambience";
    next->wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!next->wake)return false;
    if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCSTR>(&Run),&next->module)){CloseHandle(next->wake);return false;}
    next->thread=CreateThread(nullptr,0,Run,next.get(),0,nullptr);
    if(!next->thread){FreeLibrary(next->module);CloseHandle(next->wake);return false;}
    state=next.release();return true;
}
}
void SoundFrame(bool live) {
    const bool enabled=rvk_settings::Get("RVK_ReworkSound")!=0;
    uint32_t zone=0;bool outdoor=false;
    if(enabled&&live)ReadSoundContext(zone,outdoor);
    const bool underwater=enabled&&live&&g_rvk.device&&g_rvk.device->UnderwaterLive(zone);
    const Bed wanted=rvk::sound::Select(enabled,live,zone,outdoor,underwater);
    if(state&&state->quit.load()) {
        if(WaitForSingleObject(state->thread,0)!=WAIT_OBJECT_0)return;
        if(state->failed.load()&&wanted!=Bed::None&&wanted==state->wanted.load())return;
        CloseHandle(state->thread);CloseHandle(state->wake);delete state;state=nullptr;
    }
    if(!state&&wanted==Bed::None)return;
    try{if(!state&&!Start())return;}catch(...){return;}
    state->volume=std::clamp(rvk_settings::Get("RVK_SoundVol"),0.f,1.f);
    if(state->wanted.exchange(wanted)!=wanted)SetEvent(state->wake);
}
void SoundShutdown() {
    if(!state)return;state->quit=true;SetEvent(state->wake);
    // WM_DESTROY calls this outside the DLL loader lock. The worker never takes
    // the graphics COM lock; its own module reference also protects late exit.
    if(WaitForSingleObject(state->thread,5000)==WAIT_OBJECT_0){CloseHandle(state->thread);CloseHandle(state->wake);delete state;state=nullptr;}
}
}
