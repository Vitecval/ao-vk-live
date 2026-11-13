// Lossless prepared mip chains, shared by the offline tool and the game worker.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <compressapi.h>
#include "material_maps.h"
#include <filesystem>
#include <fstream>
#include <array>
#include <cctype>
#include <system_error>
#include <cstdio>
#include <unordered_map>
#include <sstream>

namespace rvk::maps::prepared {
namespace fs=std::filesystem;
constexpr uint32_t Version=4, MaxLevels=12, Compressed=0x80000000u;
constexpr uint64_t Limit=64ull<<20;
inline uint64_t Hash(const void* bytes,size_t size,uint64_t h=14695981039346656037ull) {
    const auto* p=static_cast<const unsigned char*>(bytes);uint32_t a=uint32_t(h),b=uint32_t(h>>32);
    while(size>=8){uint32_t x,y;std::memcpy(&x,p,4);std::memcpy(&y,p+4,4);a=(a^x)*16777619u;a=(a<<13)|(a>>19);b=(b^y)*2246822519u;b=(b<<17)|(b>>15);p+=8;size-=8;}
    while(size--){a=(a^*p++)*16777619u;b=(b^a)*2246822519u;}
    return uint64_t(a)|(uint64_t(b)<<32);
}
// A single prepared set serves every smaller resolution and alpha policy.
// These options never change how Decode builds the pixels before dropping mips.
inline uint64_t Key(const MaterialFiles& f) {
    uint64_t h=Hash(&Version,sizeof(Version));
    for(const auto* path:{&f.normal,&f.height,&f.packed,&f.parts[0],&f.parts[1],&f.parts[2],&f.albedo,&f.emissive,&f.leaves,&f.leavesNormal}) {
        std::string name;uint64_t stamp[2]{};
        if(!path->empty()) {
            std::error_code ec;auto absolute=fs::absolute(fs::path(*path),ec);if(ec)return 0;
            name=absolute.lexically_normal().generic_string();
            for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
            stamp[0]=fs::file_size(absolute,ec);if(ec)return 0;
            stamp[1]=uint64_t(fs::last_write_time(absolute,ec).time_since_epoch().count());if(ec)return 0;
        }
        h=Hash(name.c_str(),name.size()+1,h);h=Hash(stamp,sizeof(stamp),h);
    }
    return h?h:1;
}
inline fs::path Path(const fs::path& root,uint64_t key) {
    char name[32];std::snprintf(name,sizeof(name),"%016llx.rvkm",static_cast<unsigned long long>(key));return root/name;
}
struct ChainInfo {uint32_t w,h,levels;};
// Setup binds portable recipes to the exact source timestamps committed with
// them. Key(f) checks today's files, so subsequent edits invalidate bindings.
inline uint64_t BoundKey(const fs::path& root,uint64_t key) {
    struct Index {fs::path root;fs::file_time_type stamp{};bool ready=false;std::unordered_map<uint64_t,uint64_t> keys;};
    static thread_local Index index;
    std::error_code ec;const auto file=root/"bindings.tsv";auto stamp=fs::last_write_time(file,ec);
    if(ec)return 0;
    if(!index.ready||index.root!=root||index.stamp!=stamp){
        index={};index.root=root;index.stamp=stamp;index.ready=true;
        auto bytes=fs::file_size(file,ec);if(ec||bytes>(32u<<20))return 0;
        std::ifstream in(file);std::string line;
        if(!std::getline(in,line)||line!="RVK-BINDINGS-1")return 0;
        const auto client=root.parent_path().parent_path().parent_path();
        unsigned records=0;
        try {while(std::getline(in,line)){
            if(++records>30000||line.size()>20000)throw std::runtime_error("limit");
            std::istringstream row(line);std::string token;
            if(!std::getline(row,token,'\t')||token.size()!=16)throw std::runtime_error("key");
            const uint64_t stored=std::stoull(token,nullptr,16);uint64_t h=Hash(&Version,sizeof(Version));
            for(unsigned i=0;i<10;++i){
                std::string name,size,time;
                if(!std::getline(row,name,'\t')||!std::getline(row,size,'\t')||!std::getline(row,time,'\t'))throw std::runtime_error("record");
                uint64_t stamps[2]={std::stoull(size),std::stoull(time)};
                if(!name.empty()){
                    if(name.rfind("randy-vk/packs/",0)!=0||name.find("..")!=std::string::npos||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos||name.size()>512)throw std::runtime_error("path");
                    name=fs::absolute(client/fs::u8path(name)).lexically_normal().generic_string();
                    for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
                }else if(stamps[0]||stamps[1])throw std::runtime_error("empty");
                h=Hash(name.c_str(),name.size()+1,h);h=Hash(stamps,sizeof(stamps),h);
            }
            if(row.rdbuf()->in_avail()!=0)throw std::runtime_error("extra");
            auto inserted=index.keys.emplace(h?h:1,stored);
            if(!inserted.second&&inserted.first->second!=stored)throw std::runtime_error("duplicate");
        }}catch(...){index.keys.clear();return 0;}
    }
    auto found=index.keys.find(key);return found==index.keys.end()?0:found->second;
}
struct Header {
    uint32_t magic=0x4d4b5652,version=Version;
    uint64_t key=0,storedBytes=0,decodedBytes=0,checksum=0;
    uint32_t maxSize=0,maxAlbedoSize=0,maxAlbedoPixels=0,height=0;
    ChainInfo chains[6]{};
    uint32_t sizes[6][MaxLevels]{};
};
inline std::array<MipChain*,6> Chains(Decoded& d){return {&d.normal,&d.orm,&d.albedo,&d.emissive,&d.leaves,&d.leavesNormal};}
struct Codec {
    COMPRESSOR_HANDLE handle=nullptr;bool decode=false;
    explicit Codec(bool d):decode(d){}
    bool Open(){return handle || (decode?CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle):CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF,nullptr,&handle));}
    ~Codec(){if(handle){if(decode)CloseDecompressor(handle);else CloseCompressor(handle);}}
};
inline uint64_t Area(uint32_t side,uint32_t pixels){return std::min(uint64_t(side)*side,pixels?uint64_t(pixels):UINT64_MAX);}
inline bool Load(const fs::path& root,const MaterialFiles& f,Decoded& result) {
    uint64_t key=Key(f);if(!key)return false;
    std::ifstream in(Path(root,key),std::ios::binary|std::ios::ate);
    if(!in){key=BoundKey(root,key);if(!key)return false;in.clear();in.open(Path(root,key),std::ios::binary|std::ios::ate);}
    if(!in)return false;
    const auto size=in.tellg();if(size<std::streamoff(sizeof(Header))||size>std::streamoff(Limit+sizeof(Header)))return false;
    in.seekg(0);Header h{};
    if(!in.read(reinterpret_cast<char*>(&h),sizeof(h))||h.magic!=0x4d4b5652||h.version!=Version||h.key!=key||
       h.storedBytes>Limit||h.decodedBytes>Limit||uint64_t(size)!=sizeof(Header)+h.storedBytes||h.height>1||
       !h.maxSize||h.maxSize>2048||!h.maxAlbedoSize||h.maxAlbedoSize>2048)return false;
    const uint32_t albedoCap=f.maxAlbedoSize?f.maxAlbedoSize:f.maxSize;
    if(h.maxSize<f.maxSize||h.maxAlbedoSize<albedoCap||Area(h.maxAlbedoSize,h.maxAlbedoPixels)<Area(albedoCap,f.maxAlbedoPixels))return false;
    uint64_t bytes=0,stored=0;
    for(unsigned i=0;i<6;++i){const auto& c=h.chains[i];
        if(!c.levels){if(c.w||c.h)return false;}else if(!c.w||!c.h||c.w>2048||c.h>2048||c.levels!=LevelCount(c.w,c.h)||c.levels>MaxLevels)return false;
        uint32_t w=c.w,y=c.h;
        for(unsigned l=0;l<MaxLevels;++l){
            const uint32_t encoded=h.sizes[i][l],n=encoded&~Compressed;
            if(l>=c.levels){if(encoded)return false;continue;}
            const uint64_t raw=uint64_t(w)*y*4;
            if(!n||n>raw||(!(encoded&Compressed)&&n!=raw))return false;
            bytes+=raw;stored+=n;w=std::max(w/2,1u);y=std::max(y/2,1u);
        }
    }
    if(bytes!=h.decodedBytes||stored!=h.storedBytes||(h.height&&!h.chains[0].levels))return false;
    const uint64_t expected=h.checksum;h.checksum=0;uint64_t checksum=Hash(&h,sizeof(h));
    Decoded d;auto chains=Chains(d);Codec codec(true);std::vector<uint8_t> packed;
    for(unsigned i=0;i<6;++i){auto& c=*chains[i];c.w=h.chains[i].w;c.h=h.chains[i].h;uint32_t w=c.w,y=c.h;
        for(unsigned l=0;l<h.chains[i].levels;++l,w=std::max(w/2,1u),y=std::max(y/2,1u)){
            c.levels.emplace_back(size_t(w)*y);auto& level=c.levels.back();const size_t raw=level.size()*4;
            const uint32_t encoded=h.sizes[i][l],n=encoded&~Compressed;
            if(encoded&Compressed){
                if(!codec.Open())return false;packed.resize(n);
                if(!in.read(reinterpret_cast<char*>(packed.data()),n))return false;
                SIZE_T written=0;
                if(!Decompress(codec.handle,packed.data(),n,level.data(),raw,&written)||written!=raw)return false;
            }else if(!in.read(reinterpret_cast<char*>(level.data()),raw))return false;
            checksum=Hash(level.data(),raw,checksum);
        }
    }
    if(checksum!=expected)return false;
    // Dropping filtered levels preserves normal/roughness coupling, including
    // the existing low-address-space 256-pixel fallback.
    for(unsigned i=0;i<6;++i){std::string note;CapChain(*chains[i],&note,i==2?albedoCap:f.maxSize);}
    while(f.maxAlbedoPixels&&uint64_t(d.albedo.w)*d.albedo.h>f.maxAlbedoPixels&&d.albedo.levels.size()>1){
        d.albedo.levels.erase(d.albedo.levels.begin());d.albedo.w=std::max(d.albedo.w/2,1u);d.albedo.h=std::max(d.albedo.h/2,1u);
    }
    d.height=h.height!=0;d.preserveAlbedoAlpha=f.preserveAlbedoAlpha;d.ormFrom=f.packed.empty()?"prepared separate controls":f.packed;
    result=std::move(d);return true;
}
inline bool Save(const fs::path& root,const MaterialFiles& f,Decoded& d) {
    if(d.decodeFailed||!d.Bytes()||d.Bytes()>Limit)return false;
    Header h{};h.key=Key(f);if(!h.key)return false;
    h.height=d.height;h.decodedBytes=d.Bytes();h.maxSize=f.maxSize;h.maxAlbedoSize=f.maxAlbedoSize?f.maxAlbedoSize:f.maxSize;h.maxAlbedoPixels=f.maxAlbedoPixels;
    auto chains=Chains(d);
    for(unsigned i=0;i<6;++i){const auto& c=*chains[i];if(c.levels.size()>MaxLevels)return false;h.chains[i]={c.w,c.h,uint32_t(c.levels.size())};}
    std::error_code ec;fs::create_directories(root,ec);if(ec)return false;
    auto destination=Path(root,h.key),temporary=destination;temporary+=".tmp";
    Codec codec(false);std::vector<uint8_t> packed;
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out.write(reinterpret_cast<const char*>(&h),sizeof(h)))return false;
        for(unsigned i=0;i<6;++i)for(unsigned l=0;l<chains[i]->levels.size();++l){
            const auto& level=chains[i]->levels[l];const uint32_t raw=uint32_t(level.size()*4);const void* data=level.data();uint32_t n=raw,flag=0;
            if(raw>=4096&&codec.Open()){
                packed.resize(raw);SIZE_T written=0;
                if(Compress(codec.handle,level.data(),raw,packed.data(),raw,&written)&&written+64<raw){data=packed.data();n=uint32_t(written);flag=Compressed;}
            }
            h.sizes[i][l]=n|flag;h.storedBytes+=n;
            if(!out.write(static_cast<const char*>(data),n))return false;
        }
        h.checksum=Hash(&h,sizeof(h));
        for(const auto* c:chains)for(const auto& level:c->levels)h.checksum=Hash(level.data(),level.size()*4,h.checksum);
        out.seekp(0);out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.close();if(!out)return false;
    }
    fs::rename(temporary,destination,ec);
    if(ec){fs::remove(destination,ec);ec.clear();fs::rename(temporary,destination,ec);}
    if(ec){std::error_code ignored;fs::remove(temporary,ignored);return false;}return true;
}
}
