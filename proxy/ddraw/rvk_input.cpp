// The game kept from the keyboard and the mouse while the free camera (rvk_freecam.cpp) or the console
// (rvk_console.cpp) has them: each owner asks for the keyboard, the mouse or both (InputBlock).
//
// - DirectInput (DisplaySystem.dll's keyboard and mouse) reads nothing: the device method table every DirectInput
//   device of the process shares is found by making a device of our own, and its GetDeviceState / GetDeviceData give
//   an idle state.
// - The game's window is subclassed (InputAttach, once; inert while nothing blocks) and gets no key, mouse or raw
//   input messages of the kind held back. The console sees every message first (ConsoleMessage): its key opens it
//   at any time, and while open it takes the typing.
// Releases always go through: a key or button held when blocking began must not stay held for the game.
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstring>

#include "rvk_backend.h"
#include "rvk_zoning.h"
#include "presentation_policy.h"

namespace rvkproxy {

namespace {

std::atomic<unsigned> g_blockKeys{0}, g_blockMouse{0};   // owners (InputOwner bits) holding each back

// ---------------------------------------------------------------- DirectInput: an idle state while blocking
using GetStateFn = HRESULT(__stdcall*)(void*, DWORD, void*);
using GetDataFn = HRESULT(__stdcall*)(void*, DWORD, void*, DWORD*, DWORD);
struct Patched { void** vtable; GetStateFn state; GetDataFn data; };
Patched g_patched[4];
int g_patchedCount = 0;
bool g_dinputTried = false;

const Patched* PatchedFor(void* self)
{
    void** vtable = *static_cast<void***>(self);
    for (int i = 0; i < g_patchedCount; ++i)
        if (g_patched[i].vtable == vtable) return &g_patched[i];
    return nullptr;
}

bool IsMouse(void* self);

// Whether the game is kept from this device now (a keyboard or a mouse).
bool Blocked(void* self)
{
    const unsigned keys = g_blockKeys, mouse = g_blockMouse;
    return (keys || mouse) && (IsMouse(self) ? mouse != 0 : keys != 0);
}

HRESULT __stdcall HookGetDeviceState(void* self, DWORD bytes, void* data)
{
    const Patched* p = PatchedFor(self);
    if (!p) return E_FAIL;
    HRESULT r = p->state(self, bytes, data);
    if (SUCCEEDED(r) && data && Blocked(self)) std::memset(data, 0, bytes);   // (nothing pressed, no movement)
    return r;
}

// Whether a device is a mouse (its type: DIDEVICEINSTANCEA.dwDevType's low byte 2; IDirectInputDevice slot 15
// GetDeviceInfo), asked once each.
bool IsMouse(void* self)
{
    static void* known[8];
    static bool mouse[8];
    static int count = 0;
    for (int i = 0; i < count; ++i)
        if (known[i] == self) return mouse[i];
    struct { DWORD size; GUID instance, product; DWORD type; char names[2 * 260]; GUID ff; WORD page, usage; } info{};
    info.size = sizeof(info);
    using GetInfo = HRESULT(__stdcall*)(void*, void*);
    const bool m = SUCCEEDED(reinterpret_cast<GetInfo>((*static_cast<void***>(self))[15])(self, &info)) &&
                   (info.type & 0xFF) == 2;
    if (count < 8) { known[count] = self; mouse[count++] = m; }
    return m;
}

HRESULT __stdcall HookGetDeviceData(void* self, DWORD objectBytes, void* data, DWORD* count, DWORD flags)
{
    const Patched* p = PatchedFor(self);
    if (!p) return E_FAIL;
    HRESULT r = p->data(self, objectBytes, data, count, flags);   // (drained as usual)
    if (SUCCEEDED(r) && count && Blocked(self)) {
        // Only the releases kept (a key, a button let go): held when it went on (Ctrl+Shift+C itself), the game would
        // think them held still. (DIDEVICEOBJECTDATA: offset, data - bit 7 down -, ...; a mouse's axes at offsets
        // 0 / 4 / 8: dropped.)
        if (!data || objectBytes < 8) {
            *count = 0;
        } else {
            const bool mouse = IsMouse(self);
            uint8_t* d = static_cast<uint8_t*>(data);
            DWORD kept = 0;
            for (DWORD i = 0; i < *count; ++i) {
                const DWORD* e = reinterpret_cast<const DWORD*>(d + size_t(i) * objectBytes);
                if ((e[1] & 0x80) == 0 && !(mouse && e[0] < 12)) {
                    if (kept != i) std::memmove(d + size_t(kept) * objectBytes, e, objectBytes);
                    ++kept;
                }
            }
            *count = kept;
        }
    }
    return r;
}

void PatchVtable(void** vtable)
{
    if (!vtable || g_patchedCount >= 4) return;
    for (int i = 0; i < g_patchedCount; ++i)
        if (g_patched[i].vtable == vtable) return;
    DWORD old;
    if (!VirtualProtect(&vtable[9], sizeof(void*) * 2, PAGE_EXECUTE_READWRITE, &old)) return;
    Patched& p = g_patched[g_patchedCount++];
    p.vtable = vtable;
    p.state = reinterpret_cast<GetStateFn>(vtable[9]);     // IDirectInputDevice: 9 GetDeviceState, 10 GetDeviceData
    p.data = reinterpret_cast<GetDataFn>(vtable[10]);
    vtable[9] = reinterpret_cast<void*>(&HookGetDeviceState);
    vtable[10] = reinterpret_cast<void*>(&HookGetDeviceData);
    VirtualProtect(&vtable[9], sizeof(void*) * 2, old, &old);
}

// The keyboard's and mouse's method tables: from devices of our own (DirectInputCreateA, version 7, as the game's).
void PatchDirectInput()
{
    if (g_dinputTried) return;
    g_dinputTried = true;
    HMODULE dinput = LoadLibraryA("dinput.dll");
    auto create = dinput ? reinterpret_cast<HRESULT(__stdcall*)(HINSTANCE, DWORD, void**, void*)>(
                               GetProcAddress(dinput, "DirectInputCreateA")) : nullptr;
    void* di = nullptr;
    if (!create || FAILED(create(GetModuleHandleA(nullptr), 0x0700, &di, nullptr)) || !di) {
        RvkLog("input: no DirectInput - the game's own input not held back");
        return;
    }
    static const GUID kKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
    static const GUID kMouse = {0x6F1D2B60, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};
    using CreateDevice = HRESULT(__stdcall*)(void*, const GUID&, void**, void*);
    using Release = ULONG(__stdcall*)(void*);
    void** diVtable = *static_cast<void***>(di);
    for (const GUID* guid : {&kKeyboard, &kMouse}) {
        void* dev = nullptr;
        if (SUCCEEDED(reinterpret_cast<CreateDevice>(diVtable[3])(di, *guid, &dev, nullptr)) && dev) {
            PatchVtable(*static_cast<void***>(dev));
            reinterpret_cast<Release>((*static_cast<void***>(dev))[2])(dev);
        }
    }
    reinterpret_cast<Release>(diVtable[2])(di);
    RvkLog("input: DirectInput's keyboard and mouse can be held back (%d method tables)", g_patchedCount);
}

// ---------------------------------------------------------------- the window
HWND g_window = nullptr;
constexpr wchar_t kPreviousWindowProc[] = L"AO-VK.Input.PreviousWindowProc";
std::atomic<HWND> g_presentationWindow{nullptr};
std::atomic<bool> g_presentationMinimized{false};
std::atomic<unsigned> g_erasesPrevented{0}, g_paints{0}, g_sizes{0}, g_displayChanges{0};
std::atomic<DWORD> g_lastEraseTick{0};

// A raw input message's device kind (RIM_TYPEMOUSE / KEYBOARD / HID).
DWORD RawInputType(LPARAM lp)
{
    RAWINPUTHEADER header{};
    UINT size = sizeof(header);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_HEADER, &header, &size, sizeof(RAWINPUTHEADER)) ==
        UINT(-1))
        return RIM_TYPEHID;
    return header.dwType;
}

LRESULT CALLBACK InputWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Keep the original procedure with its own HWND: replacing the render window
    // must not send late messages from the old window to the new one's handler.
    const auto previous = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kPreviousWindowProc));
    if (!previous) return DefWindowProcA(hwnd, msg, wp, lp);
    if (msg == WM_NCDESTROY) {
        HWND owned = hwnd;
        g_presentationWindow.compare_exchange_strong(owned, nullptr);
        if (g_window == hwnd) g_window = nullptr;
        const LRESULT result = CallWindowProcA(previous, hwnd, msg, wp, lp);
        RemovePropW(hwnd, kPreviousWindowProc);
        return result;
    }
    if (hwnd == g_presentationWindow.load(std::memory_order_acquire)) {
        // GDI must not replace a Vulkan-owned picture with the class background
        // between presents. Forward WM_PAINT normally so AO validates its update
        // region and keeps receiving its ordinary window notifications.
        if (msg == WM_ERASEBKGND) {
            ++g_erasesPrevented;
            g_lastEraseTick = GetTickCount();
            return 1;
        }
        if (msg == WM_PAINT) ++g_paints;
        if (msg == WM_SIZE) {
            ++g_sizes;
            if(wp==SIZE_MINIMIZED)g_presentationMinimized.store(true,std::memory_order_release);
            else if(rvk::presentation::UsableClient(LOWORD(lp),HIWORD(lp)))g_presentationMinimized.store(false,std::memory_order_release);
        }
        if (msg == WM_DISPLAYCHANGE) ++g_displayChanges;
    }
    if (msg == WM_SIZE)
        BackendWindowResized(hwnd);
    if (msg == WM_DESTROY)
        BackendWindowDestroyed(hwnd);
    // A confirmation screen may wait indefinitely. Keep the window-close
    // shortcut available while its keyboard block is held.
    if (msg == WM_SYSKEYDOWN && wp == VK_F4 && (g_blockKeys.load() & kInputZoning))
        return CallWindowProcA(previous, hwnd, msg, wp, lp);
    if (ZoningMessage(hwnd, msg, wp, lp) || ConsoleMessage(hwnd, msg, wp, lp))
        return 0;
    const unsigned keys = g_blockKeys, mouse = g_blockMouse;
    if (keys || mouse) {
        if (msg == WM_MOUSEWHEEL && (mouse & kInputFreeCam)) {
            FreeCamWheel(float(GET_WHEEL_DELTA_WPARAM(wp)) / float(WHEEL_DELTA));
            return 0;
        }
        if (msg == WM_SETCURSOR && mouse)
            return DefWindowProcA(hwnd, msg, wp, lp);
        if (msg == WM_INPUT) {
            const DWORD type = RawInputType(lp);
            if ((type == RIM_TYPEKEYBOARD && keys) || (type == RIM_TYPEMOUSE && mouse))
                return DefWindowProcA(hwnd, msg, wp, lp);
        }
        const bool release = msg == WM_KEYUP || msg == WM_SYSKEYUP || msg == WM_LBUTTONUP || msg == WM_RBUTTONUP ||
                             msg == WM_MBUTTONUP || msg == WM_XBUTTONUP;
        if (!release && ((keys && msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
                         (mouse && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)))
            return 0;
    }
    return CallWindowProcA(previous, hwnd, msg, wp, lp);
}

}  // namespace

void InputAttach(HWND window)
{
    if (!window || g_window == window) return;
    if (!GetPropW(window, kPreviousWindowProc)) {
        const auto previous = reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window, GWLP_WNDPROC));
        if (!previous || !SetPropW(window, kPreviousWindowProc, reinterpret_cast<HANDLE>(previous))) return;
        SetLastError(0);
        const LONG_PTR replaced = SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&InputWindowProc));
        if (!replaced && GetLastError()) {
            RemovePropW(window, kPreviousWindowProc);
            return;
        }
    }
    g_window = window;
}

void InputSetPresentationWindow(HWND window)
{
    // The window callback must never take the backend lock or wait for the GPU.
    if(g_presentationWindow.load(std::memory_order_acquire)!=window)
        g_presentationMinimized.store(window&&IsIconic(window),std::memory_order_release);
    g_presentationWindow.store(window, std::memory_order_release);
}

bool InputWindowMinimized(HWND window){return g_presentationWindow.load(std::memory_order_acquire)==window&&g_presentationMinimized.load(std::memory_order_acquire);}

void InputLogWindowEvents()
{
    const unsigned erases = g_erasesPrevented.exchange(0), paints = g_paints.exchange(0);
    const unsigned sizes = g_sizes.exchange(0), displays = g_displayChanges.exchange(0);
    if (!(erases || paints || sizes || displays)) return;
    SYSTEMTIME t{}; GetLocalTime(&t);
    RvkLog("window repaint: %u background clears prevented, %u paints, %u size events, %u display changes; "
           "last erase tick %lu, logged %02d:%02d:%02d.%03d", erases, paints, sizes, displays,
           g_lastEraseTick.load(), t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
}

void InputBlock(unsigned owner, bool keys, bool mouse)
{
    if (keys || mouse)
        PatchDirectInput();
    if (keys) g_blockKeys |= owner; else g_blockKeys &= ~owner;
    if (mouse) g_blockMouse |= owner; else g_blockMouse &= ~owner;
}

bool InputFocused()
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

}  // namespace rvkproxy
