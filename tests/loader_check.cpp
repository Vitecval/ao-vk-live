#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <cstdio>
int main()
{
    HMODULE shim = LoadLibraryA("ddraw.dll");
    if (!shim) { std::printf("shim load failed: %lu\n", GetLastError()); return 1; }
    for (WORD i = 1; i <= 22; ++i)
        if (!GetProcAddress(shim, MAKEINTRESOURCEA(i))) { std::printf("missing ordinal %u\n", i); return 1; }
    using Lock = void(WINAPI*)();
    reinterpret_cast<Lock>(GetProcAddress(shim, "AcquireDDThreadLock"))();
    reinterpret_cast<Lock>(GetProcAddress(shim, "ReleaseDDThreadLock"))();
    using Enumerate = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
    unsigned count = 0;
    auto callback = [](GUID*, LPSTR, LPSTR, void* context, HMONITOR) -> BOOL {
        ++*static_cast<unsigned*>(context); return DDENUMRET_OK;
    };
    // x86 callback uses stdcall; a noncapturing lambda's Windows conversion selects the required convention.
    LPDDENUMCALLBACKEXA cb = callback;
    HRESULT hr = reinterpret_cast<Enumerate>(GetProcAddress(shim, "DirectDrawEnumerateExA"))(cb, &count, 0);
    if (FAILED(hr) || !count || GetModuleHandleA("ao-vk.dll")) {
        std::printf("passthrough failed: HRESULT=%lx displays=%u addon=%p\n", hr, count, GetModuleHandleA("ao-vk.dll"));
        return 1;
    }
    std::printf("loader passthrough passed: 22 ordinals, system locks, %u displays, addon not loaded in other programs\n", count);
    return 0;
}
