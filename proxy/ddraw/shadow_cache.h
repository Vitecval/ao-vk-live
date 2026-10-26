// Lossless storage for cold CPU texture copies. GPU images and game pixels are
// unchanged. All calls in the proxy are serialized by ComScope.
#pragma once
#include <windows.h>
#include <compressapi.h>
#include <cstdint>
#include <algorithm>
#include <new>
#include <vector>

namespace rvkproxy::shadow_cache {
struct FileCopy { uint64_t offset = 0; uint32_t bytes = 0, checksum = 0; };

// Process-private, delete-on-close backing for cold compressed pixels. Nothing
// stays mapped in the game's address space; freed extents are reused across zones.
class BackingStore {
    HANDLE file = INVALID_HANDLE_VALUE;
    uint64_t end = 0;
    uint32_t live = 0;
    bool allowed;
    struct Extent { uint64_t offset, bytes; };
    std::vector<Extent> free;
    static uint32_t Hash(const std::vector<uint8_t>& bytes) noexcept {
        uint32_t hash=2166136261u;
        for (uint8_t b:bytes) hash=(hash^b)*16777619u;
        return hash;
    }
    bool Open() noexcept {
        if (file != INVALID_HANDLE_VALUE) return true;
        if (!allowed) return false;
        wchar_t directory[MAX_PATH+1], path[MAX_PATH+1];
        DWORD n = GetTempPathW(MAX_PATH, directory);
        if (!n || n >= MAX_PATH || !GetTempFileNameW(directory,L"avk",0,path)) return false;
        file = CreateFileW(path,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,
                           nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE|
                           FILE_FLAG_RANDOM_ACCESS,nullptr);
        if (file == INVALID_HANDLE_VALUE) DeleteFileW(path);
        return file != INVALID_HANDLE_VALUE;
    }
    bool Seek(uint64_t offset) noexcept {
        LARGE_INTEGER at; at.QuadPart = static_cast<LONGLONG>(offset);
        return SetFilePointerEx(file,at,nullptr,FILE_BEGIN) != FALSE;
    }
public:
    explicit BackingStore(bool enable = true) : allowed(enable) {}
    ~BackingStore() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
    BackingStore(const BackingStore&) = delete;
    BackingStore& operator=(const BackingStore&) = delete;
    bool Store(std::vector<uint8_t>& bytes, FileCopy& copy) noexcept {
        if (bytes.empty() || bytes.size() > UINT32_MAX || copy.bytes || !Open()) return false;
        size_t slot = free.size();
        for (size_t i=0;i<free.size();++i)
            if (free[i].bytes >= bytes.size() && (slot==free.size() || free[i].bytes<free[slot].bytes)) slot=i;
        uint64_t offset = slot==free.size() ? end : free[slot].offset;
        if (offset+bytes.size() > (2ull<<30)) return false;
        DWORD written = 0;
        if (!Seek(offset) || !WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr) || written!=bytes.size()) return false;
        if (slot==free.size()) end=offset+bytes.size();
        else {
            free[slot].offset+=bytes.size(); free[slot].bytes-=bytes.size();
            if (!free[slot].bytes) free.erase(free.begin()+slot);
        }
        copy={offset,uint32_t(bytes.size()),Hash(bytes)}; ++live;
        std::vector<uint8_t>().swap(bytes);
        return true;
    }
    bool Read(const FileCopy& copy, std::vector<uint8_t>& output) {
        if (!copy.bytes || file==INVALID_HANDLE_VALUE) return false;
        std::vector<uint8_t> bytes(copy.bytes);
        DWORD read = 0;
        if (!Seek(copy.offset) || !ReadFile(file,bytes.data(),copy.bytes,&read,nullptr) || read!=copy.bytes || Hash(bytes)!=copy.checksum) return false;
        output.swap(bytes);
        return true;
    }
    void Release(FileCopy& copy) noexcept {
        if (!copy.bytes) return;
        try {
            free.push_back({copy.offset,copy.bytes});
            std::sort(free.begin(),free.end(),[](const Extent& a,const Extent& b){return a.offset<b.offset;});
            size_t count=0;
            for (const auto& range:free) {
                if (count && free[count-1].offset+free[count-1].bytes==range.offset) free[count-1].bytes+=range.bytes;
                else free[count++]=range;
            }
            free.resize(count);
        } catch (const std::bad_alloc&) { /* An unreclaimed hole cannot invalidate live copies. */ }
        copy={};
        if (!--live) {
            CloseHandle(file); file=INVALID_HANDLE_VALUE; end=0;
            std::vector<Extent>().swap(free);
        }
    }
    uint64_t FileBytes() const noexcept { return end; }
};
inline BackingStore& GetBackingStore() {
    // Surfaces may outlive CRT singleton destruction. Windows closes this private
    // delete-on-close handle at process exit, including abnormal termination.
    alignas(BackingStore) static unsigned char memory[sizeof(BackingStore)];
    static BackingStore* store = new(memory) BackingStore;
    return *store;
}

struct Codec {
    COMPRESSOR_HANDLE compressor = nullptr;
    DECOMPRESSOR_HANDLE decompressor = nullptr;
    Codec() {
        CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor);
        CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor);
    }
    ~Codec() {
        if (compressor) CloseCompressor(compressor);
        if (decompressor) CloseDecompressor(decompressor);
    }
};
inline Codec& GetCodec() { static Codec codec; return codec; }

inline bool Compact(std::vector<uint8_t>& raw, std::vector<uint8_t>& packed) noexcept
{
    if (raw.size() < 16384 || !packed.empty()) return false;
    auto& codec = GetCodec();
    if (!codec.compressor || !codec.decompressor) return false;
    try {
        // A failed or unprofitable compression leaves the only valid copy intact.
        std::vector<uint8_t> temporary(raw.size() * 7 / 8);
        SIZE_T bytes = 0;
        if (!Compress(codec.compressor, raw.data(), raw.size(), temporary.data(), temporary.size(), &bytes) ||
            !bytes || bytes >= temporary.size()) return false;
        std::vector<uint8_t> exact(temporary.begin(), temporary.begin() + bytes);
        packed.swap(exact);
        std::vector<uint8_t>().swap(raw);
        return true;
    } catch (const std::bad_alloc&) { return false; }
}

// Transactional: on allocation/API failure packed stays valid for another attempt.
inline bool Restore(std::vector<uint8_t>& raw, std::vector<uint8_t>& packed, size_t size)
{
    if (packed.empty()) {
        if (raw.size() != size) raw.assign(size, 0);
        return true;
    }
    auto& codec = GetCodec();
    if (!codec.decompressor) return false;
    std::vector<uint8_t> restored(size);
    SIZE_T bytes = 0;
    if (!Decompress(codec.decompressor, packed.data(), packed.size(), restored.data(), restored.size(), &bytes) ||
        bytes != size) return false;
    raw.swap(restored);
    std::vector<uint8_t>().swap(packed);
    return true;
}
} // namespace rvkproxy::shadow_cache
