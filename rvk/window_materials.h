// Preview 20 Newland experiment. Ownership: material-packs/building-families.json,
// neutral-midtech (5). Pane coordinates: ao-cyber-world/catalog.json, reviewed
// against the selected HD art. No guessing from texture names or brightness.
#pragma once
#include <cstdint>
#include "building_families.h"
#include "window_global.h"
namespace rvk::environment {
inline uint32_t WindowLayout(uint32_t type, uint32_t id) {
    if (buildings::FamilyOf(type, id) != 5) return 0;
    switch (id) {
    case 6052: return 1; case 6055: return 2; case 6081: return 3;
    case 6059: return 4; case 6060: return 5; case 6069: return 6; case 6086: return 7;
    default: return 0;
    }
}
inline bool WindowNeedsModel(uint32_t id) { return id == 6059 || id == 6069; }
inline bool WindowModelAllowed(uint32_t texture, uint32_t type, uint32_t model) {
    if (type != 1010001 && type != 1010026) return false;
    if (texture == 6059) return model == 1416 || model == 1418 || model == 1420 || model == 99415;
    if (texture == 6069) return model == 1419;
    return false;
}
}
