#pragma once
#include <algorithm>

namespace rnative::rings {
// N3's own five rings, bounded by the zones it has loaded. Small-object
// detail and large scenery have independent budgets; actors are not statels.
inline void ScaledFactors(const float (&saved)[5], float kept, float detail,
                          float scenery, float (&out)[5])
{
    const float limit=kept*2.f;
    for(int i=0;i<5;++i) {
        float wanted=saved[i]*(i<2 ? detail:scenery);
        if(i>0)wanted=(std::max)(wanted,out[i-1]);
        out[i]=limit>0.f ? (std::max)(saved[i],(std::min)(wanted,limit)):wanted;
    }
}
}
