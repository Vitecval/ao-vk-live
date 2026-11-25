#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/caustic_policy.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

int main()
{
    using namespace rvk::caustics;
    for(uint32_t id=226088;id<=226103;++id) {
        assert(Receiver(1010004,id,true,true,true,false,true));
        // Screen overlays, physical material passes and other depth/blend
        // roles cannot acquire the special decal depth or brightness policy.
        assert(!Receiver(1010004,id,false,true,true,false,true));
        assert(!Receiver(1010004,id,true,false,true,false,true));
        assert(!Receiver(1010004,id,true,true,false,false,true));
        assert(!Receiver(1010004,id,true,true,true,true,true));
        assert(!Receiver(1010004,id,true,true,true,false,false));
        assert(!Receiver(1010006,id,true,true,true,false,true));
    }
    for(uint32_t id : {226087u,226104u,473u,474u,258412u,82934u})
        assert(!Receiver(1010004,id,true,true,true,false,true));
    assert(kReceiverTolerance>0.f && kReceiverTolerance<=.001f);
    assert(kReceiverGain>0.f && kReceiverGain<.2f);
    assert(Gain(true,true,0.f)==0.f);
    assert(Gain(true,true,1.f)==.16f);
    assert(Gain(true,true,2.f)==.32f);
    assert(Gain(true,true,-1.f)==0.f && Gain(true,true,100.f)==.32f);
    assert(Gain(true,false,0.f)==1.f && Gain(false,true,1.f)==1.f);
    assert(Gain(true,true,std::numeric_limits<float>::quiet_NaN())==0.f);
    std::puts("Caustic light fields only: sixteen frames, physical/depth/blend negative controls, bounded receiver depth and gain passed");
}
