#include "material_profiles.h"
#include <cstdio>
int main()
{
    using rvk::materials::Lookup;
    auto skin = Lookup(1010011, 8763), steel = Lookup(1010004, 8731), cloth = Lookup(1010004, 9622);
    if (!skin || !steel || !cloth || skin >> 8 || cloth >> 8 || (steel >> 8) < 128) return 1;
    if (Lookup(1010019, 8763) != skin || Lookup(1010020, 8763) != skin) return 2;
    if (Lookup(1010016, 8731) != steel || Lookup(1010017, 8731) != steel) return 3;
    if (Lookup(1010006, 8731) || Lookup(0, 8731) || Lookup(1010004, 0)) return 4;
    if (Lookup(1010004, 15857)) return 5; // armour shop sign, not wearable armour
    const auto& c = rvk::materials::catalog;
    auto window = rvk::materials::LookupWorld(1010004,6055);
    if (window >> 24 != 3 || rvk::materials::LookupWorld(1010016,6055) != window) return 7;
    if (rvk::materials::LookupWorld(1010011,6055) || rvk::materials::LookupWorld(1010006,6055) ||
        rvk::materials::LookupWorld(1010004,256236)) return 8;
    const auto& world = rvk::materials::worldCatalog;
    for (uint32_t id : {206854u,210817u,205836u}) {
        const auto rock=rvk::materials::LookupWorld(1010004,id);
        if (rock>>24!=7 || (rock>>8&255)!=0 || (rock&255)<240) return 10;
    }
    const auto marble=rvk::materials::LookupWorld(1010004,209481);
    if (marble>>24!=8 || (marble>>8&255)!=0 || (marble&255)<220) return 11;
    for (size_t i=1;i<std::size(world);++i) if (world[i-1].key >= world[i].key) return 9;
    for (size_t i = 1; i < std::size(c); ++i) if (c[i - 1].key >= c[i].key) return 6;
    std::printf("material catalog: %zu entries; quality aliases, type isolation and non-metal skin/cloth passed\n", std::size(c));
    return 0;
}
