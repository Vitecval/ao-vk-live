#include "preview23_artwork.h"
#include "building_families.h"
#include "terrain_materials.h"
#include <cstdio>
int main() {
    using namespace rvk::materials;
    for (size_t i=0;i<std::size(preview23CompletedWorldIds);++i) {
        const auto id=preview23CompletedWorldIds[i];
        if (i && preview23CompletedWorldIds[i-1]>=id) return 1;
        // Exceptions must affect only previously managed, resource-less world
        // IDs. Reviewed existing families retain their complete-map guard.
        if (!rvk::buildings::Managed(1010004,id) || rvk::buildings::ResourceOf(1010004,id)) return 2;
        if (!Preview23CompletedWorld(1010004,id) || !Preview23CompletedWorld(1010016,id) ||
            !Preview23CompletedWorld(1010017,id)) return 3;
        if (Preview23CompletedWorld(1010009,id) || Preview23CompletedWorld(1010011,id) ||
            Preview23CompletedWorld(1010006,id)) return 4;
    }
    for (auto id:{0u,5971u,6014u,774u}) if (Preview23CompletedWorld(1010004,id)) return 5;
    if (!VoidGround(1010006,774) || AuthoredGround(1010006,774)) return 6;
    std::printf("Preview23: %zu completed world exceptions; retained controls, reviewed families and terrain unchanged\n",std::size(preview23CompletedWorldIds));
    return 0;
}
