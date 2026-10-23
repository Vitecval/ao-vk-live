// App-local DirectDraw loader. Bootstrap only from AO's renderer, outside DllMain.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ddraw.h>
#include <intrin.h>
#include <cstdio>
#include <cwchar>
#include <atomic>

static_assert(sizeof(void*) == 4, "AO's loader must be built for x86");
namespace {
HMODULE g_self;
std::atomic<FARPROC> g_targets[22];
const char* const kNames[] = {
    "AcquireDDThreadLock", "CompleteCreateSysmemSurface", "D3DParseUnknownCommand", "DDGetAttachedSurfaceLcl",
    "DDInternalLock", "DDInternalUnlock", "DSoundHelp", "DirectDrawCreate", "DirectDrawCreateClipper",
    "DirectDrawCreateEx", "DirectDrawEnumerateA", "DirectDrawEnumerateExA", "DirectDrawEnumerateExW",
    "DirectDrawEnumerateW", "DllCanUnloadNow", "DllGetClassObject", "GetDDSurfaceLocal", "GetOLEThunkData",
    "GetSurfaceFromDC", "RegisterSpecialCase", "ReleaseDDThreadLock", "SetAppCompatData"
};
using CreateFn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
using EnumerateFn = HRESULT(WINAPI*)(LPDDENUMCALLBACKEXA, LPVOID, DWORD);
CreateFn g_create;
EnumerateFn g_enumerate;

void LocalPath(wchar_t* path, const wchar_t* file)
{
    path[0] = 0;
    if (!GetModuleFileNameW(g_self, path, MAX_PATH)) return;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || size_t(slash - path) + 1 + wcslen(file) >= MAX_PATH) { path[0] = 0; return; }
    wcscpy(slash + 1, file);
}
void Log(const char* line)
{
    wchar_t path[MAX_PATH];
    LocalPath(path, L"ao-vk-loader.log");
    if (FILE* f = _wfopen(path, L"a")) {
        std::fprintf(f, "pid %lu: %s (Windows error %lu)\n", GetCurrentProcessId(), line, GetLastError());
        std::fclose(f);
    }
}
HMODULE SystemDDraw()
{
    static HMODULE module = [] {
        wchar_t path[MAX_PATH] = {};
        const UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (!n || n + 11 >= MAX_PATH) return HMODULE(nullptr);
        wcscat(path, L"\\ddraw.dll");
        return LoadLibraryW(path);
    }();
    return module;
}
FARPROC SystemExport(unsigned index)
{
    FARPROC p = GetProcAddress(SystemDDraw(), kNames[index]);
    if (!p) { Log("required system DirectDraw export unavailable"); RaiseFailFastException(nullptr, nullptr, 0); }
    return p;
}
bool AddonFor(void* caller)
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(caller), &module) || module != GetModuleHandleW(L"randy31.dll"))
        return false;
    wchar_t process[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, process, MAX_PATH);
    const wchar_t* name = wcsrchr(process, L'\\');
    name = name ? name + 1 : process;
    if (_wcsicmp(name, L"AnarchyOnline.exe") && _wcsicmp(name, L"randy_harness.exe")) return false;
    static const bool ready = [module] {
    // AOPP launches a copy of the executable in AppData. Its ddraw shim lives there, but the
    // renderer, addon, settings and assets still belong to the actual client installation.
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return false;
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash || size_t(slash - path) + 11 >= MAX_PATH) return false;
    wcscpy(slash + 1, L"ao-vk.dll");
    HMODULE addon = path[0] ? LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) : nullptr;
    using Bootstrap = BOOL(__cdecl*)(HMODULE, void**, void**);
    auto boot = reinterpret_cast<Bootstrap>(addon ? GetProcAddress(addon, "RvkBootstrap") : nullptr);
    void* create = nullptr;
    void* enumerate = nullptr;
    if (!boot || !boot(module, &create, &enumerate) || !create || !enumerate) {
        Log("addon unavailable or client unsupported; using system renderer");
        return false;
    }
    g_create = reinterpret_cast<CreateFn>(create);
    g_enumerate = reinterpret_cast<EnumerateFn>(enumerate);
    Log("classic live addon loaded; original randy31.dll kept on disk");
    return true;
    }();
    return ready;
}
HRESULT WINAPI Create(GUID* guid, LPVOID* out, REFIID iid, IUnknown* outer)
{
    if (AddonFor(_ReturnAddress())) return g_create(guid, out, iid, outer);
    return reinterpret_cast<CreateFn>(SystemExport(9))(guid, out, iid, outer);
}
HRESULT WINAPI Enumerate(LPDDENUMCALLBACKEXA callback, LPVOID context, DWORD flags)
{
    if (AddonFor(_ReturnAddress())) return g_enumerate(callback, context, flags);
    return reinterpret_cast<EnumerateFn>(SystemExport(11))(callback, context, flags);
}
}
extern "C" FARPROC __cdecl ResolveDDraw(unsigned index)
{
    if (FARPROC cached = g_targets[index].load(std::memory_order_acquire)) return cached;
    FARPROC p = index == 9 ? reinterpret_cast<FARPROC>(&Create)
        : index == 11 ? reinterpret_cast<FARPROC>(&Enumerate) : SystemExport(index);
    g_targets[index].store(p, std::memory_order_release);
    return p;
}
extern "C" __declspec(naked) void ddraw_common()
{
    __asm {
        pushad
        mov eax, [esp + 32]
        push eax
        call ResolveDDraw
        add esp, 4
        mov [esp + 32], eax
        popad
        ret
    }
}
#define FORWARD(N) extern "C" __declspec(naked) void ddraw_##N() { \
    __asm { push N } __asm { jmp ddraw_common } }
FORWARD(0) FORWARD(1) FORWARD(2) FORWARD(3) FORWARD(4) FORWARD(5) FORWARD(6) FORWARD(7)
FORWARD(8) FORWARD(9) FORWARD(10) FORWARD(11) FORWARD(12) FORWARD(13) FORWARD(14) FORWARD(15)
FORWARD(16) FORWARD(17) FORWARD(18) FORWARD(19) FORWARD(20) FORWARD(21)
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { g_self = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
