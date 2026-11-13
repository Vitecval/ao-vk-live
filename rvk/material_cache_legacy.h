// Read-only compatibility with the previously installed v3 prepared cache.
// Prepared material mip chains. Shared by the offline preparer and the game worker.
// This is a disposable local cache: every source stamp and decode option is keyed.
#pragma once
#include "material_maps.h"
#include <filesystem>
#include <fstream>
#include <array>
#include <cctype>
#include <system_error>
#include <cstdio>

namespace rvk::maps::legacy {
namespace fs = std::filesystem;
constexpr uint32_t Version = 3; // Bump when Decode/filtering, checksum or the binary layout changes.
constexpr uint64_t Limit = 64ull << 20;
inline uint64_t Hash(const void* bytes, size_t size, uint64_t h = 14695981039346656037ull) {
    const auto* p = static_cast<const unsigned char*>(bytes);
    // Two independent 32-bit lanes keep checksums cheap in AO's x86 process.
    // This detects damaged disposable cache data; it is not a security digest.
    uint32_t a=uint32_t(h),b=uint32_t(h>>32);
    while(size>=8){
        uint32_t x,y;std::memcpy(&x,p,4);std::memcpy(&y,p+4,4);
        a=(a^x)*16777619u;a=(a<<13)|(a>>19);
        b=(b^y)*2246822519u;b=(b<<17)|(b>>15);
        p+=8;size-=8;
    }
    while(size--){a=(a^*p++)*16777619u;b=(b^a)*2246822519u;}
    return uint64_t(a)|(uint64_t(b)<<32);
}
inline uint64_t Key(const MaterialFiles& f) {
    uint64_t h = Hash(&Version, sizeof(Version));
    const uint32_t options[] = {f.maxSize, f.maxAlbedoSize, f.maxAlbedoPixels, uint32_t(f.preserveAlbedoAlpha)};
    h = Hash(options, sizeof(options), h);
    for (const auto* path : {&f.normal,&f.height,&f.packed,&f.parts[0],&f.parts[1],&f.parts[2],
                             &f.albedo,&f.emissive,&f.leaves,&f.leavesNormal}) {
        std::string name;
        uint64_t stamp[2]{};
        if (!path->empty()) {
            std::error_code ec;
            auto absolute = fs::absolute(fs::path(*path), ec);
            if (ec) return 0;
            name = absolute.lexically_normal().generic_string();
            for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
            stamp[0] = fs::file_size(absolute, ec); if (ec) return 0;
            stamp[1] = uint64_t(fs::last_write_time(absolute, ec).time_since_epoch().count()); if (ec) return 0;
        }
        h = Hash(name.c_str(), name.size()+1, h);
        h = Hash(stamp, sizeof(stamp), h);
    }
    return h ? h : 1;
}
inline fs::path Path(const fs::path& root, uint64_t key) {
    char name[32]; std::snprintf(name, sizeof(name), "%016llx.rvkm", static_cast<unsigned long long>(key));
    return root / name;
}
struct ChainInfo { uint32_t w, h, levels; };
struct Header {
    uint32_t magic = 0x4d4b5652, version = Version;
    uint64_t key = 0, bytes = 0, checksum = 0;
    uint32_t height = 0, alpha = 0;
    ChainInfo chains[6]{};
};
inline std::array<MipChain*,6> Chains(Decoded& d) { return {&d.normal,&d.orm,&d.albedo,&d.emissive,&d.leaves,&d.leavesNormal}; }
inline bool Load(const fs::path& root, const MaterialFiles& f, Decoded& result) {
    const uint64_t key = Key(f); if (!key) return false;
    std::ifstream in(Path(root,key), std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = in.tellg();
    if (size < std::streamoff(sizeof(Header)) || size > std::streamoff(Limit + sizeof(Header))) return false;
    in.seekg(0); Header h;
    if (!in.read(reinterpret_cast<char*>(&h), sizeof(h)) || h.magic != 0x4d4b5652 || h.version != Version ||
        h.key != key || h.bytes > Limit || uint64_t(size) != sizeof(Header)+h.bytes || h.height > 1 || h.alpha > 1 ||
        bool(h.alpha) != f.preserveAlbedoAlpha) return false;
    uint64_t bytes = 0;
    for (const auto& c : h.chains) {
        if (!c.levels) { if (c.w || c.h) return false; continue; }
        if (!c.w || !c.h || c.w > 2048 || c.h > 2048 || c.levels != LevelCount(c.w,c.h)) return false;
        for (uint32_t w=c.w,y=c.h,l=0;l<c.levels;++l,w=std::max(w/2,1u),y=std::max(y/2,1u)) bytes += uint64_t(w)*y*4;
    }
    if (bytes != h.bytes || (h.height && !h.chains[0].levels)) return false;
    Decoded d; auto chains = Chains(d); uint64_t checksum = 14695981039346656037ull;
    for (size_t i=0;i<chains.size();++i) {
        auto& c=*chains[i]; c.w=h.chains[i].w; c.h=h.chains[i].h;
        uint32_t w=c.w,y=c.h;
        for (uint32_t l=0;l<h.chains[i].levels;++l,w=std::max(w/2,1u),y=std::max(y/2,1u)) {
            c.levels.emplace_back(size_t(w)*y);
            auto& level=c.levels.back();
            if (!in.read(reinterpret_cast<char*>(level.data()), std::streamsize(level.size()*4))) return false;
            checksum=Hash(level.data(),level.size()*4,checksum);
        }
    }
    if (checksum != h.checksum) return false;
    d.height=h.height!=0; d.preserveAlbedoAlpha=h.alpha!=0;
    d.ormFrom=f.packed.empty() ? "prepared separate controls" : f.packed;
    result=std::move(d); return true;
}
}
