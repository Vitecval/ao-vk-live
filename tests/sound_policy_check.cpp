#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/sound_policy.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

int main(int argc,char** argv) {
    using namespace rvk::sound;
    assert(Select(false,true,152,false)==Bed::None);
    assert(Select(true,false,152,false)==Bed::None);
    assert(Select(true,true,152,false)==Bed::Grid);
    assert(Select(true,true,655,true)==Bed::City);
    assert(Select(true,true,640,true)==Bed::City);
    assert(Select(true,true,565,true)==Bed::Desert);
    assert(Select(true,true,505,true)==Bed::Forest);
    assert(Select(true,true,4321,true)==Bed::Cold);
    assert(Select(true,true,4005,true)==Bed::Inferno);
    assert(Select(true,true,4605,true)==Bed::Inferno);
    assert(Select(true,true,4321,false)==Bed::None);
    assert(Select(true,true,999999,true)==Bed::None);
    assert(Select(true,true,4873,true,true)==Bed::Underwater);
    assert(Select(false,true,4873,true,true)==Bed::None);
    assert(Select(true,false,4873,true,true)==Bed::None);
    assert(Select(true,true,4873,true,false)!=Bed::Underwater);
    assert(std::strcmp(File(Bed::Underwater),"underwater.wav")==0);
    assert(File(Bed::None)==nullptr);
    std::vector<uint8_t> wav(44+192000,0);
    auto u16=[&](size_t at,uint16_t n){wav[at]=uint8_t(n);wav[at+1]=uint8_t(n>>8);};
    auto u32=[&](size_t at,uint32_t n){for(unsigned c=0;c<4;++c)wav[at+c]=uint8_t(n>>(8*c));};
    std::memcpy(wav.data(),"RIFF",4);u32(4,uint32_t(wav.size()-8));std::memcpy(wav.data()+8,"WAVEfmt ",8);
    u32(16,16);u16(20,1);u16(22,2);u32(24,48000);u32(28,192000);u16(32,4);u16(34,16);std::memcpy(wav.data()+36,"data",4);u32(40,192000);
    std::vector<int16_t> pcm;
    assert(Decode(wav,pcm)&&pcm.size()==96000);
    auto bad=wav;bad.resize(50);assert(!Decode(bad,pcm)&&pcm.empty());
    bad=wav;bad[20]=3;assert(!Decode(bad,pcm));
    bad=wav;bad[22]=8;assert(!Decode(bad,pcm));
    bad=wav;bad[40]=255;assert(!Decode(bad,pcm));
    pcm.assign(2304000*2,1000);
    assert(Next(2303999,2304000)==144000);
    assert(std::abs(Sample(pcm,2303999,0)-Sample(pcm,144000,0))<1);
    assert(Output(32767,0)==0&&Output(1000,1)==600);
    assert(Output(100000,1)==32767&&Output(-100000,1)==-32768);
    assert(Output(1000,1,0)==0&&Output(1000,1,.25f)==250&&Output(1000,1,2)==1000);
    for(int i=1;i<argc;++i) {
        std::ifstream file(std::filesystem::path(argv[i]),std::ios::binary|std::ios::ate);assert(file);
        const size_t size=size_t(file.tellg());std::vector<uint8_t> bytes(size);file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(size));
        assert(file&&Decode(bytes,pcm));
        assert(pcm.size()==size_t(U32(bytes.data()+40))/2); // Delivery helper writes a canonical PCM header; retain each recording's real duration.
        std::printf("Validated runtime PCM: %s (%zu stereo frames)\n",argv[i],pcm.size()/2);
    }
    std::puts("Sound policy passed: off/login/unsupported retain original; supported regions, bounded PCM, loop overlap and software gain validated. No audio device opened.");
}
