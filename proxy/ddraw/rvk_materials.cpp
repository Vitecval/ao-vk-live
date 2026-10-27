// Material maps for RDB textures, side-loaded from <client>\randy-vk\materials\ (any size each):
//   <type>_<id>_n.png    tangent-space normal map (OpenGL convention, as Blender bakes); <type> is the full-quality
//                        table (1010004 world, 1010006 ground, 1010011 character skins), which also covers its lower
//                        levels
//   <type>_<id>_orm.png  PBR material, glTF packing: R occlusion, G roughness, B metallic
//   <type>_<id>_r.png, _m.png, _ao.png   ... or separate greyscale roughness / metallic / occlusion maps (packed here;
//                        a missing one is roughness 1, metallic 0, occlusion 1). Ignored when there is an _orm.png.
//   <type>_<id>_d.png    albedo (diffuse colour): drawn instead of the game's texture, alpha included
//   <type>_<id>_e.png    emissive: the light the surface gives off (black = none), added after its lighting
//   <type>_<id>_leaves.png, _leaves_n.png   a tree canopy's own leaf art (rvk leaves.cpp): an atlas of 4 x 4 sprigs drawn
//                        in place of this canopy texture's cards, and its normal map (ao-assets leaf-art --install)
//   <id>_<suffix>        any of these for any type (fallback)
// ao-assets writes these names (python -m aoassets material-template / import).
//
// Texture packs: <client>\randy-vk\packs\<name>\ hold the same names (a released pack is one such folder). A
// texture's maps all come from one place: live\ (preview), else materials\ if it has any file for it, else the first
// pack (by folder name) that has. Packs are read once at startup (not hot reloaded).
//
// Hot reload (RANDYVK_HOTRELOAD=1): both materials\ and live\ (the ao-assets workbench's previews of checked-out
// assets) are watched; when a texture's files change, its maps are decoded again and replace what every surface of it
// had (a map that's gone is removed). A texture id with any file in live\ takes its maps from there only;
// <type>_<id>_live.png alone means "previewed, no maps".
//
// Identity (see HOOKED_EXPORTS in tools/gen_interface.py; the hooks call the original, then record the identity on the
// surface_t's IDirectDrawSurface7 when it is one of ours):
// - World textures: DisplaySystem's RDBTexture_t hands randy an AnarchyTexCreator_t {+0x40 type, +0x44 id}, whose
//   create method calls the exported TextureStreamCreator::CreateTexture with the creator as `this`.
// - Ground textures: RDBGroundTexture_t {DbObject_t: +0x08 type, +0x0C id, +0x20 its RTexture_t once made} uses a
//   plain TextureStreamCreator, created and called inside randy. What crosses the export table is the
//   RTexture_t(name, TextureCreator*) constructor it calls; the RDBGroundTexture_t making it is found by RTTI in the
//   caller's saved registers and stack, and the new RTexture_t's surface_t (+0x30) gets its identity.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "rvk_backend.h"
#include "native/native.h"

#include "material_maps.h"
#include "terrain_materials.h"
#include "building_families.h"

namespace rvkproxy {

// {6a1f0e52-2d43-4b8e-9c55-7f3a0b1d9e10}
const GUID IID_RvkSurface = {0x6a1f0e52, 0x2d43, 0x4b8e, {0x9c, 0x55, 0x7f, 0x3a, 0x0b, 0x1d, 0x9e, 0x10}};

namespace {

// ---------------------------------------------------------------- creator identity (MSVC RTTI)
bool Readable(const void* p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
        return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return false;
    auto end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    return static_cast<const uint8_t*>(p) + n <= end;
}

enum class Kind { Other, AnarchyTexCreator, RDBGroundTexture, RDBMesh, RDBCATMesh };

// Class of a polymorphic object from its vtable's complete object locator (x86: vtable[-1] -> COL, COL+12 -> type
// descriptor, name at +8, e.g. ".?AVAnarchyTexCreator_t@@").
Kind ClassOf(const void* object)
{
    static std::unordered_map<const void*, Kind> cache;   // vtable -> class
    if (!Readable(object, 4))
        return Kind::Other;
    const void* vtable = *static_cast<void* const*>(object);
    auto it = cache.find(vtable);
    if (it != cache.end())
        return it->second;
    Kind kind = Kind::Other;
    auto vt = static_cast<const uint8_t* const*>(vtable);
    if (Readable(vt - 1, 4)) {
        const uint8_t* col = vt[-1];
        if (Readable(col, 16)) {
            const uint8_t* td = *reinterpret_cast<const uint8_t* const*>(col + 12);
            static const char kTex[] = ".?AVAnarchyTexCreator_t@@", kGround[] = ".?AVRDBGroundTexture_t@@";
            static const char kMesh[] = ".?AVRDBMesh_t@@", kCat[] = ".?AVRDBCATMesh_t@@";
            if (Readable(td + 8, sizeof(kTex)) && std::memcmp(td + 8, kTex, sizeof(kTex)) == 0)
                kind = Kind::AnarchyTexCreator;
            else if (Readable(td + 8, sizeof(kGround)) && std::memcmp(td + 8, kGround, sizeof(kGround)) == 0)
                kind = Kind::RDBGroundTexture;
            else if (Readable(td + 8, sizeof(kMesh)) && std::memcmp(td + 8, kMesh, sizeof(kMesh)) == 0)
                kind = Kind::RDBMesh;
            else if (Readable(td + 8, sizeof(kCat)) && std::memcmp(td + 8, kCat, sizeof(kCat)) == 0)
                kind = Kind::RDBCATMesh;
        }
    }
    cache.emplace(vtable, kind);
    return kind;
}

uint32_t U32(const void* p, size_t offset)
{
    return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(p) + offset);
}

// An RDBGroundTexture_t still making its texture: a ground texture type, no RTexture_t yet.
bool IsGroundMaker(const void* p)
{
    if (!p || (reinterpret_cast<uintptr_t>(p) & 3) || !Readable(p, 0x28) || ClassOf(p) != Kind::RDBGroundTexture)
        return false;
    uint32_t type = U32(p, 0x08);
    return (type == 1010006 || type == 1010021 || type == 1010022) && U32(p, 0x20) == 0;
}

// The lower texture quality levels are other tables with the same ids: the ground's 1010021/22 for 1010006, the
// world's 1010016/17 for 1010004. A map is named after the full-quality table and serves every level.
uint32_t FullQualityType(uint32_t type)
{
    switch (type) {
    case 1010021: case 1010022: return 1010006;
    case 1010016: case 1010017: return 1010004;
    case 1010019: case 1010020: return 1010011;
    default: return type;
    }
}

uint64_t KeyOf(uint32_t type, uint32_t id) { return uint64_t(FullQualityType(type)) << 32 | id; }

// ---------------------------------------------------------------- material folders
struct Folder {
    std::string name;                            // explicit provider for reviewed family resources
    std::string dir;
    unsigned modern = 0;                         // optional style pack: 1 classic, 2 cyberpunk
    bool world = false;                          // world artwork preserves original architecture alpha
    std::unordered_map<std::string, uint64_t> files;   // lower-case name -> last write time ^ size (hot reload)
};
Folder g_mat, g_live;                         // materials\ (applied maps), live\ (workbench previews, hot reload only)
std::vector<Folder> g_packs;                  // packs\<name>\ (released texture packs), by name
std::unordered_set<uint64_t> g_liveKeys;      // texture ids with any file in live\ (their maps come from there only)
bool g_scanned = false;
bool g_hot = false;                           // RANDYVK_HOTRELOAD=1
std::atomic<unsigned> g_modernStyle{0};
std::atomic<bool> g_sharedTerrainEnabled{false}, g_sharedTerrainChanged{false};
std::atomic<bool> g_modernChanged{false};
std::atomic<bool> g_characterPack{false}, g_characterChanged{false};
uint64_t g_buildingFamiliesAvailable = 0, g_buildingFamiliesReady = 0;
void ScanBuildingFamilies();
void PollBuildingFamilies();

void BuildingFamilyReady(uint32_t family, bool ready)
{
    if (!family || family >= 64 || !g_rvk.device) return;
    const uint64_t bit=uint64_t(1)<<family;
    if (bool(g_buildingFamiliesReady&bit)==ready) return;
    if (ready) g_buildingFamiliesReady |= bit;
    else g_buildingFamiliesReady &= ~bit;
    g_rvk.device->SetBuildingFamilyReady(family,ready);
    RvkLog("building family %u: %s",family,ready ? "resident replacements ready" : "no resident replacements");
}

std::string Lower(std::string s)
{
    for (char& c : s) c = char(tolower(static_cast<unsigned char>(c)));
    return s;
}

void ScanFolder(Folder& f)
{
    f.files.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((f.dir + "*.png").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        uint64_t stamp = (uint64_t(fd.ftLastWriteTime.dwHighDateTime) << 32 | fd.ftLastWriteTime.dwLowDateTime)
                         ^ (uint64_t(fd.nFileSizeHigh) << 32 | fd.nFileSizeLow);
        f.files.emplace(Lower(fd.cFileName), stamp);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// "<type>_<id>_..." -> its texture key (0 for other names).
uint64_t KeyOfName(const std::string& name)
{
    unsigned type = 0, id = 0;
    char sep = 0;
    if (std::sscanf(name.c_str(), "%u_%u%c", &type, &id, &sep) == 3 && sep == '_' && type >= 1000000)
        return KeyOf(type, id);
    return 0;
}

void RebuildLiveKeys()
{
    g_liveKeys.clear();
    for (auto& [name, stamp] : g_live.files)
        if (uint64_t key = KeyOfName(name))
            g_liveKeys.insert(key);
}

void ScanPacks(const std::string& root)
{
    std::vector<std::string> names;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((root + "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.')
            names.push_back(fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); });
    for (const std::string& name : names) {
        Folder f;
        f.name = Lower(name);
        f.dir = root + name + "\\";
        f.world = Lower(name) == "ao-cyber-world";
        f.modern = Lower(name) == "ao-modern-icc" ? 1u : (Lower(name) == "ao-cyber-icc" || f.world) ? 2u :
                   Lower(name) == "ao-cyber-characters" ? 3u : 0u;
        ScanFolder(f);
        RvkLog("materials: pack '%s', %u files", name.c_str(), unsigned(f.files.size()));
        if (!f.files.empty())
            g_packs.push_back(std::move(f));
    }
}

void Scan()
{
    if (g_scanned)
        return;
    g_scanned = true;
    char path[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&Scan), &self);
    GetModuleFileNameA(self, path, MAX_PATH);
    char* slash = std::strrchr(path, '\\');
    if (slash) slash[1] = 0; else path[0] = 0;
    g_mat.dir = std::string(path) + "randy-vk\\materials\\";
    g_live.dir = std::string(path) + "randy-vk\\live\\";
    char hot[8] = {};
    g_hot = GetEnvironmentVariableA("RANDYVK_HOTRELOAD", hot, sizeof(hot)) && hot[0] == '1';
    ScanFolder(g_mat);
    ScanPacks(std::string(path) + "randy-vk\\packs\\");
    ScanBuildingFamilies();
    if (g_hot) {
        CreateDirectoryA(g_live.dir.c_str(), nullptr);
        ScanFolder(g_live);
        RebuildLiveKeys();
    }
    RvkLog("materials: %u files in %s%s", unsigned(g_mat.files.size()), g_mat.dir.c_str(),
           g_hot ? "; hot reload on (watching it and live\\)" : "");
}

constexpr const char* kSuffixes[] = {"_n.png", "_h.png", "_orm.png", "_r.png", "_m.png", "_ao.png", "_d.png", "_e.png",
                                     "_leaves.png", "_leaves_n.png"};

std::string FindIn(const Folder& f, uint32_t type, uint32_t id, const char* suffix)
{
    for (uint32_t t : {FullQualityType(type), type}) {
        std::string a = std::to_string(t) + "_" + std::to_string(id) + suffix;
        if (f.files.count(a)) return f.dir + a;
    }
    std::string b = std::to_string(id) + suffix;
    if (f.files.count(b)) return f.dir + b;
    return {};
}

bool HasAny(const Folder& f, uint32_t type, uint32_t id)
{
    for (const char* suffix : kSuffixes)
        if (!FindIn(f, type, id, suffix).empty())
            return true;
    return false;
}

void ScanBuildingFamilies()
{
    g_buildingFamiliesAvailable=0;
    for (uint32_t family=1;family<rvk::buildings::familyCount;++family) {
        bool complete=true,found=false;
        for (const auto& resource:rvk::buildings::resources) {
            if (resource.family!=family) continue;
            found=true;
            const char* suffixes[]={"_d.png","_n.png","_orm.png","_e.png","_h.png"};
            for (unsigned map=0;map<5;++map) {
                if (!(resource.maps&(1u<<map))) continue;
                bool present=false;
                for (const Folder& pack:g_packs)
                    if (pack.name==resource.pack && !FindIn(pack,1010004,resource.id,suffixes[map]).empty()) {present=true;break;}
                if (!present) {complete=false;break;}
            }
        }
        if (complete && found) g_buildingFamiliesAvailable |= uint64_t(1)<<family;
        RvkLog("building family %u: %s",family,complete && found ? "complete files" : "incomplete files; originals retained");
    }
}

// Where a texture's maps come from (see the top of the file).
const Folder& FolderFor(uint32_t type, uint32_t id)
{
    Scan();
    if (g_liveKeys.count(KeyOf(type, id)))
        return g_live;
    if (!HasAny(g_mat,type,id) && g_modernStyle.load()==2)
        if (const auto* resource=rvk::buildings::ResourceOf(type,id)) {
            for (const Folder& pack:g_packs)
                if (pack.name==resource->pack) return pack;
            return g_mat; // no replacement from an unrelated older pack
        }
    if (!g_packs.empty() && !HasAny(g_mat, type, id))
        for (const Folder& pack : g_packs) {
            if (pack.modern == 3 ? !g_characterPack.load() : pack.modern && pack.modern != g_modernStyle.load()) continue;
            if (HasAny(pack, type, id)) {
                // A matched family must use one authored set, even when an older
                // ICC pack also repainted that ID. Personal folders win above.
                if (pack.modern==2 && !pack.world && FindIn(pack,type,id,"_d.png").empty())
                    for (const Folder& world : g_packs)
                        if (world.world && !FindIn(world,type,id,"_d.png").empty()) return world;
                return pack;
            }
        }
    return g_mat;
}

std::string FindMap(uint32_t type, uint32_t id, const char* suffix)
{
    return FindIn(FolderFor(type, id), type, id, suffix);
}

// ---------------------------------------------------------------- map loading (rvk/material_maps.h)
// Decoding takes time (a 1024 x 1024 normal map and material about 100 ms), so it runs on a worker thread: a
// texture asks for its maps (AttachMaterialMaps), the worker decodes them, and PollMaterialMaps (each present, on the
// game's thread like every device call) uploads them to every surface still waiting for that texture id. Decoded
// maps stay in a small cache: a texture's other quality levels, and textures made again for the same id (zoning),
// follow soon after.
constexpr size_t kCacheBytes = 24u << 20;          // decoded maps kept, not GPU texture resolution
constexpr size_t kUploadBytesPerFrame = 12u << 20; // keep staging bursts small in the 2 GB retail process

uint64_t FreeMaterialAddressSpace()
{
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    return GlobalMemoryStatusEx(&memory) ? memory.ullAvailVirtual : UINT64_MAX;
}

rvk::maps::MaterialFiles FilesFor(uint32_t type, uint32_t id)
{
    rvk::maps::MaterialFiles f;
    const Folder& folder = FolderFor(type, id);
    // A statue/corpse can share a character texture. Architecture's retained
    // originals must not suppress the independently selected equipment pack.
    if (folder.modern == 2 && rvk::buildings::Managed(type,id)) {
        const uint32_t family=rvk::buildings::FamilyOf(type,id);
        const auto* resource=rvk::buildings::ResourceOf(type,id);
        if (!family || !resource) return f;
        const char* required[]={"_d.png","_n.png","_orm.png","_e.png","_h.png"};
        for (unsigned i=0;i<5;++i)
            if ((resource->maps&(1u<<i)) && FindIn(folder,type,id,required[i]).empty()) return f;
    }
    // Partial per-tile repaints break road/biome transitions even with shared
    // detail off. Original ground supplies those masks in both modes. Keep the
    // reviewed manufactured deck/paving sets and all personal overrides.
    if (folder.modern && rvk::materials::IsGroundType(type) &&
        !rvk::materials::AuthoredGround(type, id)) return f;
    if (folder.modern) f.maxSize = 512; // bounded built-in artwork in the 2 GB retail process
    f.preserveAlbedoAlpha = folder.modern == 3 ||
        ((folder.world || (folder.modern==2 && rvk::buildings::FamilyOf(type,id))) && FullQualityType(type) != 1010006);
    f.normal = FindMap(type, id, "_n.png");
    f.height = FindMap(type, id, "_h.png");
    f.packed = FindMap(type, id, "_orm.png");
    f.parts[0] = FindMap(type, id, "_ao.png");
    f.parts[1] = FindMap(type, id, "_r.png");
    f.parts[2] = FindMap(type, id, "_m.png");
    f.albedo = FindMap(type, id, "_d.png");
    f.emissive = FindMap(type, id, "_e.png");
    f.leaves = FindMap(type, id, "_leaves.png");
    f.leavesNormal = FindMap(type, id, "_leaves_n.png");
    if (rvk::materials::DeckGround(type,id)) {
        // This identified family always retains AO's painting and tread.
        // Old packs, previews and quality aliases cannot restore noisy relief.
        f.albedo.clear(); f.normal.clear(); f.height.clear(); f.emissive.clear();
    }
    return f;
}

using DecodedPtr = std::shared_ptr<const rvk::maps::Decoded>;

// Worker thread and its queues (never destroyed: the thread may still wait on them while the process exits).
struct Worker {
    std::mutex mutex;
    std::condition_variable wake;
    struct Job { uint64_t key; uint32_t gen; rvk::maps::MaterialFiles files; };
    struct Done { uint64_t key; uint32_t gen; DecodedPtr decoded; };
    std::deque<Job> jobs;
    std::vector<Done> done;
    size_t doneBytes = 0;
    uint64_t decoding = 0;
};
Worker* g_worker = nullptr;

void WorkerLoop(Worker* w)
{
    for (;;) {
        Worker::Job job;
        {
            std::unique_lock<std::mutex> lock(w->mutex);
            // Bounded look-ahead also progresses while AO is loading without
            // presenting. Never accumulate an entire zone's decoded maps.
            w->wake.wait(lock, [w] { return !w->jobs.empty() && w->done.size() < 4 && w->doneBytes < (12u << 20); });
            job = std::move(w->jobs.front());
            w->jobs.pop_front();
            w->decoding = job.key;
        }
        DecodedPtr decoded;
        try {
            if (FreeMaterialAddressSpace() >= (192u << 20)) {
                if (FreeMaterialAddressSpace() < (384u << 20))
                    job.files.maxSize = std::min(job.files.maxSize,256u);
                decoded = std::make_shared<rvk::maps::Decoded>(rvk::maps::Decode(job.files));
            }
            else
                RvkLog("materials: RDB texture %u:%u keeps original maps (low process address space)",
                       uint32_t(job.key >> 32), uint32_t(job.key));
        } catch (const std::bad_alloc&) {
            // Optional artwork must not terminate the entire game if a fragmented 32-bit heap cannot fit it.
            RvkLog("materials: RDB texture %u:%u keeps original maps (decode allocation failed)",
                   uint32_t(job.key >> 32), uint32_t(job.key));
        }
        std::lock_guard<std::mutex> lock(w->mutex);
        w->doneBytes += decoded ? decoded->Bytes() : 0;
        w->decoding = 0;
        w->done.push_back({job.key, job.gen, std::move(decoded)});
    }
}

// Game thread only from here.
struct Waiter {
    RSurface* top;
    rvk::Texture* texture;                         // the texture it asked for (a re-created one asks again)
};
std::unordered_map<uint64_t, std::vector<Waiter>> g_waiting;   // by texture id, until its maps are decoded
std::list<std::pair<uint64_t, DecodedPtr>> g_cache;            // most recently used first
std::unordered_map<uint64_t, std::list<std::pair<uint64_t, DecodedPtr>>::iterator> g_cacheIndex;
size_t g_cacheBytes = 0;
std::deque<std::pair<Waiter, DecodedPtr>> g_uploads;           // decoded, not yet uploaded (kUploadBytesPerFrame)
std::unordered_map<uint64_t, uint32_t> g_gen;                  // bumped by a hot reload: older decodes are dropped
// Weak owners: no map is held after its last live AO surface disappears.
// Quality copies previously uploaded another complete HD material set each.
std::unordered_map<uint64_t,std::vector<Waiter>> g_uploaded;
std::unordered_map<uint64_t,DWORD> g_deferred; // low-memory maps retried after headroom recovers
std::unordered_set<uint64_t> g_failed;
std::unordered_set<RSurface*> g_identified;       // surfaces holding an RDB texture (for DescribeTexture)

bool ShareUploaded(const Waiter& w)
{
    static const bool enabled=[] {char value[8]{};GetEnvironmentVariableA("RANDYVK_SHARE_MAPS",value,sizeof(value));return value[0]!='0';}();
    if (!enabled || w.top->texture!=w.texture || !g_rvk.device) return false;
    const uint64_t key=KeyOf(w.top->rdbType,w.top->rdbId);
    auto found=g_uploaded.find(key);
    if (found==g_uploaded.end()) return false;
    auto& owners=found->second;
    owners.erase(std::remove_if(owners.begin(),owners.end(),[](const Waiter& a){return a.top->texture!=a.texture;}),owners.end());
    owners.erase(std::remove_if(owners.begin(),owners.end(),[](const Waiter& a){
        return a.texture->MaterialsState()==rvk::Texture::MaterialState::Failed;
    }),owners.end());
    if (owners.empty()) return false;
    for (const Waiter& a:owners) if (a.texture==w.texture) return true;
    g_rvk.device->ShareMaterialMaps(w.texture,owners.front().texture);
    owners.push_back(w);
    static unsigned shared=0;
    if (++shared<=16 || (shared&(shared-1))==0)
        RvkLog("materials: RDB texture %u:%u shares uploaded material maps (%u copies avoided)",w.top->rdbType,w.top->rdbId,shared);
    return true;
}

DecodedPtr CacheGet(uint64_t key)
{
    auto it = g_cacheIndex.find(key);
    if (it == g_cacheIndex.end())
        return nullptr;
    g_cache.splice(g_cache.begin(), g_cache, it->second);
    return it->second->second;
}

void CacheErase(uint64_t key)
{
    auto it = g_cacheIndex.find(key);
    if (it == g_cacheIndex.end())
        return;
    g_cacheBytes -= it->second->second->Bytes();
    g_cache.erase(it->second);
    g_cacheIndex.erase(it);
}

void CachePut(uint64_t key, DecodedPtr d)
{
    if (g_cacheIndex.count(key) || d->Bytes() > kCacheBytes / 2)
        return;
    g_cache.emplace_front(key, d);
    g_cacheIndex[key] = g_cache.begin();
    g_cacheBytes += d->Bytes();
    while (g_cacheBytes > kCacheBytes && !g_cache.empty()) {
        g_cacheBytes -= g_cache.back().second->Bytes();
        g_cacheIndex.erase(g_cache.back().first);
        g_cache.pop_back();
    }
}

unsigned g_attached = 0;

void Upload(const Waiter& w, const rvk::maps::Decoded& d)
{
    if (w.top->texture != w.texture || !g_rvk.device) return;
    const auto* resource=w.top->buildingFamily ? rvk::buildings::ResourceOf(w.top->rdbType,w.top->rdbId) : nullptr;
    const uint32_t required=resource ? resource->maps : 0;
    if (w.top->buildingFamily && (d.decodeFailed ||
        ((required&1u) && d.albedo.Empty()) || ((required&2u) && d.normal.Empty()) ||
        ((required&4u) && d.orm.Empty()) || ((required&8u) && d.emissive.Empty()) || ((required&16u) && !d.height))) {
        g_failed.insert(KeyOf(w.top->rdbType,w.top->rdbId));
        return;
    }
    if (d.Bytes() && ShareUploaded(w)) return;
    if (w.top->texture != w.texture || !g_rvk.device)
        return;                                    // made again since: that one asks for itself
    RSurface* top = w.top;
    if (w.texture->MaterialsState()!=rvk::Texture::MaterialState::Ready) w.texture->MaterialsPending();
    const bool complete=rvk::maps::Replace(*g_rvk.device, w.texture, d); // exactly these maps, including removed ones
    if (!complete) {
        g_deferred[KeyOf(top->rdbType,top->rdbId)]=GetTickCount();
        return;
    }
    if (d.Bytes()) g_uploaded[KeyOf(top->rdbType,top->rdbId)].push_back(w);
    ++g_attached;
    if (g_attached <= 64 || (g_attached & (g_attached - 1)) == 0)
        RvkLog("materials: RDB texture %u:%u gets%s%s%s%s%s%s%s (%u attached)", top->rdbType, top->rdbId,
               d.normal.Empty() ? "" : " [normal map]", d.orm.Empty() ? "" : " [PBR material ",
               d.orm.Empty() ? "" : (d.ormFrom + "]").c_str(), d.albedo.Empty() ? "" : " [albedo map]",
               d.emissive.Empty() ? "" : " [emissive map]",
               !d.albedo.Empty() && d.preserveAlbedoAlpha ? " [original alpha]" : "",
               d.Bytes() ? "" : " nothing", g_attached);
}

void PollBuildingFamilies()
{
    static uint64_t serial=0;
    if (serial!=g_rvk.deviceSerial) {
        serial=g_rvk.deviceSerial;
        g_buildingFamiliesReady=0;
        if (g_rvk.device)
            for (uint32_t family=1;family<rvk::buildings::familyCount;++family)
                g_rvk.device->SetBuildingFamilyReady(family,false);
    }
    uint64_t seen=0,ready=0;
    if (g_modernStyle.load()!=2) ready=0;
    for (RSurface* top:g_identified) {
        if (!top->texture || !top->buildingFamily) continue;
        const uint64_t bit=uint64_t(1)<<top->buildingFamily;
        seen |= bit;
        const uint64_t key=KeyOf(top->rdbType,top->rdbId);
        auto it=g_uploaded.find(key);
        bool uploaded=false;
        if (it!=g_uploaded.end())
            for (const Waiter& owner:it->second)
                if (owner.top==top && owner.texture==top->texture) {uploaded=true;break;}
        if (uploaded && top->texture->MaterialsState()==rvk::Texture::MaterialState::Failed)
            g_failed.insert(key);
        if (uploaded && top->texture->MaterialsState()==rvk::Texture::MaterialState::Ready && g_modernStyle.load()==2) ready |= bit;
    }
    // Aggregate family diagnostics only. Shared artwork ownership is preserved,
    // but another resource's readiness cannot hide a valid material set.
    ready &= seen;
    for (uint32_t family=1;family<rvk::buildings::familyCount;++family)
        BuildingFamilyReady(family,bool(ready & (uint64_t(1)<<family)));
}

}  // namespace

void HotReload();
void PollModernPack();

void SetModernTextureStyle(unsigned style)
{
    if (g_modernStyle.exchange(style) != style)
        g_modernChanged.store(true);
}

void SetSharedTerrainSetting(bool enabled)
{
    if (g_sharedTerrainEnabled.exchange(enabled) != enabled) g_sharedTerrainChanged.store(true);
}

void SetCharacterMaterialPack(bool enabled)
{
    if (g_characterPack.exchange(enabled) != enabled)
        g_characterChanged.store(true);
}

void PollMaterialMaps()
{
    const bool pressure = FreeMaterialAddressSpace() < (384u << 20);
    static bool wasPressure = false;
    if (pressure && !g_cache.empty()) {
        g_cacheIndex.clear();
        g_cache.clear();
        g_cacheBytes = 0;
    }
    if (pressure != wasPressure) {
        wasPressure = pressure;
        RvkLog("materials: CPU decode cache %s (free process address space %llu MB)",
               pressure ? "released" : "available", (unsigned long long)(FreeMaterialAddressSpace() >> 20));
    }
    HotReload();
    PollModernPack();
    if (g_worker) {
        std::vector<Worker::Done> done;
        {
            std::lock_guard<std::mutex> lock(g_worker->mutex);
            done.swap(g_worker->done);
            g_worker->doneBytes = 0;
        }
        for (auto& [key, gen, decoded] : done) {
            if (gen != g_gen[key])
                continue;                          // decoded from files a hot reload replaced since
            if (!decoded) {
                g_deferred[key]=GetTickCount();
                g_waiting.erase(key);
                continue;
            }
            if (!decoded->errors.empty())
                RvkLog("materials: %s", decoded->errors.c_str());
            if (!pressure) CachePut(key, decoded);
            auto it = g_waiting.find(key);
            if (it == g_waiting.end())
                continue;
            for (const Waiter& w : it->second)
                g_uploads.emplace_back(w, decoded);
            g_waiting.erase(it);
        }
    }
    size_t bytes = 0;
    while (!g_uploads.empty() && bytes < kUploadBytesPerFrame) {
        auto [w, d] = std::move(g_uploads.front());
        g_uploads.pop_front();
        bytes += d->Bytes();
        Upload(w, *d);
    }
    if (g_worker) g_worker->wake.notify_one();
    // A low-memory skip must not permanently leave holes in the selected pack.
    // Retry a few live resources at a time, with hysteresis and a cooldown; no
    // setting toggles or zone reload required when the game releases memory.
    if (!g_deferred.empty() && FreeMaterialAddressSpace()>=(448u<<20)) {
        const DWORD now=GetTickCount();
        uint64_t keys[8]{};unsigned count=0;
        for (const auto& [key,when]:g_deferred)
            if (now-when>=2000 && count<8) keys[count++]=key;
        for (unsigned i=0;i<count;++i) {
            g_deferred.erase(keys[i]);
            for (RSurface* top:g_identified) {
                if (!top->texture || KeyOf(top->rdbType,top->rdbId)!=keys[i]) continue;
                top->materialsFor=nullptr;
                AttachMaterialMaps(top);
            }
        }
        if (count) RvkLog("materials: retrying %u deferred resource(s) after memory recovery",count);
    }
    PollBuildingFamilies();
}

namespace {
std::unordered_map<uint64_t, std::string> g_names;   // RDB texture -> the name the game gave it
}  // namespace

void ForgetMaterialMaps(RSurface* top)
{
    g_identified.erase(top);
    for (auto it=g_uploaded.begin();it!=g_uploaded.end();) {
        auto& owners=it->second;
        owners.erase(std::remove_if(owners.begin(),owners.end(),[top](const Waiter& a){return a.top==top;}),owners.end());
        if (owners.empty()) it=g_uploaded.erase(it); else ++it;
    }
    if (g_waiting.empty() && g_uploads.empty())
        return;
    for (auto& [key, list] : g_waiting)
        list.erase(std::remove_if(list.begin(), list.end(), [top](const Waiter& w) { return w.top == top; }),
                   list.end());
    g_uploads.erase(std::remove_if(g_uploads.begin(), g_uploads.end(),
                                   [top](const auto& u) { return u.first.top == top; }),
                    g_uploads.end());
}

namespace {

unsigned g_registered = 0;

unsigned g_ground = 0;

void Register(void* surface_t, uint32_t type, uint32_t id, const char* name)
{
    if (!surface_t || !Readable(surface_t, 4))
        return;
    auto* dds = *static_cast<IUnknown**>(surface_t);
    if (!dds)
        return;
    RSurface* s = nullptr;
    if (FAILED(dds->QueryInterface(IID_RvkSurface, reinterpret_cast<void**>(&s))) || !s)
        return;                                       // not our backend (D7VK): nothing to attach to
    RSurface* top = s->top ? s->top : s;
    top->rdbType = type;
    top->rdbId = id;
    g_identified.insert(top);
    if (name && Readable(name, 1)) {
        std::string& known = g_names[KeyOf(type, id)];
        if (known.empty())
            known.assign(name, strnlen(name, 120));
    }
    bool ground = type == 1010006 || type == 1010021 || type == 1010022;
    g_ground += ground;
    if (++g_registered <= 8 || (g_registered & (g_registered - 1)) == 0 || (ground && g_ground <= 4))
        RvkLog("materials: RDB texture %u:%u is a %lux%lu surface (%u textures identified)", type, id,
               top->desc.dwWidth, top->desc.dwHeight, g_registered);
    if (top->texture)
        AttachMaterialMaps(top);
    for (RSurface* level = top; level; level = level->nextLevel)
        level->CompactShadow();
    s->Release();
}

void RegisterCreator(void* surface, const void* creator, const char* name)
{
    if (ClassOf(creator) != Kind::AnarchyTexCreator || !Readable(creator, 0x48))
        return;
    static const unsigned index = ComIndex("rvk::Materials");
    ComScope scope(index);
    Register(surface, U32(creator, 0x40), U32(creator, 0x44), name);
}

using CreateFromBitmap = void*(__thiscall*)(void* self, void* bitmap, const char* name);
using CreateFromStream = void*(__thiscall*)(void* self, void* stream, const char* name);
using RTextureFromCreator = void*(__thiscall*)(void* self, const char* name, void* creator);

template <typename F>
F Original(const char* mangled)
{
    HMODULE orig = rnative::OriginalModule();
    return reinterpret_cast<F>(GetProcAddress(orig, mangled));
}

}  // namespace

std::string DescribeTexture(const rvk::Texture* texture, std::string* reference)
{
    if (!texture)
        return "nothing (no textured 3D surface there)";
    char buf[512];
    for (RSurface* top : g_identified) {
        if (top->texture != texture)
            continue;
        const uint32_t full = FullQualityType(top->rdbType);
        auto it = g_names.find(KeyOf(top->rdbType, top->rdbId));
        std::string maps;
        for (const char* suffix : kSuffixes)
            if (!FindMap(top->rdbType, top->rdbId, suffix).empty())
                maps += std::string(" ") + suffix;
        // (No name: what the game hands CreateTexture as one isn't the texture's - "a.?", "" - the workbench has them.)
        (void)it;
        const uint64_t key=KeyOf(top->rdbType,top->rdbId);
        const Folder& folder=FolderFor(top->rdbType,top->rdbId);
        const char* state="replacement artwork missing";
        if (folder.modern==2 && rvk::buildings::Managed(top->rdbType,top->rdbId) && !top->buildingFamily)
            state="original retained intentionally (unapproved artwork)";
        else if (folder.modern && rvk::materials::IsGroundType(top->rdbType) && !rvk::materials::AuthoredGround(top->rdbType,top->rdbId))
            state="original retained intentionally (ground colour/blend policy)";
        else if (!maps.empty() && !FilesFor(top->rdbType,top->rdbId).Any())
            state="replacement suppressed (eligibility or required map missing)";
        if (g_waiting.count(key)) {
            state="replacement queued";
            if (g_worker) {
                std::lock_guard<std::mutex> lock(g_worker->mutex);
                if (g_worker->decoding==key) state="replacement decoding";
                for (const auto& d:g_worker->done) if (d.key==key) state="replacement awaiting GPU upload";
            }
        }
        for (const auto& upload:g_uploads) if (upload.first.top==top) state="replacement awaiting GPU upload";
        if (g_deferred.count(key)) state="replacement deferred (process memory)";
        if (g_failed.count(key) || texture->MaterialsState()==rvk::Texture::MaterialState::Failed) state="replacement failed";
        if (texture->MaterialsState()==rvk::Texture::MaterialState::Pending) state="replacement awaiting GPU upload";
        if (texture->MaterialsState()==rvk::Texture::MaterialState::Ready) state="replacement GPU-ready";
        std::snprintf(buf, sizeof(buf), "RDB texture %u:%u, %lux%lu; %s; last selected draw %llu; available files:%s",
                      full, top->rdbId, top->desc.dwWidth, top->desc.dwHeight, state,
                      (unsigned long long)texture->MaterialDrawFrame(), maps.empty() ? " none" : maps.c_str());
        if (reference) {
            char ref[32];
            std::snprintf(ref, sizeof(ref), "%u:%u", full, top->rdbId);
            *reference = ref;
        }
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "a texture that isn't an RDB texture (%ux%u; made by the game itself)",
                  texture->Width(), texture->Height());
    return buf;
}

void AttachMaterialMaps(RSurface* top)
{
    if (!top || !top->rdbId || !top->texture || top->materialsFor == top->texture || !g_rvk.device)
        return;
    top->materialsFor = top->texture;
    const Folder& folder=FolderFor(top->rdbType,top->rdbId);
    top->buildingFamily=folder.modern==2 ? rvk::buildings::FamilyOf(top->rdbType,top->rdbId) : 0;
    g_rvk.device->SetTextureRdbId(top->texture, top->rdbId, top->rdbType, top->buildingFamily,
        folder.modern && rvk::materials::AuthoredGround(top->rdbType,top->rdbId));
    uint64_t key = KeyOf(top->rdbType, top->rdbId);
    Waiter waiter{top, top->texture};
    // A warm hit only shares existing objects, never changes family visibility.
    if (ShareUploaded(waiter)) return;
    if (g_deferred.count(key) || g_failed.count(key)) return;
    if (DecodedPtr cached = CacheGet(key)) {
        g_uploads.emplace_back(waiter, cached);
        return;
    }
    auto it = g_waiting.find(key);
    if (it != g_waiting.end()) {                  // being decoded already
        it->second.push_back(waiter);
        return;
    }
    rvk::maps::MaterialFiles files = FilesFor(top->rdbType, top->rdbId);
    if (!files.Any())
        return;
    g_waiting[key].push_back(waiter);
    if (!g_worker) {
        g_worker = new Worker;
        std::thread(WorkerLoop, g_worker).detach();
    }
    {
        std::lock_guard<std::mutex> lock(g_worker->mutex);
        g_worker->jobs.push_back({key, g_gen[key], std::move(files)});
    }
    g_worker->wake.notify_one();
}

// ---------------------------------------------------------------- hot reload
namespace {
HANDLE g_watch[2] = {};
bool g_watchTried = false;
DWORD g_lastCheck = 0, g_changedAt = 0;
bool g_changed = false;

void Changed(const std::unordered_map<std::string, uint64_t>& before, const std::unordered_map<std::string, uint64_t>& now,
             std::unordered_set<uint64_t>* keys)
{
    for (auto& [name, stamp] : now) {
        auto it = before.find(name);
        if (it == before.end() || it->second != stamp)
            if (uint64_t key = KeyOfName(name)) keys->insert(key);
    }
    for (auto& [name, stamp] : before)
        if (!now.count(name))
            if (uint64_t key = KeyOfName(name)) keys->insert(key);
}

void Reload(uint64_t key)
{
    ++g_gen[key];
    g_uploaded.erase(key); // generations/styles must never share different artwork
    g_deferred.erase(key);
    g_failed.erase(key);
    if (g_worker) {
        std::lock_guard<std::mutex> lock(g_worker->mutex);
        g_worker->jobs.erase(std::remove_if(g_worker->jobs.begin(), g_worker->jobs.end(),
                                          [key](const Worker::Job& job) { return job.key == key; }),
                             g_worker->jobs.end());
    }
    CacheErase(key);
    g_waiting.erase(key);
    g_uploads.erase(std::remove_if(g_uploads.begin(), g_uploads.end(),
                                   [key](const auto& u) { return KeyOf(u.first.top->rdbType, u.first.top->rdbId) == key; }),
                    g_uploads.end());
    const uint32_t type = uint32_t(key >> 32), id = uint32_t(key);
    const bool any = FilesFor(type, id).Any();
    unsigned surfaces = 0;
    for (RSurface* top : g_identified) {
        if (!top->texture || KeyOf(top->rdbType, top->rdbId) != key)
            continue;
        ++surfaces;
        top->materialsFor = nullptr;
        if (any)
            AttachMaterialMaps(top);
        else if (g_rvk.device)
            rvk::maps::Replace(*g_rvk.device, top->texture, rvk::maps::Decoded{});   // its maps are gone
    }
    RvkLog("hot reload: RDB texture %u:%u from %s, %u surface(s) in use", type, id,
           !any ? "nothing (maps removed)" : FolderFor(type, id).dir.c_str(), surfaces);
}
}  // namespace

void PollModernPack()
{
    const bool modernChanged = g_modernChanged.exchange(false);
    const bool characterChanged = g_characterChanged.exchange(false);
    const bool terrainChanged = g_sharedTerrainChanged.exchange(false);
    if (!modernChanged && !characterChanged && !terrainChanged)
        return;
    Scan();
    std::unordered_set<uint64_t> keys;
    for (RSurface* top : g_identified) {
        if (terrainChanged && FullQualityType(top->rdbType) == 1010006)
            keys.insert(KeyOf(top->rdbType, top->rdbId));
        for (const Folder& pack : g_packs)
            if ((pack.modern == 3 ? characterChanged : pack.modern && modernChanged) && HasAny(pack, top->rdbType, top->rdbId))
                keys.insert(KeyOf(top->rdbType, top->rdbId));
    }
    for (uint64_t key : keys)
        Reload(key);                            // cancels stale jobs and also removes maps when disabled
    if (modernChanged)
        RvkLog("modern texture pack: style %u; %u texture id(s) refreshed", g_modernStyle.load(), unsigned(keys.size()));
    if (characterChanged)
        RvkLog("character artwork pack: %s; %u texture id(s) refreshed", g_characterPack.load() ? "on" : "off", unsigned(keys.size()));
    if (terrainChanged)
        RvkLog("shared terrain materials: %s; %u texture id(s) refreshed", g_sharedTerrainEnabled.load() ? "on" : "off", unsigned(keys.size()));
}

void HotReload()
{
    Scan();
    if (!g_hot)
        return;
    const DWORD now = GetTickCount();
    if (now - g_lastCheck < 200)
        return;
    g_lastCheck = now;
    if (!g_watchTried) {
        g_watchTried = true;
        const Folder* folders[2] = {&g_mat, &g_live};
        for (int i = 0; i < 2; ++i) {
            HANDLE h = FindFirstChangeNotificationA(folders[i]->dir.c_str(), FALSE,
                                                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
                                                    FILE_NOTIFY_CHANGE_SIZE);
            g_watch[i] = h == INVALID_HANDLE_VALUE ? nullptr : h;
            if (!g_watch[i])
                RvkLog("hot reload: can't watch %s (error %lu)", folders[i]->dir.c_str(), GetLastError());
        }
    }
    for (HANDLE& h : g_watch)
        if (h && WaitForSingleObject(h, 0) == WAIT_OBJECT_0) {
            FindNextChangeNotification(h);
            g_changed = true;
            g_changedAt = now;
        }
    if (!g_changed || now - g_changedAt < 300)       // files may still be being written
        return;
    g_changed = false;
    auto mat = std::move(g_mat.files), live = std::move(g_live.files);
    ScanFolder(g_mat);
    ScanFolder(g_live);
    RebuildLiveKeys();
    std::unordered_set<uint64_t> keys;
    Changed(mat, g_mat.files, &keys);
    Changed(live, g_live.files, &keys);
    for (uint64_t key : keys)
        Reload(key);
}

// ---------------------------------------------------------------- mesh identity (mesh picking)
// Which RDB resource a visual was made from, for Ctrl+Shift+J. DisplaySystem loads meshes as DbObject_ts - RDBMesh_t
// (static meshes 1010001 / their variants 1010026) and RDBCATMesh_t (characters 1010002 / towers 1010027), +0x08 type,
// +0x0C id; the hooks below find the one being loaded on the caller's stack (as RTextureFromCreator finds an
// RDBGroundTexture_t).
// - Characters: DisplaySystem builds them with randy's exported RCATMesh_t(parent) constructor: the visual is known.
// - Static meshes are read from the .abiff's object archive; DisplaySystem's mesh classes call the exported base
//   RTriMesh_t::Archive on each mesh there. The meshes placed in the world are clones of those (their virtual Clone,
//   no export), which share the loaded mesh's data (RTriMesh_t +0x184, RVisualData_t; CloneMeshData un-shares it):
//   the loaded mesh's data is remembered, and a picked mesh is looked up by its data.
namespace {

// visual (RTriMesh_t / RCATMesh_t, by the address its constructor got) -> type << 32 | id. Ordered: the renderer
// sees a visual through the base class it draws with, a few bytes into the object (RCATMesh_t: +0x3C).
std::map<uintptr_t, uint64_t> g_meshIds;
constexpr uintptr_t kInsideObject = 0x200;
std::unordered_map<uint32_t, uint64_t> g_visualDataIds;  // RVisualData_t of a loaded static mesh -> its id
constexpr uint32_t kVisualData = 0x184;                  // RTriMesh_t: its RVisualData_t
unsigned g_meshFound = 0, g_meshMissed = 0;

// Reads another object's memory without faulting (ReadProcessMemory on ourselves fails cleanly where a plain read
// would crash: the stack holds every kind of value, and VirtualQuery's protection doesn't tell all unreadable pages).
bool SafeCopy(const void* p, void* out, size_t n)
{
    SIZE_T got = 0;
    return p && ReadProcessMemory(GetCurrentProcess(), p, out, n, &got) && got == n;
}

// The RTTI class name of a polymorphic object (".?AVName@@"), or "" - without ever faulting.
std::string SafeClassName(const void* object)
{
    uint32_t vtable = 0, col = 0, td = 0;
    char name[48] = {};
    if (!SafeCopy(object, &vtable, 4) || !vtable || !SafeCopy(reinterpret_cast<const void*>(uintptr_t(vtable) - 4), &col, 4) ||
        !SafeCopy(reinterpret_cast<const void*>(uintptr_t(col) + 12), &td, 4) ||
        !SafeCopy(reinterpret_cast<const void*>(uintptr_t(td) + 8), name, sizeof(name) - 1))
        return {};
    return std::string(name, strnlen(name, sizeof(name) - 1));
}

// The RDBMesh_t (character: RDBCATMesh_t) being loaded, from the caller's saved registers and stack. -> type << 32 | id
uint64_t FindLoading(const void* returnAddress, bool character)
{
    static std::unordered_map<uint32_t, Kind> byVtable;   // vtable -> class (only these two matter)
    static uint32_t words[4096];                          // (game thread only) up to 16 KB of stack: archives nest
    const auto* sp = reinterpret_cast<const uint32_t*>(returnAddress) - 8;
    size_t count = 4096;
    while (count && !SafeCopy(sp, words, count * 4))      // (near the top of the stack: fewer)
        count /= 2;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t v = words[i];
        if (!v || (v & 3) || v < 0x10000)
            continue;
        uint32_t head[4];                                 // vtable, ..., type (+8), id (+0xC)
        if (!SafeCopy(reinterpret_cast<const void*>(uintptr_t(v)), head, sizeof(head)) || !head[0])
            continue;
        auto it = byVtable.find(head[0]);
        if (it == byVtable.end()) {
            const std::string name = SafeClassName(reinterpret_cast<const void*>(uintptr_t(v)));
            it = byVtable.emplace(head[0], name == ".?AVRDBMesh_t@@" ? Kind::RDBMesh
                                           : name == ".?AVRDBCATMesh_t@@" ? Kind::RDBCATMesh : Kind::Other).first;
        }
        if (it->second != (character ? Kind::RDBCATMesh : Kind::RDBMesh))
            continue;
        const uint32_t type = head[2], id = head[3];
        const bool ok = character ? (type == 1010002 || type == 1010027) : (type == 1010001 || type == 1010026);
        if (ok && id)
            return uint64_t(type) << 32 | id;
    }
    return 0;
}

void Remember(const void* visual, uint32_t data, uint64_t id, const char* what)
{
    if (id) {
        g_meshIds[uintptr_t(visual)] = id;
        if (data) g_visualDataIds[data] = id;
        ++g_meshFound;
    } else {
        g_meshIds.erase(uintptr_t(visual));          // (a new object where an identified one was)
        ++g_meshMissed;
    }
    const unsigned n = g_meshFound + g_meshMissed;
    if (n <= 6 || (n & (n - 1)) == 0)
        RvkLog("meshes: %s %p %s%u:%u (%u identified, %u not)", what, visual, id ? "is RDB mesh " : "not identified ",
               unsigned(id >> 32), unsigned(id), g_meshFound, g_meshMissed);
}

extern "C" void rvk_ensure_init();          // trace.cpp: the native install, before the rva registry is used
extern "C" int rvk_exports_ready;
using RTriMeshArchive = void(__thiscall*)(const void* self, void* archive);
using RCATMeshCtor = void*(__thiscall*)(void* self, void* parent);
constexpr uint32_t kRTriMeshArchiveRva = 0x48F85, kRCATMeshCtorRva = 0x57FE0;   // interface/exports.tsv

}  // namespace

std::string DescribeMesh(uint32_t visual, uint32_t owner, uint32_t kind, std::string* reference)
{
    auto find = [](uint32_t p) -> uint64_t {                // the object p points into, if identified
        auto it = g_meshIds.upper_bound(uintptr_t(p));
        if (it == g_meshIds.begin()) return 0;
        --it;
        return uintptr_t(p) - it->first < kInsideObject ? it->second : 0;
    };
    char buf[256];
    if (!visual)
        return "nothing (no 3D object there)";
    // A character's own body (kind 1) is its RCATMesh_t; a piece attached to it (2) is a mesh of its own.
    uint64_t id = find(visual);
    if (!id && kind == 1u) id = find(owner);
    if (!id) {
        // A static mesh: a clone of a loaded one, sharing its data. The renderer has the visual through the base it
        // draws with; the complete object (RTTI: the vtable's locator +4 is this base's offset in it) is the mesh.
        uint32_t vtable = 0, col = 0, offset = 0, data = 0;
        if (SafeCopy(reinterpret_cast<const void*>(uintptr_t(visual)), &vtable, 4) &&
            SafeCopy(reinterpret_cast<const void*>(uintptr_t(vtable) - 4), &col, 4) &&
            SafeCopy(reinterpret_cast<const void*>(uintptr_t(col) + 4), &offset, 4) &&
            SafeCopy(reinterpret_cast<const void*>(uintptr_t(visual - offset + kVisualData)), &data, 4)) {
            auto it = g_visualDataIds.find(data);
            if (it != g_visualDataIds.end()) id = it->second;
        }
    }
    const uint64_t character = owner && owner != visual ? find(owner) : 0;
    if (!id) {
        std::snprintf(buf, sizeof(buf), "a visual %p not identified (made before randy-vk saw it, or not from an RDB "
                      "mesh)%s", reinterpret_cast<void*>(uintptr_t(visual)), character ? "; on a character" : "");
        if (character && reference) {
            std::snprintf(buf + std::strlen(buf), sizeof(buf) - std::strlen(buf), " %u:%u", unsigned(character >> 32),
                          unsigned(character));
            char ref[32];
            std::snprintf(ref, sizeof(ref), "%u:%u", unsigned(character >> 32), unsigned(character));
            *reference = ref;
        }
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "RDB %s %u:%u", (id >> 32) == 1010002 || (id >> 32) == 1010027 ? "character model"
                  : "mesh", unsigned(id >> 32), unsigned(id));
    if (character && character != id)
        std::snprintf(buf + std::strlen(buf), sizeof(buf) - std::strlen(buf), " (worn by character model %u:%u)",
                      unsigned(character >> 32), unsigned(character));
    if (reference) {
        char ref[32];
        std::snprintf(ref, sizeof(ref), "%u:%u", unsigned(id >> 32), unsigned(id));
        *reference = ref;
    }
    return buf;
}

}  // namespace rvkproxy

using namespace rvkproxy;

// RTriMesh_t::Archive(ObjectArchive_c*) const - a static mesh read from its .abiff - and RCATMesh_t::RCATMesh_t(RRefFrame_t*)
// - a character being built: mesh identity (above).
extern "C" void __fastcall rvk_RTriMeshArchive(const void* self, void*, void* archive)
{
    const uint64_t id = FindLoading(_AddressOfReturnAddress(), false);
    if (!rvk_exports_ready) rvk_ensure_init();      // (the exports' thunks do this on their first call)
    // Through the rva registry, as the export's own thunk would go: the native port's version where it has one.
    static auto original = reinterpret_cast<RTriMeshArchive>(rnative::AddressFor(kRTriMeshArchiveRva));
    original(self, archive);
    uint32_t data = 0;
    SafeCopy(static_cast<const uint8_t*>(self) + kVisualData, &data, 4);
    static const unsigned index = ComIndex("rvk::Meshes");
    ComScope scope(index);
    Remember(self, data, id, "static");
}

extern "C" void* __fastcall rvk_RCATMeshCtor(void* self, void*, void* parent)
{
    const uint64_t id = FindLoading(_AddressOfReturnAddress(), true);
    if (!rvk_exports_ready) rvk_ensure_init();      // (the exports' thunks do this on their first call)
    static auto original = reinterpret_cast<RCATMeshCtor>(rnative::AddressFor(kRCATMeshCtorRva));
    void* r = original(self, parent);
    static const unsigned index = ComIndex("rvk::Meshes");
    ComScope scope(index);
    Remember(self, 0, id, "character");
    return r;
}

// The hooked exports (thiscall: `this` in ecx; __fastcall's unused edx keeps the stack arguments in place).
extern "C" void* __fastcall rvk_CreateTextureBitmap(void* self, void*, void* bitmap, const char* name)
{
    static auto original = Original<CreateFromBitmap>("?CreateTexture@TextureStreamCreator@@QAEPAVsurface_t@@PAVLBitmap_t@@PBD@Z");
    void* surface = original(self, bitmap, name);
    RegisterCreator(surface, self, name);
    return surface;
}

extern "C" void* __fastcall rvk_CreateTextureStream(void* self, void*, void* stream, const char* name)
{
    static auto original = Original<CreateFromStream>("?CreateTexture@TextureStreamCreator@@QAEPAVsurface_t@@PAVPositionIO_t@fun@@PBD@Z");
    void* surface = original(self, stream, name);
    RegisterCreator(surface, self, name);
    return surface;
}

// RTexture_t::RTexture_t(char const*, TextureCreator*): world textures are identified by the CreateTexture hooks above;
// a plain creator may be an RDBGroundTexture_t's. Its `this` is in a callee-saved register of the caller, which this
// function's prologue pushes just below its return address, or on the caller's stack above it.
extern "C" void* __fastcall rvk_RTextureFromCreator(void* self, void*, const char* name, void* creator)
{
    const void* maker = nullptr;
    if (ClassOf(creator) != Kind::AnarchyTexCreator) {
        auto* sp = reinterpret_cast<const uint32_t*>(_AddressOfReturnAddress()) - 8;
        for (int i = 0; !maker && i < 264 && Readable(sp + i, 4); ++i)
            if (IsGroundMaker(reinterpret_cast<const void*>(uintptr_t(sp[i]))))
                maker = reinterpret_cast<const void*>(uintptr_t(sp[i]));
    }
    static auto original = Original<RTextureFromCreator>("??0RTexture_t@@QAE@PBDPAVTextureCreator@@@Z");
    void* texture = original(self, name, creator);
    if (maker && texture && Readable(static_cast<uint8_t*>(texture) + 0x30, 4)) {
        static const unsigned index = ComIndex("rvk::Materials");
        ComScope scope(index);
        Register(*reinterpret_cast<void**>(static_cast<uint8_t*>(texture) + 0x30), U32(maker, 0x08), U32(maker, 0x0C),
                 name);
    }
    return texture;
}

