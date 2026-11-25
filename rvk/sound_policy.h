#pragma once
#include "area_profiles.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace rvk::sound {
enum class Bed : uint32_t { None, Grid, City, Desert, Forest, Cold, Inferno, Underwater };
inline Bed Select(bool enabled,bool live,uint32_t zone,bool outdoor,bool underwater=false) {
    if(!enabled||!live)return Bed::None;
    if(underwater)return Bed::Underwater;
    if(zone==152)return Bed::Grid;
    if(!outdoor)return Bed::None;
    if(zone==540||zone==640)return Bed::City;
    switch(areas::For(zone)) {
    case areas::Biome::City:return Bed::City;
    case areas::Biome::Desert:return Bed::Desert;
    case areas::Biome::Forest:return Bed::Forest;
    case areas::Biome::Snow:return Bed::Cold;
    case areas::Biome::Volcanic:return Bed::Inferno;
    default:return Bed::None;
    }
}
inline const char* File(Bed bed) {
    switch(bed) {
    case Bed::Grid:return "grid.wav";case Bed::City:return "city.wav";
    case Bed::Desert:return "desert.wav";case Bed::Forest:return "forest.wav";
    case Bed::Cold:return "cold.wav";case Bed::Inferno:return "inferno.wav";
    case Bed::Underwater:return "underwater.wav";
    default:return nullptr;
    }
}
inline uint32_t U32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint16_t U16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
// Fixed delivery format keeps malformed/unsupported media out of the audio driver.
inline bool Decode(std::span<const uint8_t> bytes,std::vector<int16_t>& pcm) {
    pcm.clear();
    if(bytes.size()<44||bytes.size()>(64u<<20)||std::memcmp(bytes.data(),"RIFF",4)||std::memcmp(bytes.data()+8,"WAVE",4))return false;
    const uint64_t end=uint64_t(U32(bytes.data()+4))+8;
    if(end>bytes.size()||end<12)return false;
    bool format=false;std::span<const uint8_t> data;
    for(size_t at=12;at+8<=end;) {
        const uint32_t length=U32(bytes.data()+at+4);const uint64_t next=uint64_t(at)+8+length;
        if(next>end)return false;
        const auto* p=bytes.data()+at+8;
        if(!std::memcmp(bytes.data()+at,"fmt ",4)) {
            if(length<16||U16(p)!=1||U16(p+2)!=2||U32(p+4)!=48000||U32(p+8)!=192000||U16(p+12)!=4||U16(p+14)!=16)return false;
            format=true;
        } else if(!std::memcmp(bytes.data()+at,"data",4))data={p,length};
        at=size_t(next+(length&1u));
    }
    if(!format||data.size()<192000||data.size()%4)return false;
    pcm.resize(data.size()/2);
    for(size_t i=0;i<pcm.size();++i)pcm[i]=int16_t(U16(data.data()+i*2));
    return true;
}
// Three-second overlap at the existing loop boundary; no source file is edited.
inline float Sample(const std::vector<int16_t>& pcm,size_t frame,unsigned channel) {
    const size_t frames=pcm.size()/2,overlap=std::min<size_t>(144000,frames/4);
    float sample=float(pcm[frame*2+channel]);
    if(overlap&&frame>=frames-overlap) {
        const float t=float(frame-(frames-overlap))/float(overlap);
        sample=sample*std::cos(t*1.57079632679f)+float(pcm[(frame-(frames-overlap))*2+channel])*std::sin(t*1.57079632679f);
    }
    return sample;
}
inline size_t Next(size_t frame,size_t frames) {
    return ++frame<frames?frame:std::min<size_t>(144000,frames/4);
}
inline int16_t Output(float sample,float fade,float volume=.6f){return int16_t(std::clamp(sample*std::clamp(volume,0.f,1.f)*fade,-32768.f,32767.f));}
}
