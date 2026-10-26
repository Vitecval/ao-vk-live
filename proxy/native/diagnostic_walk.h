// Bounded traversal for optional diagnostics of live game-owned frame links.
#pragma once
#include <array>
#include <cstddef>

namespace rnative::diagnostics {
struct WalkBudget {
    std::array<const void*,512> seen{};
    size_t count=0;
    bool truncated=false;
    bool Take(const void* node) {
        for (size_t i=0;i<count;++i) if (seen[i]==node) { truncated=true; return false; }
        if (count==seen.size()) { truncated=true; return false; }
        seen[count++]=node;
        return true;
    }
};

template<class First,class Next,class Visit>
bool WalkChildren(const void* root,int depth,WalkBudget& budget,First first,Next next,Visit visit)
{
    if (depth>=3) return true;
    for (const void* node=first(root);node;node=next(node)) {
        if (!budget.Take(node)) return false;
        visit(node,depth);
        if (!WalkChildren(node,depth+1,budget,first,next,visit)) return false;
    }
    return true;
}
}
