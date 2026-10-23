#include "native/client_build.h"

namespace rnative {
ClientBuild IdentifyClientModule(HMODULE module, ClientModule kind)
{
    if (!module) return ClientBuild::Unknown;
    const auto* base = reinterpret_cast<const unsigned char*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096)
        return ClientBuild::Unknown;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return ClientBuild::Unknown;
    struct Profile { DWORD prkStamp, liveStamp, prkSize, liveSize; };
    static constexpr Profile profiles[] = {
        {0x5CD328BA, 0x647A069E, 0x1ED000, 0x1ED000}, // Randy: identical .text, .data and export RVAs
        {0x5CD32932, 0x647A0716, 0x15E000, 0x15E000}, // DisplaySystem: identical .text and .data
        {0x5CD32B09, 0x647A08F0, 0x30A000, 0x30B000},
        {0x5CD32BEA, 0x647A09DA, 0x2B0000, 0x2B1000},
        {0x5CD32958, 0x647A073C, 0x62000, 0x62000}, // N3: identical ground/ring functions
        {0x5CD328C2, 0x647A06A6, 0x1C000, 0x1C000},
    };
    const auto& p = profiles[static_cast<unsigned>(kind)];
    const DWORD stamp = nt->FileHeader.TimeDateStamp, size = nt->OptionalHeader.SizeOfImage;
    if (stamp == p.prkStamp && size == p.prkSize) return ClientBuild::Prk;
    if (stamp == p.liveStamp && size == p.liveSize) return ClientBuild::Live;
    return ClientBuild::Unknown;
}
}
