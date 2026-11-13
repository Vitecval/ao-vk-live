#include "../rvk/huge_shadow_policy.h"
#include <cstdio>

int main() {
    const float origin[3] = {}, sun[3] = {.6f,-.7f,.3f};
    const float huge[3] = {60,70,50}, house[3] = {12,9,12}, person[3] = {1,2,1};
    const float pole[3] = {2,80,2}, broadLow[3] = {80,3,80}, warehouse[3] = {50,12,45};
    if (!rvk::shadow::Huge(origin,huge,sun) || !rvk::shadow::Huge(origin,warehouse,sun)) return 1;
    for (const auto* box : {house,person,pole,broadLow}) if (rvk::shadow::Huge(origin,box,sun)) return 2;
    const float tower[3] = {20,30,12}, overhead[3] = {0,-1,0}, lowSun[3] = {1,-.1f,.3f};
    if (rvk::shadow::Huge(origin,tower,overhead) || !rvk::shadow::Huge(origin,tower,lowSun)) return 3;
    rvk::shadow::HugeGroups groups;
    const float leftLo[3]={0,0,0}, leftHi[3]={30,40,30};
    const float rightLo[3]={30,0,0}, rightHi[3]={60,40,30};
    groups.Observe(1,leftLo,leftHi,overhead);
    if (groups.Excluded(1)) return 4;
    groups.Observe(1,rightLo,rightHi,overhead);
    if (!groups.Excluded(1)) return 5;
    for (int frame=0;frame<120;++frame) {
        groups.Observe(1,origin,house,overhead);
        groups.Observe(2,origin,house,lowSun);
        if (!groups.Excluded(1) || groups.Excluded(2)) return 6;
    }
    groups.Observe(3,origin,tower,lowSun);
    groups.Observe(3,origin,tower,overhead);
    if (!groups.Excluded(3)) return 7;
    groups.Clear();
    if (groups.Size() || groups.Excluded(1)) return 8;
    std::puts("huge-shadow decisions survive partial views, LOD and sun changes; separate instances and zone reset stay independent");
    std::puts("huge-shadow policy rejects massive buildings and long broad projections, retains normal objects and thin poles");
}
