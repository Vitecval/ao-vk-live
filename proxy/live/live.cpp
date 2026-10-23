// The stock renderer stays on disk. ddraw.dll loads this addon on its first graphics call.
#include "native/client_build.h"
#include "ddraw/rvk_backend.h"
#include <cstring>
#include <set>

extern "C" void rvk_ensure_init();
namespace rvkproxy {
HRESULT WINAPI RvkDirectDrawCreateEx(GUID*, LPVOID*, REFIID, IUnknown*);
HRESULT WINAPI RvkDirectDrawEnumerateExA(LPDDENUMCALLBACKEXA, LPVOID, DWORD);

void LiveFrame()
{
    static std::set<HMODULE> patched;
    struct Importer { const char* name; rnative::ClientModule kind; };
    static constexpr Importer importers[] = {
        {"DisplaySystem.dll", rnative::ClientModule::DisplaySystem},
        {"Gamecode.dll", rnative::ClientModule::Gamecode},
        {"GUI.dll", rnative::ClientModule::Gui},
        {"FXS.dll", rnative::ClientModule::Fxs},
    };
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<const char*>(&LiveFrame), &self);
    for (const auto& importer : importers) {
        HMODULE module = GetModuleHandleA(importer.name);
        if (!module || patched.count(module)) continue;
        patched.insert(module);
        if (rnative::IdentifyClientModule(module, importer.kind) == rnative::ClientBuild::Unknown) {
            RvkLog("live: %s is unsupported - its imports left alone", importer.name);
            continue;
        }
        auto* base = reinterpret_cast<BYTE*>(module);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        unsigned count = 0;
        if (directory.VirtualAddress)
            for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress); imp->Name; ++imp) {
                if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), "randy31.dll")) continue;
                if (!imp->OriginalFirstThunk) continue;
                auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->OriginalFirstThunk);
                auto* iat = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++iat) {
                    const char* name = IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)
                        ? MAKEINTRESOURCEA(IMAGE_ORDINAL32(names->u1.Ordinal))
                        : reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name;
                    FARPROC target = GetProcAddress(self, name);
                    FARPROC original = GetProcAddress(rnative::OriginalModule(), name);
                    if (!target || !original || target == original) continue;
                    DWORD protect;
                    if (!VirtualProtect(&iat->u1.Function, sizeof(DWORD), PAGE_READWRITE, &protect)) continue;
                    iat->u1.Function = reinterpret_cast<DWORD>(target);
                    VirtualProtect(&iat->u1.Function, sizeof(DWORD), protect, &protect);
                    ++count;
                }
            }
        RvkLog("live: %s routed %u renderer imports to ao-vk", importer.name, count);
    }
}
}

extern "C" BOOL __cdecl RvkBootstrap(HMODULE original, void** create, void** enumerate)
{
    if (!create || !enumerate || original != GetModuleHandleA("randy31.dll") ||
        rnative::IdentifyClientModule(original, rnative::ClientModule::Randy) != rnative::ClientBuild::Live)
        return FALSE;
    rvk_ensure_init();
    if (!rvkproxy::RvkBackend()) return FALSE;
    rvkproxy::LiveFrame();
    *create = reinterpret_cast<void*>(&rvkproxy::RvkDirectDrawCreateEx);
    *enumerate = reinterpret_cast<void*>(&rvkproxy::RvkDirectDrawEnumerateExA);
    rvkproxy::RvkLog("live: addon active with the stock classic renderer; no game DLL replaced on disk");
    return TRUE;
}
