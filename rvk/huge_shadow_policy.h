#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace rvk::shadow {
inline bool Huge(const float lo[3], const float hi[3], const float light[3]) {
    const float x = hi[0]-lo[0], y = hi[1]-lo[1], z = hi[2]-lo[2];
    if (!(x > 0 && z > 0 && y >= 10)) return false;
    const float footprint = x*z, width = std::max(x,z);
    const float elevation = std::max(std::fabs(light[1]), .05f);
    const float projected = (x+y*std::fabs(light[0])/elevation)*(z+y*std::fabs(light[2])/elevation);
    return (footprint >= 1600 && width >= 40) || (y >= 18 && width >= 18 && footprint >= 120 && projected >= 2500);
}

// Build an instance's bounds from everything learned in this playfield, not just
// the subset submitted for the current camera. Once huge, keep that decision
// until the playfield changes; LOD and small sun-angle changes must not toggle it.
class HugeGroups {
    struct Bounds {
        float lo[3]={1e30f,1e30f,1e30f}, hi[3]={-1e30f,-1e30f,-1e30f};
        bool huge=false;
    };
    std::unordered_map<uint64_t,Bounds> groups;
public:
    void Clear() { groups.clear(); }
    size_t Size() const { return groups.size(); }
    void Observe(uint64_t key,const float lo[3],const float hi[3],const float light[3]) {
        auto it=groups.find(key);
        if (it==groups.end()) {
            if (groups.size()>=32768) return;
            it=groups.emplace(key,Bounds{}).first;
        }
        auto& b=it->second;
        for (int j=0;j<3;++j) { b.lo[j]=std::min(b.lo[j],lo[j]); b.hi[j]=std::max(b.hi[j],hi[j]); }
        b.huge=b.huge || Huge(b.lo,b.hi,light);
    }
    bool Excluded(uint64_t key) const {
        const auto it=groups.find(key);
        return it!=groups.end() && it->second.huge;
    }
};
}
