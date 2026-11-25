#include "native/tooltip_delay.h"
#include "native/client_build.h"
#include "native/native.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace rnative::tooltips {
namespace {
constexpr uint32_t kDelayRva=0x00157C7B;
constexpr LONG kOriginalDelay=500;
constexpr uint8_t kOriginal[]={0x05,0xF4,0x01,0x00,0x00};
bool Executable(const void* p,size_t size) {
    MEMORY_BASIC_INFORMATION m{};
    if(!VirtualQuery(p,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const DWORD access=m.Protect&0xFF;
    return (access==PAGE_EXECUTE_READ || access==PAGE_EXECUTE_READWRITE || access==PAGE_EXECUTE_WRITECOPY) &&
        reinterpret_cast<uintptr_t>(p)+size<=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
}
std::atomic<bool> desired{false};
std::mutex lock;
DelayPatch patch;
}
bool DelayPatch::Refuse(const char* reason) {
    if(lastReason_!=reason)Log("instant tooltips: refused - %s; GUI.dll file unchanged",reason);
    lastReason_=reason;return false;
}
bool DelayPatch::Apply(HMODULE module,bool instant) {
    if(module_!=module && changed_) {
        // A vanished module no longer has an operand to restore. Never follow
        // its old pointer into a subsequently mapped allocation.
        changed_=false;module_=nullptr;
    }
    if(!instant && !changed_)return true;
    if(!module)return Refuse("GUI.dll is not loaded; waiting for a game frame");
    if(IdentifyClientModule(module,ClientModule::Gui)!=ClientBuild::Live)
        return Refuse("unsupported GUI.dll client build (only the audited classic Live build is supported)");
    auto* code=reinterpret_cast<uint8_t*>(module)+kDelayRva;
    if(!Executable(code,sizeof(kOriginal)))return Refuse("tooltip instruction is not in readable executable memory");
    const LONG previous=changed_ ? 0:kOriginalDelay;
    uint8_t expected[sizeof(kOriginal)];std::memcpy(expected,kOriginal,sizeof(expected));
    std::memcpy(expected+1,&previous,sizeof(previous));
    if(std::memcmp(code,expected,sizeof(expected))!=0)
        return Refuse("expected add eax,500 instruction or our owned zero-delay operand does not match");
    const LONG next=instant ? 0:kOriginalDelay;
    if(previous==next){lastReason_=nullptr;return true;}
    auto* operand=reinterpret_cast<volatile LONG*>(code+1);
    if(reinterpret_cast<uintptr_t>(operand)%alignof(LONG))return Refuse("tooltip delay operand is not aligned");
    DWORD protection=0;
    if(!VirtualProtect(code,sizeof(kOriginal),PAGE_EXECUTE_READWRITE,&protection))
        return Refuse("Windows denied temporary write access to the tooltip instruction");
    // The operand at RVA+1 is four-byte aligned. Atomic exchange leaves another
    // game thread seeing either complete delay, and detects competing patches.
    const bool exchanged=InterlockedCompareExchange(operand,next,previous)==previous;
    const bool flushed=exchanged && FlushInstructionCache(GetCurrentProcess(),code,sizeof(kOriginal));
    DWORD ignored=0;
    if(!exchanged || !flushed) {
        if(exchanged)InterlockedCompareExchange(operand,previous,next);
        FlushInstructionCache(GetCurrentProcess(),code,sizeof(kOriginal));
        const bool restored=VirtualProtect(code,sizeof(kOriginal),protection,&ignored)!=FALSE;
        return Refuse(!restored ? "could not restore tooltip page protection after a failed change" :
            !exchanged ? "tooltip operand changed concurrently; no overwrite performed" : "instruction-cache flush failed; change rolled back");
    }
    if(!VirtualProtect(code,sizeof(kOriginal),protection,&ignored)) {
        InterlockedCompareExchange(operand,previous,next);
        FlushInstructionCache(GetCurrentProcess(),code,sizeof(kOriginal));
        VirtualProtect(code,sizeof(kOriginal),protection,&ignored);
        return Refuse("could not restore tooltip page protection; delay change rolled back");
    }
    module_=module;changed_=instant;lastReason_=nullptr;
    Log("instant tooltips: native interface hover delay %ld ms (GUI.dll %p + RVA 0x%08X); next rendered frame eligible at zero",next,module,kDelayRva);
    return true;
}
void SetInstant(bool enabled) {
    std::lock_guard<std::mutex> guard(lock);desired=enabled;
    patch.Apply(GetModuleHandleW(L"GUI.dll"),enabled);
}
void Poll() {
    std::lock_guard<std::mutex> guard(lock);
    if(desired.load() || patch.Changed())patch.Apply(GetModuleHandleW(L"GUI.dll"),desired.load());
}
void Restore() {
    std::lock_guard<std::mutex> guard(lock);
    patch.Apply(GetModuleHandleW(L"GUI.dll"),false);
}
}
