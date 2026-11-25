#pragma once
// Shared display decisions, independent of Win32 callbacks or Vulkan objects.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace rvk::display {
constexpr size_t kNativeCapacity=128;
enum class WindowMode { Game,Windowed,Borderless };
struct Options {
    WindowMode mode=WindowMode::Game;
    uint32_t width=0,height=0;
    std::wstring monitor;
    bool valid=true;
    std::string error;
};
inline bool HasOverride(const Options& options) {
    return options.mode!=WindowMode::Game || options.width || !options.monitor.empty();
}
struct Mode { uint32_t width=0,height=0,bits=32,refresh=0; };
struct Limits {
    uint32_t width=65535,height=65535;
    uint64_t pixels=std::numeric_limits<uint32_t>::max()/4u;
};
inline bool Dimensions(uint32_t w,uint32_t h,Limits limits={}) {
    return w>=32 && h>=32 && w<=65535 && h<=65535 && w<=limits.width && h<=limits.height &&
        uint64_t(w)*h<=limits.pixels;
}
inline bool Supported(const Mode& m,Limits limits={}) {
    return Dimensions(m.width,m.height,limits) && (m.bits==16 || m.bits==32);
}
struct Filter {
    bool width=false,height=false,bits=false,refresh=false;
    Mode wanted;
};
inline bool Matches(const Mode& m,const Filter& f) {
    return (!f.width || m.width==f.wanted.width) && (!f.height || m.height==f.wanted.height) &&
        (!f.bits || m.bits==f.wanted.bits) && (!f.refresh || m.refresh==f.wanted.refresh);
}
inline bool Same(const Mode& a,const Mode& b,bool refresh) {
    return a.width==b.width && a.height==b.height && a.bits==b.bits && (!refresh || a.refresh==b.refresh);
}
// Preserve the current resolution, then 32-bit high-resolution choices when AO
// exhausts its fixed ABI array. No refresh duplicates consume that array.
inline bool Preferred(const Mode& a,const Mode& b,const Mode& current) {
    auto common=[](const Mode& m){
        const uint32_t sizes[][2]={{640,480},{800,600},{1024,768},{1280,720},{1920,1080},{2560,1440},
            {3840,2160},{2560,1080},{3440,1440},{5120,1440},{5120,2160},{7680,2160},{7680,4320}};
        for(const auto& size:sizes)if(m.width==size[0] && m.height==size[1])return true;
        return false;
    };
    auto priority=[&](const Mode& m){return std::make_tuple(
        m.width==current.width && m.height==current.height,common(m),m.bits==32,uint64_t(m.width)*m.height,m.width,m.height,m.refresh);};
    return priority(a)>priority(b);
}
inline std::vector<Mode> Enumerate(const std::vector<Mode>& physical,const Mode& current,
    const Filter& filter={},bool refreshRates=false,Limits limits={},size_t capacity=0)
{
    std::vector<Mode> out;
    auto add=[&](Mode m){
        if(!Supported(m,limits) || !Matches(m,filter))return;
        auto found=std::find_if(out.begin(),out.end(),[&](const Mode& p){return Same(p,m,refreshRates);});
        if(found==out.end())out.push_back(m);
        else if(!refreshRates && (m.refresh==current.refresh || (found->refresh!=current.refresh && m.refresh>found->refresh)))
            found->refresh=m.refresh;
    };
    // Modern desktops are 32-bit, while AO's 16-bit surface format is emulated
    // by Vulkan. Dimensions and refresh remain actual monitor capabilities.
    auto addPhysical=[&](const Mode& m){for(uint32_t bits:{16u,32u})add({m.width,m.height,bits,m.refresh});};
    for(const Mode& m:physical)addPhysical(m);
    addPhysical(current); // same filters/limits apply; no desktop bypass
    if(physical.empty()) {
        const Mode fallback[]={{640,480},{800,600},{1024,768},{1280,720},{1920,1080}};
        for(Mode m:fallback)if(m.width<=current.width && m.height<=current.height) {
            m.refresh=current.refresh;addPhysical(m);
        }
    }
    if(capacity && out.size()>capacity) {
        std::stable_sort(out.begin(),out.end(),[&](const Mode& a,const Mode& b){return Preferred(a,b,current);});
        out.resize(capacity);
    }
    std::sort(out.begin(),out.end(),[](const Mode& a,const Mode& b){return
        std::make_tuple(uint64_t(a.width)*a.height,a.width,a.height,a.bits,a.refresh)<
        std::make_tuple(uint64_t(b.width)*b.height,b.width,b.height,b.bits,b.refresh);});
    return out;
}
// Native callback insertion, also safe when an unmodified backend returns
// duplicate refreshes or >16-bit dimensions. Never alter DeviceDesc's ABI.
template<class NativeMode>
inline void InsertNative(NativeMode* modes,uint8_t& count,const Mode& candidate,const Mode& current={}) {
    if(!Supported(candidate))return;
    for(unsigned i=0;i<count;++i)
        if(modes[i].width==candidate.width && modes[i].height==candidate.height && modes[i].bits==candidate.bits)return;
    unsigned slot=count;
    if(count>=kNativeCapacity) {
        slot=0;
        for(unsigned i=1;i<count;++i) {
            Mode a{modes[slot].width,modes[slot].height,modes[slot].bits,0};
            Mode b{modes[i].width,modes[i].height,modes[i].bits,0};
            if(Preferred(a,b,current))slot=i;
        }
        Mode worst{modes[slot].width,modes[slot].height,modes[slot].bits,0};
        if(!Preferred(candidate,worst,current))return;
    } else ++count;
    modes[slot]={uint16_t(candidate.width),uint16_t(candidate.height),uint8_t(candidate.bits),0};
}
struct Selection {
    uint32_t width,height;
    bool overridden=false,rejected=false;
    const char* reason="game selection retained";
};
inline Selection SelectSize(const Options& options,uint32_t gameWidth,uint32_t gameHeight,
    const std::vector<Mode>& modes,Limits limits={})
{
    Selection out{gameWidth,gameHeight};
    if(!options.valid) {out.rejected=true;out.reason="invalid Display configuration";return out;}
    if(!options.width && !options.height)return out;
    if(!Dimensions(options.width,options.height,limits)) {
        out.rejected=true;out.reason="requested size exceeds safe display/GPU limits";return out;
    }
    if(std::none_of(modes.begin(),modes.end(),[&](const Mode& m){return m.width==options.width && m.height==options.height;})) {
        out.rejected=true;out.reason="requested size is not supported on the selected monitor";return out;
    }
    out.width=options.width;out.height=options.height;out.overridden=true;out.reason="installation Display override";
    return out;
}
} // namespace rvk::display
