// RViewPort_t natively, first part (viewport.cpp): randy-vk.ini [Native] Scene=on.
#pragma once

#include "native/native.h"

namespace rnative::viewport {

void Install(HMODULE orig);
void PrepareDisplayCamera(void* viewport,void* camera); // shared by projection, pick rays and frustum setup

}  // namespace rnative::viewport
