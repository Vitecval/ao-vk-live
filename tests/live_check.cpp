// Run from an isolated copy of the classic client's DLLs, with ao-vk.dll and ddraw.dll beside it.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>

int main(int argc, char**)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS* e) -> LONG {
        if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
            static bool reporting;
            if (reporting) return EXCEPTION_CONTINUE_SEARCH;
            reporting = true;
            std::printf("fault at %p, accessing %p; eax %lx ebx %lx ecx %lx edx %lx esi %lx edi %lx esp %lx\n",
                e->ExceptionRecord->ExceptionAddress, reinterpret_cast<void*>(e->ExceptionRecord->ExceptionInformation[1]),
                e->ContextRecord->Eax, e->ContextRecord->Ebx, e->ContextRecord->Ecx, e->ContextRecord->Edx,
                e->ContextRecord->Esi, e->ContextRecord->Edi, e->ContextRecord->Esp);
            MEMORY_BASIC_INFORMATION memory{};
            VirtualQuery(e->ExceptionRecord->ExceptionAddress, &memory, sizeof(memory));
            char path[MAX_PATH] = {};
            GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase), path, MAX_PATH);
            std::printf("module %s + %lx\n", path, DWORD(e->ExceptionRecord->ExceptionAddress) - DWORD(memory.AllocationBase));
            void* trace[20] = {};
            USHORT n = CaptureStackBackTrace(0, 20, trace, nullptr);
            for (USHORT i = 0; i < n; ++i) {
                VirtualQuery(trace[i], &memory, sizeof(memory));
                GetModuleFileNameA(static_cast<HMODULE>(memory.AllocationBase), path, MAX_PATH);
                std::printf("  %s + %lx\n", path, DWORD(trace[i]) - DWORD(memory.AllocationBase));
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    });
    std::puts("loading stock renderer");
    HMODULE stock = LoadLibraryA("randy31.dll");
    if (!stock) { std::printf("stock renderer load failed: %lu\n", GetLastError()); return 1; }
    std::puts("loading addon");
    HMODULE addon = LoadLibraryA("ao-vk.dll");
    if (!addon) { std::printf("addon load failed: %lu\n", GetLastError()); return 1; }
    using Boot = BOOL(__cdecl*)(HMODULE, void**, void**);
    auto boot = reinterpret_cast<Boot>(GetProcAddress(addon, "RvkBootstrap"));
    void* create = nullptr;
    void* enumerate = nullptr;
    if (!boot || boot(nullptr, &create, &enumerate) || boot(stock, nullptr, &enumerate)) {
        std::puts("bootstrap argument validation failed"); return 1;
    }
    std::puts("bootstrapping addon");
    if (!boot(stock, &create, &enumerate) || !create || !enumerate) {
        std::puts("classic live bootstrap failed"); return 1;
    }
    auto* base = reinterpret_cast<BYTE*>(stock);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    auto* exp = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + nt->OptionalHeader.DataDirectory[0].VirtualAddress);
    auto* names = reinterpret_cast<DWORD*>(base + exp->AddressOfNames);
    unsigned missing = 0;
    for (DWORD i = 0; i < exp->NumberOfNames; ++i)
        if (!GetProcAddress(addon, reinterpret_cast<char*>(base + names[i]))) ++missing;
    const char* data = "?m_pcInstance@render_t@@0PAV1@A";
    if (GetProcAddress(stock, data) != GetProcAddress(addon, data)) { std::puts("data forwarding failed"); return 1; }
    using GetSize = unsigned(__cdecl*)(unsigned);
    auto getSize = reinterpret_cast<GetSize>(GetProcAddress(addon, "?GetFormatSize@VertexBuffer_c@@SAII@Z"));
    if (!getSize || getSize(0x152) != 36 || missing) {
        std::printf("export checks failed: missing=%u\n", missing); return 1;
    }
    if (argc > 1) {
        for (const char* name : {"DisplaySystem.dll", "N3.dll", "GUI.dll", "Gamecode.dll", "FXS.dll"}) {
            std::printf("loading importer %s\n", name);
            if (!LoadLibraryA(name)) { std::printf("importer load failed: %lu\n", GetLastError()); return 1; }
        }
        if (!boot(stock, &create, &enumerate)) return 1;
        unsigned routed = 0;
        for (const char* name : {"DisplaySystem.dll", "Gamecode.dll", "GUI.dll", "FXS.dll"}) {
            auto* module = reinterpret_cast<BYTE*>(GetModuleHandleA(name));
            auto* headers = reinterpret_cast<IMAGE_NT_HEADERS32*>(module + reinterpret_cast<IMAGE_DOS_HEADER*>(module)->e_lfanew);
            auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(module + headers->OptionalHeader.DataDirectory[1].VirtualAddress);
            for (; imp->Name; ++imp) {
                if (_stricmp(reinterpret_cast<char*>(module + imp->Name), "randy31.dll")) continue;
                auto* symbol = reinterpret_cast<IMAGE_THUNK_DATA32*>(module + imp->OriginalFirstThunk);
                auto* iat = reinterpret_cast<IMAGE_THUNK_DATA32*>(module + imp->FirstThunk);
                for (; symbol->u1.AddressOfData; ++symbol, ++iat) {
                    const char* symbolName = IMAGE_SNAP_BY_ORDINAL32(symbol->u1.Ordinal)
                        ? MAKEINTRESOURCEA(IMAGE_ORDINAL32(symbol->u1.Ordinal))
                        : reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(module + symbol->u1.AddressOfData)->Name;
                    if (iat->u1.Function != DWORD(GetProcAddress(addon, symbolName))) {
                        std::printf("import routing failed in %s\n", name); return 1;
                    }
                    ++routed;
                }
            }
        }
        std::printf("import routing passed: %u imports across four retail modules\n", routed);
    }
    std::printf("live bootstrap passed: %lu stock exports, data forwarding, native call, no backup renderer needed\n", exp->NumberOfNames);
    std::fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0); // avoid the original game's process-exit singleton destructors
}
