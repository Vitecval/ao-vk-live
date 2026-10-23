#include "native/client_build.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 4) { std::puts("client_build_check PRK_DIR LIVE_DIR NEW_ENGINE_DIR"); return 2; }
    const char* names[] = {"randy31.dll", "DisplaySystem.dll", "Gamecode.dll", "GUI.dll", "N3.dll", "FXS.dll"};
    unsigned failures = 0;
    for (int client = 0; client < 3; ++client)
        for (unsigned i = 0; i < 6; ++i) {
            const char* name = client == 0 && i == 0 ? "randy31_orig.dll" : names[i];
            std::string path = std::string(argv[client + 1]) + "\\" + name;
            HMODULE module = LoadLibraryExA(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
            auto actual = rnative::IdentifyClientModule(module, static_cast<rnative::ClientModule>(i));
            auto expected = client == 0 ? rnative::ClientBuild::Prk : client == 1 ? rnative::ClientBuild::Live : rnative::ClientBuild::Unknown;
            if (!module || actual != expected) { ++failures; std::printf("FAIL %s\n", path.c_str()); }
            if (module) FreeLibrary(module);
        }
    std::printf("client build profiles: %u failures across 18 real modules\n", failures);
    return failures ? 1 : 0;
}
