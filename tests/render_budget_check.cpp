#include "render_budget.h"
#include "material_profiles.h"
#include "area_profiles.h"
#include "ambient_policy.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rvk::budget;
    for(uint32_t id:{5955u,9863u,205836u,214375u,205675u,205838u,202640u}) {
        assert(rvk::materials::NaturalRock(1010004,id));
        assert(rvk::materials::NaturalRock(1010016,id));
        assert(!rvk::materials::NaturalRock(1010011,id));
    }
    assert(!rvk::materials::NaturalRock(1010004,0));
    assert(SmokeCells(128,0,8)==128);
    assert(SmokeCells(128,1.5f,8)==48);
    assert(SmokeCells(128,1.5f,50)==32);
    assert(SmokeCells(32,1.5f,8)==32);
    assert(SmokeScale(5,50,1)>5);
    assert(SmokeScale(8,0,1)>=.1f);
    assert(!MaterialCold(400,300,0));
    assert(!MaterialCold(400,1,390));
    assert(MaterialCold(400,1,100));
    const uint64_t heaps[]={8ull<<30,16ull<<30,214ull<<20};
    const bool local[]={true,false,true};
    assert(MaterialHeap(heaps,local,3)==0);
    assert(MaterialExcess(3465ull<<20,7221ull<<20)==0); // real frame: main heap healthy
    assert(MaterialExcess(214ull<<20,171ull<<20)>0);   // BAR pressure must not drive image eviction
    assert(MaterialExcess(7000ull<<20,7221ull<<20)>0);
    assert(MaterialExcess(4977ull<<20,5691ull<<20)==0); // flying-session churn with >700 MiB headroom
    assert(MaterialExcess(5300ull<<20,5691ull<<20)>0); // actual pressure still trims
    assert(MaterialExcess(5364ull<<20,5674ull<<20,5295ull<<20,2597ull<<20)==0); // captured flight: reusable blocks
    assert(MaterialExcess(5684ull<<20,5682ull<<20,5615ull<<20,3367ull<<20)==0); // looking-up capture
    assert(MaterialExcess(7000ull<<20,7221ull<<20,6900ull<<20,6800ull<<20)>0); // actual live pressure
    assert(MaterialExcess(100,100,1,2)==12); // inconsistent statistics cannot underflow
    assert(MaterialExcess(100,0,200,0)==0);
    const uint64_t uma[]={16ull<<30};const bool unified[]={true};
    assert(MaterialHeap(uma,unified,1)==0);
    using namespace rvk::ambient;
    assert(Frequency(true,false,true,0)==Frequency(false,false,true,0));
    assert(Kind(655,0x808080,.5f)==0.f && Kind(655,0x3C6A2A,.5f)==9.f);
    assert(Kind(655,0x254A90,.5f)==5.f && Kind(655,0x903521,.5f)==7.f);
    assert(Kind(4314,0x808080,.5f)==5.f && Kind(4314,0x808080,.9f)==9.f);
    assert(Kind(4320,0,.5f)==6.f && Kind(4605,0,.5f)==7.f && Kind(4900,0,.5f)==8.f);
    const auto a=rvk::areas::AmbientLook(4310),b=rvk::areas::AmbientLook(4311);
    assert(a.colour[2]!=b.colour[2] && a.speed!=b.speed);
    std::puts("Render budgets: bounded smoke grid, uncapped calibration, active-map protection and regional ambience passed");
}
