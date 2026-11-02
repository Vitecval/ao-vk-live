// Static meshes natively (mesh.cpp): randy-vk.ini [Native] Scene=on.
#pragma once

#include "native/native.h"

namespace rnative::mesh {

void Install(HMODULE orig);
// Current, engine-owned RTriMesh_t only (caller verifies its scene class).
// Whole visual's model-space bounds; never a retained visual-address cache.
bool WindowHouseBounds(const void* visual, float bounds[6]);

}  // namespace rnative::mesh
