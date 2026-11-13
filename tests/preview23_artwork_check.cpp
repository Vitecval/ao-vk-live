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
    for (size_t i=0;i<std::size(preview23CompletedGroundIds);++i) {
        const auto id=preview23CompletedGroundIds[i];
        if ((i && preview23CompletedGroundIds[i-1]>=id) || id==27 || id==774) return 7;
        for (auto type:{1010006u,1010021u,1010022u}) if (!Preview23CompletedGround(type,id)) return 8;
        for (auto type:{1010004u,1010016u,1010017u,1010009u,1010011u})
            if (Preview23CompletedGround(type,id)) return 9;
    }
    for (auto type:{1010006u,1010021u,1010022u})
        for (auto id:{0u,27u,774u}) if (Preview23CompletedGround(type,id)) return 10;
    if (!DeckGround(1010006,17)) return 11;
    std::printf("Preview23: %zu completed world and %zu ground IDs; typed aliases and control-marker isolation passed\n",
        std::size(preview23CompletedWorldIds),std::size(preview23CompletedGroundIds));
    return 0;
}
