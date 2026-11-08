#pragma once
#include <algorithm>

namespace rnative::rings {
// N3's own five rings, bounded by the zones it has loaded. Small-object
// detail and large scenery have independent budgets; actors are not statels.
inline void ScaledFactors(const float (&saved)[5], float kept, float detail,
                          float scenery, float (&out)[5], float farScale=1.f)
{
    const float limit=kept*2.f;
    // Do not expand beyond an unknown loading boundary. A far setting extends
    // small props by at most 2x; large scenery can use the full 3x range.
    const float reach=kept>0.f ? (std::clamp)(farScale,1.f,3.f):1.f;
    for(int i=0;i<5;++i) {
        float wanted=saved[i]*(i<2 ? detail*(std::min)(reach,2.f):(std::max)(scenery,reach));
        if(i>0)wanted=(std::max)(wanted,out[i-1]);
        out[i]=limit>0.f ? (std::max)(saved[i],(std::min)(wanted,limit)):wanted;
    }
}
}
