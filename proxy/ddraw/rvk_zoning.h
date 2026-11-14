#pragma once
#include <windows.h>

namespace rvkproxy {
void ZoningFrame();
void ZoningStop();
bool ZoningMessage(HWND window, UINT message, WPARAM key, LPARAM data);
}
