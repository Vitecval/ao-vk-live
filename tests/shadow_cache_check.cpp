#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ddraw/shadow_cache.h"
#include <cstdio>

int main()
{
    using namespace rvkproxy::shadow_cache;
    std::vector<uint8_t> raw(1024 * 1024), packed;
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = uint8_t((i / 64) ^ (i / 1024));
    const auto expected = raw;
    LARGE_INTEGER a,b,f;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&a);
    if (!Compact(raw, packed) || !raw.empty() || packed.empty()) return 1;
    QueryPerformanceCounter(&b);
    std::printf("cold shadow: %zu -> %zu bytes, %.2f ms\n", expected.size(), packed.size(),
                double(b.QuadPart-a.QuadPart)*1000/double(f.QuadPart));
    auto saved = packed;
    {
        BackingStore file;
        FileCopy a,b,c;
        auto payload=packed;
        if (!file.Store(payload,a) || !payload.empty() || !a.bytes) return 10;
        std::vector<uint8_t> loaded;
        if (!file.Read(a,loaded) || loaded!=saved) return 11;
        auto broken=a; broken.checksum^=1;
        auto unchanged=loaded;
        if (file.Read(broken,loaded) || loaded!=unchanged) return 12;
        // Failed decode retains the durable source, and retry restores exact pixels.
        std::vector<uint8_t> restored;
        if (Restore(restored,loaded,expected.size()-1) || !restored.empty()) return 13;
        if (!file.Read(a,loaded) || !Restore(restored,loaded,expected.size()) || restored!=expected) return 14;
        payload=saved;
        if (!file.Store(payload,b)) return 15;
        const uint64_t peak=file.FileBytes();
        const uint64_t hole=a.offset;
        file.Release(a);
        payload=saved;
        if (!file.Store(payload,c) || c.offset!=hole || file.FileBytes()!=peak) return 16;
        // Thousands of zone-like create/release cycles must reuse the same file.
        for (int i=0;i<2000;++i) {
            file.Release(c); payload=saved;
            if (!file.Store(payload,c) || file.FileBytes()!=peak) return 17;
        }
        file.Release(b); file.Release(c);
        if (file.FileBytes()!=0 || a.bytes || b.bytes || c.bytes) return 18;
        BackingStore unavailable(false);
        payload=saved;
        if (unavailable.Store(payload,a) || payload!=saved || a.bytes) return 19;
        std::puts("PASS: file round trip, integrity failure, decode retry, 2000 extent-reuse cycles, cleanup and file-unavailable fallback");
    }
    if (Restore(raw, packed, expected.size() - 1) || packed != saved || !raw.empty()) return 2;
    if (!Restore(raw, packed, expected.size()) || raw != expected || !packed.empty()) return 3;
    raw[511] = 99; // a later game edit survives another round trip
    auto edited = raw;
    if (!Compact(raw, packed) || !Restore(raw, packed, edited.size()) || raw != edited) return 4;
    uint32_t seed = 0x759ad731;
    for (auto& x : raw) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; x = uint8_t(seed); }
    auto noise = raw;
    if (Compact(raw, packed) || !packed.empty() || raw != noise) return 5;
    raw.assign(1024, 31);
    if (Compact(raw, packed) || raw.size() != 1024) return 6;
    std::puts("PASS: exact pixels, failed restore retains source, later edits, incompressible fallback and small-copy bypass");
    return 0;
}
