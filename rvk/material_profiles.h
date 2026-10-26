// Lightweight material defaults for identified character/equipment surfaces.
// Explicit ORM maps always take priority. No texture images or per-surface allocations are required.
#pragma once
#include <algorithm>
#include <cstdint>
#include <iterator>

namespace rvk::materials {
struct Entry { uint64_t key; uint32_t packed; };
inline constexpr Entry catalog[] = {
#include "material_catalog.inc"
};
inline constexpr Entry worldCatalog[] = {
#include "world_material_catalog.inc"
};

inline uint32_t LookupWorld(uint32_t type, uint32_t id)
{
    if (type == 1010016 || type == 1010017) type = 1010004;
    const uint64_t key = uint64_t(type) << 32 | id;
    const auto it = std::lower_bound(std::begin(worldCatalog), std::end(worldCatalog), key,
                                    [](const Entry& e, uint64_t k) { return e.key < k; });
    return it != std::end(worldCatalog) && it->key == key ? it->packed : 0;
}

inline uint32_t Lookup(uint32_t type, uint32_t id)
{
    if (type == 1010016 || type == 1010017) type = 1010004;
    if (type == 1010019 || type == 1010020) type = 1010011;
    const uint64_t key = uint64_t(type) << 32 | id;
    const auto it = std::lower_bound(std::begin(catalog), std::end(catalog), key,
                                    [](const Entry& e, uint64_t k) { return e.key < k; });
    return it != std::end(catalog) && it->key == key ? it->packed : 0;
}
}
