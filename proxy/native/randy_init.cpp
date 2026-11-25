// Randy_t::Initialize natively (part of [Native] Device=on): the render_t, DirectDraw (cooperative level, display
// mode in full screen), the primary surface and its back buffer or an offscreen frame buffer, the Z-buffer, Direct3D
// and its device - with the original's checks of the hardware (3dfx and Kyro by name, S3 Savage and early NVIDIA by
// ID) - and Randy_t's construction and destruction.
//
// Globals: 0x1016BED0 render_t::m_pcInstance (render_t, 0xACC bytes: +0 IDirect3DDevice7, +4 IDirect3D7, +8
// IDirectDraw7, +0x278 a RECT), 0x100B772C Randy_t::s_eHardwareLevel; 0x1017D2A4 the window's screen rectangle,
// 0x1017D2F0 IDirectDraw7, 0x1017D2F4 the primary surface_t, 0x1017D2F8 render target 0 (the frame buffer),
// 0x1017D314 IDirect3D7, 0x1017D318 IDirect3DDevice7, 0x1017D31C full screen, 0x1017D31D a 3dfx card, 0x1017D31E a
// Kyro, 0x1017D31F / 0x1017D330 DXT1 / DXT3 textures, 0x1017D320 the window, 0x1017D324 / 28 / 2C colour, Z and
// stencil bits, 0x1017D331 an early NVIDIA card, 0x1017D332 an S3 Savage (Randy_t::NeedsClearFix).
// render_t's wrappers throw a fun::DXError when a call fails (but DDERR_SURFACEBUSY); so do ours (DisplaySystem
// catches it).
#include "native/randy_init.h"
#include "native/devices.h"
#include "native/dxerror.h"
#include "native/orig_api.gen.h"
#include "native/vc10.h"
#include "display_mode_win32.h"
#include "framebuffer_transaction.h"
#include "native/resize_queue_policy.h"
#include "native/main_surface_policy.h"
#include "window_callback_policy.h"

#include <ddraw.h>

#include <cstring>
#include <array>
#include <atomic>

namespace rnative::randyinit {

namespace {

HMODULE g_orig;

template <typename T>
T& Global(uint32_t rva) { return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(g_orig) + rva); }
template <typename T>
T& Field(void* object, uint32_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset); }
template <typename F>
F Internal(uint32_t rva) { return reinterpret_cast<F>(AddressFor(rva)); }

template <typename... A>
HRESULT Com(void* object, uint32_t offset, A... args)
{
    using Fn = HRESULT(__stdcall*)(void*, A...);
    return reinterpret_cast<Fn>((*static_cast<void***>(object))[offset / 4])(object, args...);
}

constexpr uint32_t kRender = 0x16BED0, kHardwareLevel = 0xB772C, kWindowRect = 0x17D2A4, kDirectDraw = 0x17D2F0,
                   kPrimary = 0x17D2F4, kRenderTargets = 0x17D2F8, kDirect3D = 0x17D314, kDevice = 0x17D318,
                   kFullscreen = 0x17D31C, k3dfx = 0x17D31D, kKyro = 0x17D31E, kDxt1 = 0x17D31F, kWindow = 0x17D320,
                   kColorBits = 0x17D324, kZBits = 0x17D328, kStencilBits = 0x17D32C, kDxt3 = 0x17D330,
                   kEarlyNvidia = 0x17D331, kClearFix = 0x17D332, kWidths = 0x17D2D0, kHeights = 0x17D2B4;
constexpr HRESULT kSurfaceBusy = HRESULT(0x887601AE);

constexpr GUID kTnLHalDevice = {0xF5049E78, 0x4861, 0x11D2, {0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8}};
constexpr GUID kHalDevice = {0x84E63DE0, 0x46AA, 0x11CF, {0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E}};
constexpr GUID kRgbDevice = {0xA4665C60, 0x2673, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56}};
constexpr GUID kDirect3D7 = {0xF5049E77, 0x4861, 0x11D2, {0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8}};
constexpr GUID kDirectDraw7 = {0x15E65EC0, 0x3B9C, 0x11D2, {0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B}};

constexpr const char* kDeviceFile = "..\\renderlib\\render_Interface_Device.cpp";

void* Render() { return Global<void*>(kRender); }

void Check(HRESULT hr, const char* what, const char* file, int line)
{
    if (hr && hr != kSurfaceBusy) dxerror::Throw(hr, what, file, line);
}

// FUN_10020bba: the DirectDraw device's identifier (`flags` 1: the host's); "succeeds" without a DirectDraw.
HRESULT Identifier(DDDEVICEIDENTIFIER2* id, DWORD flags)
{
    void* dd = Field<void*>(Render(), 8);
    return dd ? Com(dd, 0x6C, id, flags) : 0;
}

int Lower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }   // msvcr100's tolower, C locale

// FUN_100419e3 / FUN_10041b50: whether any of `patterns` is in `s`, ignoring case - as Randy looks: one pass, each
// pattern's partial match restarting at the current character only if it is the pattern's first.
bool Contains(const char* s, const char* const* patterns, int count)
{
    int state[5] = {};
    for (size_t c = 0; s[c]; ++c) {
        const int ch = Lower(s[c]);
        for (int p = 0; p < count; ++p) {
            const char* pattern = patterns[p];
            if (ch == pattern[state[p]]) {
                if (!pattern[++state[p]]) return true;
            } else {
                state[p] = ch == pattern[0] ? 1 : 0;
            }
        }
    }
    return false;
}

// FUN_10041ac0 / FUN_10041c11: the driver's or the card's name has one of `patterns` (asked of the device, then of
// the host; if neither answers, yes).
bool NamedLike(const char* const* patterns, int count)
{
    DDDEVICEIDENTIFIER2 id;
    std::memset(&id, 0, sizeof(id));
    bool answered = false;
    if (Identifier(&id, 0) == 0) {
        answered = true;
        if (Contains(id.szDriver, patterns, count) || Contains(id.szDescription, patterns, count)) return true;
    }
    if (Identifier(&id, 1) != 0) return !answered;
    return Contains(id.szDriver, patterns, count) || Contains(id.szDescription, patterns, count);
}

// FUN_10041d6e: DXT1 / DXT3 among the device's texture formats.
HRESULT __stdcall TextureFormat(const DDPIXELFORMAT* format, void*)
{
    if (format->dwFlags & DDPF_FOURCC) {
        if (format->dwFourCC == MAKEFOURCC('D', 'X', 'T', '1')) Global<uint8_t>(kDxt1) = 1;
        else if (format->dwFourCC == MAKEFOURCC('D', 'X', 'T', '3')) Global<uint8_t>(kDxt3) = 1;
    }
    return 1;                                       // D3DENUMRET_OK
}

// FUN_10041da2: (render_t has no device yet when Initialize asks, so this finds nothing - as the original.)
void FindDxt()
{
    Global<uint8_t>(kDxt1) = 0;
    Global<uint8_t>(kDxt3) = 0;
    if (void* device = Field<void*>(Render(), 0))   // render_t::EnumTextureFormats (FUN_10021495)
        Check(Com(device, 0x10, reinterpret_cast<void*>(&TextureFormat), static_cast<void*>(nullptr)),
              "render_t::EnumTextureFormats: D3D-Call failed", kDeviceFile, 0x360);
}

// FUN_10041dd2: an S3 Savage (0x5333:0x9102: the clear fix) or an early NVIDIA card (GeForce 256 to GeForce4).
void FindCardQuirks()
{
    Global<uint8_t>(kEarlyNvidia) = 0;
    Global<uint8_t>(kClearFix) = 0;
    DDDEVICEIDENTIFIER2 id;
    std::memset(&id, 0, sizeof(id));
    if (Identifier(&id, 0) != 0) return;
    const DWORD vendor = id.dwVendorId, device = id.dwDeviceId;
    if (vendor == 0x5333) {
        if (device == 0x9102) Global<uint8_t>(kClearFix) = 1;
        return;
    }
    if (vendor == 0x104A && device == 0x10) return;   // a Kyro
    if (vendor != 0x10DE) return;
    if (device - 0x100 <= 3 || device - 0x110 <= 3 || device - 0x150 <= 3 || device == 0x1A0 || device - 0x200 <= 3 ||
        device - 0x170 <= 2 || device - 0x174 <= 5)
        Global<uint8_t>(kEarlyNvidia) = 1;
}

// FUN_10024c42: a new render_t (the old one, if any, is not let go).
void NewRender()
{
    auto* r = static_cast<uint8_t*>(vc10::Allocate(0xACC));
    std::memset(r, 0, 0x10);
    Field<uint32_t>(r, 0x288) = 0;
    SetRectEmpty(&Field<RECT>(r, 0x278));
    Global<void*>(kRender) = r;
}

void* NewSurface(void* old, const char* name)       // surface_t (0x88 bytes)
{
    return orig::surface_t_surface_t(vc10::Allocate(0x88), old, const_cast<char*>(name));
}

void ErrorText(vc10::String* error, HRESULT hr, const char* before)
{
    char text[0x50];
    orig::render_t_D3DXGetErrorString(Render(), hr, sizeof(text), text);
    vc10::String joined;
    joined.init();
    const size_t a = std::strlen(before), b = std::strlen(text);
    char buffer[0x100];
    std::memcpy(buffer, before, a);
    std::memcpy(buffer + a, text, b);
    joined.assign(buffer, a + b);
    error->release();
    *error = joined;                                // moved in
}

// render_t's caps (FUN_10021073 / FUN_10021134): DirectDraw's (kept at render_t +0x10 when the driver's are asked)
// and the device's (kept at +0x18C).
void DirectDrawCaps(void* driver, void* emulation)
{
    void* dd = Field<void*>(Render(), 8);
    if (!dd) return;
    Check(Com(dd, 0x2C, driver, emulation), "render_t::GetCaps D3DObject: D3D-Call failed", kDeviceFile, 0x1EA);
    if (driver) std::memcpy(static_cast<uint8_t*>(Render()) + 0x10, driver, 0x17C);
}

void DeviceCaps(void* desc)
{
    void* device = Field<void*>(Render(), 0);
    if (!device) return;
    Check(Com(device, 0x0C, desc), "render_t::GetCaps D3DDevice: D3D-Call failed", kDeviceFile, 0x216);
    std::memcpy(static_cast<uint8_t*>(Render()) + 0x18C, desc, 0xEC);
}

constexpr uint32_t kDefaultSurface = 0x17D498, kDefaultTexture = 0x17D49C, kTextureRefs = 0x17D494;

// FUN_10047d28: RTexture_t's default texture: 16x16, 16 bits, 0xFFBB (only the first half of it filled, as the
// original writes 128 dwords).
void MakeDefaultTexture()
{
    void*& surface = Global<void*>(kDefaultSurface);
    if (surface) return;
    uint32_t flags = 0x100, width = 16, height = 16, format = 10, mips = 0;
    surface = NewSurface(surface, "DefaultTexture");
    orig::render_t_D3DXCreateTexture(Render(), &flags, &width, &height, &format, nullptr, surface, &mips);
    DDSURFACEDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    orig::render_t_LockTexture(Render(), *static_cast<void**>(surface), nullptr, &desc, 0, nullptr);
    auto* pixels = static_cast<uint32_t*>(desc.lpSurface);
    for (int i = 0; i < 0x80; ++i) pixels[i] = 0xFFBBFFBB;
    orig::render_t_UnlockTexture(Render(), *static_cast<void**>(surface), nullptr);
    Global<void*>(kDefaultTexture) = orig::RTexture_t_RTexture_t_41(vc10::Allocate(0xBC), "");
}

void ReleaseDefaultTexture()                        // FUN_10047699
{
    void*& surface = Global<void*>(kDefaultSurface);
    if (!surface) return;
    orig::surface_t_ReleaseDXSurface(surface);
    surface = nullptr;
    --Global<int32_t>(kTextureRefs);
    orig::RResource_t_ReleaseRResource(Global<void*>(kDefaultTexture));
    Global<void*>(kDefaultTexture) = nullptr;
}

// FUN_1004165f: every render target let go (and the features of 1..6).
void ReleaseTargets()
{
    for (int i = 0; i < 7; ++i) {
        Internal<void(__cdecl*)(int)>(0x41566)(i);
        void*& rt = Global<void*>(kRenderTargets + uint32_t(i) * 4);
        if (rt) {
            Internal<void(__fastcall*)(void*)>(0x25F39)(rt);   // RenderTarget_t: its surfaces
            vc10::Free(rt);
            rt = nullptr;
        }
    }
}

// FUN_100415d8: Direct3D, every render target, the device, the primary surface and DirectDraw let go.
void ReleaseDevice()
{
    Com(Global<void*>(kDirect3D), 0x08);
    Global<void*>(kDirect3D) = nullptr;
    ReleaseTargets();
    void* device = Global<void*>(kDevice);
    Com(device, 0x04);                              // AddRef, Release, Release (as the original)
    Com(device, 0x08);
    Com(device, 0x08);
    void*& primary = Global<void*>(kPrimary);
    if (primary) {
        orig::surface_t_ReleaseDXSurface(primary);
        primary = nullptr;
    }
    Com(Global<void*>(kDirectDraw), 0x08);
}

constexpr uint32_t kRandyVtable = 0x95594, kRandy = 0x17D2EC, kRestoreCount = 0x17D334, kDirect3DCopy = 0x17D2A0;
constexpr uint32_t kEmulationCap=0xB7730,kEmulationCapUser=0xB7734;
RECT g_workingRect{};
rvk::display::WindowState g_workingWindow;
bool g_workingFrame=false;
bool g_attemptD3dxStarted=false;
resizequeue::Queue g_pendingResize;
std::atomic<bool> g_layoutActive{false};
std::atomic<bool> g_resizeBlocked{false};
std::atomic<bool> g_restoreWindowPending{false};
std::atomic<uint64_t> g_requestedLayout{0},g_originalClientLayout{0},g_actualLayout{0},g_previousLayout{0};
std::atomic<void*> g_mainColor{nullptr},g_primaryColor{nullptr};
uint64_t SizePair(uint32_t width,uint32_t height){return (uint64_t(width)<<32)|height;}
void PublishActualSize(uint32_t width,uint32_t height) {
    g_previousLayout.store(g_actualLayout.exchange(SizePair(width,height),std::memory_order_acq_rel),std::memory_order_release);
}
bool RestoreWorkingWindow() {
    if(rvk::presentation::InsideWindowCallback() || !g_restoreWindowPending.exchange(false,std::memory_order_acq_rel))return false;
    HWND window=g_workingWindow.window;
    if(!g_workingFrame || !IsWindow(window))return false;
    rvk::display::dpi::WindowContext coordinates(window);
    rvk::display::WindowTrackScope track(window,{uint32_t(g_workingWindow.rect.right-g_workingWindow.rect.left),uint32_t(g_workingWindow.rect.bottom-g_workingWindow.rect.top)});
    bool restored=rvk::display::RestoreWindow(g_workingWindow);
    const uint64_t size=g_actualLayout.load(std::memory_order_acquire);
    const LONG width=LONG(size>>32),height=LONG(uint32_t(size));
    RECT client{},outer{};
    if(restored && width && height && GetClientRect(window,&client) &&
       (client.right!=width || client.bottom!=height) && GetWindowRect(window,&outer)) {
        // A DPI/non-client change can alter the frame around the restored
        // rectangle. Keep its saved style, monitor position and exact working
        // client extent rather than stretching retained framebuffer input.
        track.SetRequired({uint32_t(outer.right-outer.left+width-client.right),uint32_t(outer.bottom-outer.top+height-client.bottom)});
        restored=SetWindowPos(window,nullptr,0,0,outer.right-outer.left+width-client.right,
            outer.bottom-outer.top+height-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED)!=FALSE;
    }
    restored=restored && GetClientRect(window,&client) && client.right==width && client.bottom==height;
    Log("display rollback window: committed framebuffer %ldx%ld; restored matching client %s; saved style and monitor position retained; no automatic retry",
        width,height,restored ? "yes":"unavailable");
    return restored;
}

struct FrameState {
    std::array<void*,7> targets{};
    std::array<float,7> widths{},heights{};
    RECT rect{};
    int32_t zBits=0,stencilBits=0;
    uint32_t caps=0,userCaps=0;
};
FrameState CaptureFrame() {
    FrameState state;
    for(unsigned i=0;i<7;++i) {
        state.targets[i]=Global<void*>(kRenderTargets+i*4);
        state.widths[i]=Global<float>(kWidths+i*4);state.heights[i]=Global<float>(kHeights+i*4);
    }
    state.rect=Global<RECT>(kWindowRect);state.zBits=Global<int32_t>(kZBits);state.stencilBits=Global<int32_t>(kStencilBits);
    state.caps=Global<uint32_t>(kEmulationCap);state.userCaps=Global<uint32_t>(kEmulationCapUser);
    return state;
}
void InstallFrame(const FrameState& state) {
    for(unsigned i=0;i<7;++i) {
        Global<void*>(kRenderTargets+i*4)=state.targets[i];
        Global<float>(kWidths+i*4)=state.widths[i];Global<float>(kHeights+i*4)=state.heights[i];
    }
    Global<RECT>(kWindowRect)=state.rect;Global<int32_t>(kZBits)=state.zBits;Global<int32_t>(kStencilBits)=state.stencilBits;
    Global<uint32_t>(kEmulationCap)=state.caps;Global<uint32_t>(kEmulationCapUser)=state.userCaps;
}
void DestroyTargetSet(const std::array<void*,7>& targets) {
    for(unsigned i=0;i<7;++i)if(targets[i]) {
        bool duplicate=false;for(unsigned j=0;j<i;++j)duplicate|=targets[j]==targets[i];
        if(!duplicate){Internal<void(__fastcall*)(void*)>(0x25F39)(targets[i]);vc10::Free(targets[i]);}
    }
}
void TargetSize(void* target,uint32_t& width,uint32_t& height) {
    width=height=0;
    if(!target || !*static_cast<void**>(target) || !**static_cast<void***>(target))return;
    const auto* desc=static_cast<const DDSURFACEDESC2*>(orig::surface_t_GetSurfaceDesc(*static_cast<void**>(target)));
    if(desc){width=desc->dwWidth;height=desc->dwHeight;}
}
bool ReplaceFrameBuffer(vc10::String* error,bool bind,const RECT* previousRect=nullptr);

}  // namespace

bool __cdecl MakeFrameBuffer(vc10::String* error);

// FUN_10043365: Randy_t::Randy_t (0x298 bytes): +0x004 fog end, +0x008 W-buffer, +0x00C DDCAPS, +0x188
// D3DDEVICEDESC7, +0x27C its DeviceState, +0x288 features in use (7), +0x28C std::list<int> of pushed render targets.
void* __fastcall Construct(uint8_t* randy)
{
    Field<uint32_t>(randy, 0) = uint32_t(reinterpret_cast<uintptr_t>(g_orig)) + kRandyVtable;
    Field<uint32_t>(randy, 0x274) = 0;
    Field<uint32_t>(randy, 0x278) = 0;
    Field<uint32_t>(randy, 0x280) = 0;
    Field<uint32_t>(randy, 0x284) = 0;
    Field<uint32_t>(randy, 0x288) = 7;
    Field<uint32_t>(randy, 0x290) = 0;              // the list: an empty head node (FUN_10044455)
    auto** head = static_cast<void**>(vc10::Allocate(0xC));
    head[0] = head[1] = head;
    Field<void*>(randy, 0x28C) = head;
    // These are per-target size caches, not the AO viewport size. Preserve the
    // actual populated target0/5/6 dimensions; untouched features retain their
    // historical256 default until their own lazy CreateRenderTarget sets them.
    for(uint32_t i=0;i<7;++i) {
        uint32_t width=0,height=0;TargetSize(Global<void*>(kRenderTargets+i*4),width,height);
        Global<float>(kWidths+i*4)=width ? float(width):256.f;
        Global<float>(kHeights+i*4)=height ? float(height):256.f;
    }
    orig::Debugger_t_Get();
    Global<uint32_t>(kRestoreCount) = 0;
    Global<void*>(kDirect3DCopy) = Global<void*>(kDirect3D);
    Field<float>(randy, 4) = 0.0f;
    Field<void*>(Render(), 0) = Global<void*>(kDevice);
    void* caps = randy + 0xC;
    std::memset(caps, 0, 0x17C);
    Field<uint32_t>(caps, 0) = 0x17C;
    if (Global<int32_t>(kHardwareLevel) == 0) DirectDrawCaps(nullptr, caps);   // software: the emulation's
    else DirectDrawCaps(caps, nullptr);
    std::memset(randy + 0x188, 0, 0xEC);
    DeviceCaps(randy + 0x188);
    randy[8] = (Field<uint32_t>(randy, 0x1CC) & 0x40000) && (Field<uint32_t>(randy, 0x194) & 0x40000);   // W-buffer
    Field<void*>(randy, 0x27C) = Internal<void*(__cdecl*)()>(0x1BDF2)();   // the DeviceState
    Internal<void(__fastcall*)(void*)>(0x41EDE)(randy);                     // the device's default states
    Global<void*>(kRandy) = randy;
    MakeDefaultTexture();
    g_mainColor.store(*static_cast<void**>(Global<void*>(kRenderTargets)),std::memory_order_release);
    g_primaryColor.store(Global<void*>(kPrimary),std::memory_order_release);
    g_pendingResize.Register(randy);
    return randy;
}

// FUN_10043484: Randy_t::~Randy_t - everything Initialize made, in turn.
void __fastcall Destruct(uint8_t* randy)
{
    g_pendingResize.Cancel();g_layoutActive.store(false,std::memory_order_release);g_resizeBlocked.store(false,std::memory_order_release);g_restoreWindowPending.store(false,std::memory_order_release);
    g_mainColor.store(nullptr,std::memory_order_release);g_primaryColor.store(nullptr,std::memory_order_release);
    Field<uint32_t>(randy, 0) = uint32_t(reinterpret_cast<uintptr_t>(g_orig)) + kRandyVtable;
    Global<void*>(kRandy) = nullptr;
    orig::DynamicVB_c_Get();
    orig::DynamicVB_c_Shutdown();
    orig::HMOccluder_t_ShutdownHMOccluder();
    ReleaseDefaultTexture();
    Internal<void(__cdecl*)()>(0x1BD14)();         // the DeviceState
    Field<void*>(randy, 0x27C) = nullptr;
    ReleaseDevice();
    Check(Internal<HRESULT(__cdecl*)()>(0x6378A)(), "render_t::D3DXUninitialize: D3D-Call failed", kDeviceFile, 0x40A);
    Internal<void(__cdecl*)()>(0x2CC89)();         // Debugger_t deleted
    vc10::Free(Global<void*>(kRender));            // render_t (FUN_10024c62)
    Global<void*>(kRender) = nullptr;
    auto** head = Field<void**>(randy, 0x28C);     // the list's nodes (FUN_1002fa4c), then its head
    void** node = static_cast<void**>(head[0]);
    head[0] = head[1] = head;
    Field<uint32_t>(randy, 0x290) = 0;
    while (node != head) {
        void** next = static_cast<void**>(node[0]);
        vc10::Free(node);
        node = next;
    }
    vc10::Free(head);
    g_workingFrame=false;g_workingWindow={};g_attemptD3dxStarted=false;
}

// FUN_10043c48 (RViewPort_t::Resize): a frame buffer of a new size - every render target made again, the device
// drawing to the new back buffer.
void __fastcall ResizeFrameBuffer(void* randy, void*, int32_t width, int32_t height)
{
    if(!ResizeDisplayEligible())return;
    if(width<0 || height<0 || !rvk::display::Dimensions(uint32_t(width),uint32_t(height)))return;
    if(rvk::presentation::InsideWindowCallback()) {
        g_pendingResize.Enqueue(randy,uint32_t(width),uint32_t(height));return;
    }
    if(randy!=Global<void*>(kRandy))return;
    RECT& rect = Global<RECT>(kWindowRect);
    uint32_t oldWidth=0,oldHeight=0;TargetSize(Global<void*>(kRenderTargets),oldWidth,oldHeight);
    if(uint32_t(width)==oldWidth && uint32_t(height)==oldHeight)return;
    const RECT previous=g_workingFrame ? g_workingRect:rect;
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    vc10::String error;
    error.init();
    if(!ReplaceFrameBuffer(&error,true,&previous)) {
        g_resizeBlocked.store(true,std::memory_order_release);
        g_restoreWindowPending.store(true,std::memory_order_release);
        RestoreWorkingWindow();
        rvk::display::DisableOverride(rnative::IniPath(),"framebuffer resize failed");
        Log("display resize rejected: requested %dx%d; retained framebuffer %ux%u and prior window; %s",width,height,oldWidth,oldHeight,error.c_str());
    } else {
        Field<int32_t>(randy,0x280)=Field<int32_t>(randy,0x284)=0;
        Log("display resize committed: native framebuffer %dx%d; old target set released only after successful bind",width,height);
    }
    error.release();
}

// FUN_10044757: Randy_t's deleting destructor (vtable slot 0).
void* __fastcall DeletingDestruct(uint8_t* randy, void*, uint32_t flags)
{
    Destruct(randy);
    if (flags & 1) vc10::Free(randy);
    return randy;
}

// FUN_100435e3: the frame buffer (windowed: an offscreen render target 0 the window's size, the primary clipped to
// the window) and its Z-buffer; then render targets 5 and 6. Also after a resize (FUN_10043c48).
static bool BuildFrameBuffer(vc10::String* error)
{
    const RECT& window = Global<RECT>(kWindowRect);
    const int32_t width = window.right - window.left, height = window.bottom - window.top;
    if(width<0 || height<0 || !rvk::display::Dimensions(uint32_t(width),uint32_t(height))) {
        error->assign("Invalid size",12);return false;
    }
    const uint32_t memory = (Global<int32_t>(kHardwareLevel) ? 0x3800u : 0u) + 0x800u;
    if (!Global<uint8_t>(kFullscreen)) {
        void* rt = vc10::Allocate(0xC);
        std::memset(rt,0,0xC);Global<void*>(kRenderTargets)=rt;
        Internal<void*(__fastcall*)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t)>(0x25E84)(
            rt, nullptr, 7, memory | 0x2040, uint32_t(width), uint32_t(height));   // RenderTarget_t
        if (HWND window = Global<HWND>(kWindow)) {
            void* clipper = nullptr;
            // The render_t wrapper returns void. Call the underlying DDRAW
            // HRESULT API so an allocation failure participates in rollback.
            void* dd=Field<void*>(Render(),8);
            HRESULT hr=dd ? Com(dd,0x10,DWORD(0),&clipper,static_cast<void*>(nullptr)):E_INVALIDARG;
            if(hr || !clipper){if(clipper)Com(clipper,0x08);ErrorText(error,hr ? hr:E_OUTOFMEMORY,"Failed to create clipper: ");return false;}
            hr=Com(clipper, 0x20, DWORD(0), window);   // SetHWnd
            void* primary = *static_cast<void**>(Global<void*>(kPrimary));
            if(!hr)hr=primary ? Com(primary, 0x70, clipper):E_INVALIDARG;
            Com(clipper, 0x08);                     // Release
            if(hr){ErrorText(error,hr,"Failed to attach clipper: ");return false;}
        }
    } else {
        // Preserve the legacy fullscreen chain, but acquire a fresh wrapper so
        // failure cannot destroy the previous working target set.
        auto* rt=static_cast<void**>(vc10::Allocate(0xC));std::memset(rt,0,0xC);
        Global<void*>(kRenderTargets)=rt;rt[0]=NewSurface(nullptr,"BackBuffer");
        DDSCAPS2 caps{DDSCAPS_BACKBUFFER,0,0,{0}};
        void* primary=Global<void*>(kPrimary);
        HRESULT hr=primary ? Com(*static_cast<void**>(primary),0x30,&caps,orig::surface_t_GetSurfacePointer(rt[0])):E_INVALIDARG;
        if(hr){ErrorText(error,hr,"Failed to acquire back buffer: ");return false;}
    }
    uint32_t actualWidth=0,actualHeight=0;TargetSize(Global<void*>(kRenderTargets),actualWidth,actualHeight);
    if(actualWidth!=uint32_t(width) || actualHeight!=uint32_t(height)) {
        error->assign("Size mismatch",13);return false;
    }
    const HRESULT hr = Internal<HRESULT(__fastcall*)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                                     int32_t*, int32_t*, int32_t)>(0x25F5C)(   // its Z-buffer
        Global<void*>(kRenderTargets), nullptr, 0x1007, memory | 0x20000, 0x400, uint32_t(width), uint32_t(height),
        &Global<int32_t>(kZBits), &Global<int32_t>(kStencilBits), Global<int32_t>(kColorBits));
    if (hr) {
        ErrorText(error, hr, "Failed during retry to create the z-buffer: ");
        return false;
    }
    void* color=*static_cast<void**>(Global<void*>(kRenderTargets));
    DDSCAPS2 zCaps{DDSCAPS_ZBUFFER,0,0,{0}};void* attached=nullptr;
    HRESULT attachedResult=Com(*static_cast<void**>(color),0x30,&zCaps,&attached);
    if(attachedResult || !attached) {
        ErrorText(error,attachedResult ? attachedResult:E_FAIL,"Replacement depth is not attached: ");return false;
    }
    Com(attached,0x08);
    Global<float>(kWidths) = float(width);
    Global<float>(kHeights) = float(height);
    Internal<void(__cdecl*)(int)>(0x42732)(5);      // Randy_t: render targets 5 and 6
    Internal<void(__cdecl*)(int)>(0x42732)(6);
    return true;
}

namespace {
bool ReplaceFrameBuffer(vc10::String* error,bool bind,const RECT* previousRect) {
    FrameState previous=CaptureFrame();if(previousRect)previous.rect=*previousRect;
    const RECT requested=Global<RECT>(kWindowRect);
    // A working legacy fullscreen chain owns its attached depth surface. Keep
    // that chain intact rather than attempting to mutate it transactionally.
    if(bind && Global<uint8_t>(kFullscreen)) {
        InstallFrame(previous);error->assign("Legacy fullscr",14);return false;
    }
    bool ok=framebuffer::Replace(
        [&]{for(unsigned i=0;i<7;++i)Global<void*>(kRenderTargets+i*4)=nullptr;Global<uint32_t>(kEmulationCap)=0;},
        [&]{
            if(!BuildFrameBuffer(error))return false;
            for(unsigned i:{5u,6u})if(previous.targets[i] && !Global<void*>(kRenderTargets+i*4))return false;
            return true;
        },
        [&]{
            if(!bind || !Field<void*>(Render(),0))return true;
            void* target=Global<void*>(kRenderTargets);void* color=target ? *static_cast<void**>(target):nullptr;
            HRESULT hr=color ? Com(Field<void*>(Render(),0),0x20,*static_cast<void**>(color),DWORD(0)):E_INVALIDARG;
            if(hr){ErrorText(error,hr,"Failed to bind replacement: ");return false;}return true;
        },
        [&]{
            auto candidates=CaptureFrame().targets;InstallFrame(previous);DestroyTargetSet(candidates);
            if(bind && previous.targets[0] && Field<void*>(Render(),0)) {
                void* color=*static_cast<void**>(previous.targets[0]);
                if(color)Com(Field<void*>(Render(),0),0x20,*static_cast<void**>(color),DWORD(0));
            }
        },
        [&]{
            // Viewports hold raw native surface_t pointers, including after
            // returning from offscreen rendering. Preserve target0's wrapper
            // before releasing the previous set. Each wrapper owns one COM
            // lease regardless of its native user count (surface.cpp).
            if(bind && previous.targets[0] && Global<void*>(kRenderTargets)) {
                auto& oldColor=*static_cast<void**>(previous.targets[0]);
                auto& newColor=*static_cast<void**>(Global<void*>(kRenderTargets));
                framebuffer::PreserveMainSurface(oldColor,newColor,[](void* retained,void* candidate){
                    std::swap(Field<void*>(retained,0),Field<void*>(candidate,0));
                    static_cast<uint8_t*>(retained)[0x84]=1;
                    static_cast<uint8_t*>(candidate)[0x84]=1;
                });
            }
            g_mainColor.store(*static_cast<void**>(Global<void*>(kRenderTargets)),std::memory_order_release);
            PublishActualSize(uint32_t(requested.right-requested.left),uint32_t(requested.bottom-requested.top));
            DestroyTargetSet(previous.targets);g_workingRect=requested;g_workingFrame=true;
            g_workingWindow=rvk::display::CaptureWindow(Global<HWND>(kWindow));});
    if(!ok && error->size==0)error->assign("Build failed",12);
    return ok;
}
} // namespace

bool __cdecl MakeFrameBuffer(vc10::String* error) {return ReplaceFrameBuffer(error,false);}

// ?Initialize@Randy_t@@SAPAV1@GGAAV?$basic_string...@PAU_GUID@@1_NPAUHWND__@@W4BufferFormat_e@1@@Z: DisplaySystem's
// way in. `format` (Randy_t::BufferFormat_e): 0 Z24 S8, 1 Z32 (Z24 S8 on a 3dfx), 2 and 3 Z16 (and 16-bit colour in
// full screen); 16-bit colour always Z16. Null (with `error` set) if the frame buffer can't be made.
static bool InitializeGraphics(uint16_t width, uint16_t height, vc10::String* error, GUID* adapter, GUID* device,
                               bool windowed, HWND window, int32_t format)
{
    Global<void*>(kDirectDraw)=Global<void*>(kDirect3D)=Global<void*>(kDevice)=Global<void*>(kPrimary)=nullptr;
    for(unsigned i=0;i<7;++i)Global<void*>(kRenderTargets+i*4)=nullptr;
    Global<uint32_t>(kEmulationCap)=0;
    g_attemptD3dxStarted=false;g_workingFrame=false;
    NewRender();
    void* render = Render();
    const bool fullscreen = !windowed;
    Global<uint8_t>(kFullscreen) = fullscreen;
    Global<HWND>(kWindow) = window;
    int32_t& level = Global<int32_t>(kHardwareLevel);
    level = !device || *device == kTnLHalDevice ? 2 : *device == kHalDevice ? 1 : 0;
    const int32_t best = devices::BestHardwareLevel();
    if (level > best) level = best;

    RECT& rect = Global<RECT>(kWindowRect);
    if (fullscreen || !window) {
        rect = {0, 0, width, height};
    } else {
        GetClientRect(window, &rect);
        if (rect.right == 0 && rect.bottom == 0) {
            rect.right = width;
            rect.bottom = height;
        }
        ClientToScreen(window, reinterpret_cast<POINT*>(&rect.left));
        ClientToScreen(window, reinterpret_cast<POINT*>(&rect.right));
    }

    HRESULT initial=Internal<HRESULT(__cdecl*)()>(0x63427)();
    if(initial){ErrorText(error,initial,"D3DX initialization failed: ");return false;}
    g_attemptD3dxStarted=true;
    using CreateFn = HRESULT(WINAPI*)(GUID*, void**, REFIID, IUnknown*);
    Check(Global<CreateFn>(0x8A004)(adapter, &Global<void*>(kDirectDraw), kDirectDraw7, nullptr),
          "render_t::DirectDrawCreateEx: D3D-Call failed", kDeviceFile, 0xF2);
    void* dd = Global<void*>(kDirectDraw);
    if(!dd){error->assign("No DirectDraw",13);return false;}
    Field<void*>(render, 8) = dd;
    if (dd)
        Check(Com(dd, 0x50, fullscreen ? window : HWND(nullptr), DWORD(fullscreen ? 0x11 : 0x1008)),   // exclusive
              "render_t::SetCooperativeLevel: D3D-Call failed", kDeviceFile, 0x159);           // / normal, FPU kept
    if (fullscreen && dd)
        Check(Com(dd, 0x54, DWORD(rect.right), DWORD(rect.bottom), DWORD(format > 2 ? 16 : 32), DWORD(0), DWORD(0)),
              "render_t::SetDisplayMode: D3D-Call failed", kDeviceFile, 400);
    static const char* const k3dfxNames[] = {"voodoo", "3dfx", "vodoo", "voodo", "vodo"};
    static const char* const kKyroNames[] = {"kyro"};
    Global<uint8_t>(k3dfx) = NamedLike(k3dfxNames, 5);
    Global<uint8_t>(kKyro) = NamedLike(kKyroNames, 1);

    // The primary surface: in full screen with one back buffer to flip.
    DDSURFACEDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    if (fullscreen) {
        desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE;
        desc.dwBackBufferCount = 1;
    } else {
        desc.dwFlags = DDSD_CAPS;
        desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    }
    void*& primary = Global<void*>(kPrimary);
    primary = NewSurface(primary, "FrameBuffer");
    if (HRESULT hr = orig::render_t_CreateSurface(render, &desc, orig::surface_t_GetSurfacePointer(primary), nullptr)) {
        char text[0x50];
        orig::render_t_D3DXGetErrorString(render, hr, sizeof(text), text);
        error->assign(text, std::strlen(text));
        return false;
    }
    DDPIXELFORMAT pf;
    std::memset(&pf, 0, sizeof(pf));
    pf.dwSize = sizeof(pf);
    orig::render_t_GetPixelFormat(render, *static_cast<void**>(primary), &pf);
    const int32_t colorBits = Global<int32_t>(kColorBits) = int32_t(pf.dwRGBBitCount);
    switch (colorBits == 16 ? 3 : format) {
    case 1:
        if (!Global<uint8_t>(k3dfx)) {
            Global<int32_t>(kZBits) = 32;
            Global<int32_t>(kStencilBits) = 0;
            break;
        }
        [[fallthrough]];
    case 0:
        Global<int32_t>(kZBits) = 24;
        Global<int32_t>(kStencilBits) = 8;
        break;
    case 2:
    case 3:
        Global<int32_t>(kZBits) = 16;
        Global<int32_t>(kStencilBits) = 0;
        break;
    default:
        break;
    }
    if (!window) {
        orig::surface_t_ReleaseDXSurface(primary);
        primary = nullptr;
    }
    if (fullscreen) {                               // render target 0: the primary's back buffer
        DDSCAPS2 caps = {DDSCAPS_BACKBUFFER, 0, 0, {0}};
        auto* rt = static_cast<void**>(vc10::Allocate(0xC));   // RenderTarget_t (FUN_10025e32)
        rt[1] = nullptr;
        reinterpret_cast<uint8_t*>(rt)[8] = 0;
        rt[0] = NewSurface(nullptr, "BackBuffer");
        Global<void*>(kRenderTargets) = rt;
        void* out = rt[0] ? orig::surface_t_GetSurfacePointer(rt[0]) : nullptr;
        Check(Com(*static_cast<void**>(primary), 0x30, &caps, out), "render_t::GetAttachedSurface: D3D-Call failed",
              "..\\renderlib\\render_Interface_Surface.cpp", 0x80);
    }
    if (!MakeFrameBuffer(error)) return false;

    if (dd) {
        HRESULT hr=Com(dd,0x00,&kDirect3D7,&Global<void*>(kDirect3D));
        if(hr){ErrorText(error,hr,"Direct3D query failed: ");return false;}
    }
    void* d3d = Field<void*>(render, 4) = Global<void*>(kDirect3D);
    if(!d3d){error->assign("No Direct3D",11);return false;}
    GUID chosen = device ? *device : level == 1 ? kHalDevice : level == 2 ? kTnLHalDevice : kRgbDevice;
    void** target = static_cast<void**>(Global<void*>(kRenderTargets));
    void* surface = target[0] ? *static_cast<void**>(target[0]) : nullptr;
    if (d3d) {
        HRESULT hr=Com(d3d,0x10,&chosen,surface,&Global<void*>(kDevice));
        if(hr){ErrorText(error,hr,"Direct3D device creation failed: ");return false;}
    }
    FindDxt();
    FindCardQuirks();
    return true;
}

static void CleanupGraphicsAttempt() {
    if(void* device=Global<void*>(kDevice))Com(device,0x08);
    Global<void*>(kDevice)=nullptr;
    DestroyTargetSet(CaptureFrame().targets);
    for(unsigned i=0;i<7;++i)Global<void*>(kRenderTargets+i*4)=nullptr;
    if(void* primary=Global<void*>(kPrimary))orig::surface_t_ReleaseDXSurface(primary);
    Global<void*>(kPrimary)=nullptr;
    if(void* d3d=Global<void*>(kDirect3D))Com(d3d,0x08);
    Global<void*>(kDirect3D)=nullptr;
    if(void* dd=Global<void*>(kDirectDraw)) {Com(dd,0x4C);Com(dd,0x08);} // restore mode/window, Release
    Global<void*>(kDirectDraw)=nullptr;
    if(g_attemptD3dxStarted)Internal<HRESULT(__cdecl*)()>(0x6378A)();
    g_attemptD3dxStarted=false;
    if(void* render=Render())vc10::Free(render);
    Global<void*>(kRender)=nullptr;g_workingFrame=false;
}

void* __cdecl Initialize(uint16_t width,uint16_t height,vc10::String* error,GUID* adapter,GUID* device,
                         bool windowed,HWND window,int32_t format)
{
    if(rvk::presentation::InsideWindowCallback()) {error->assign("Display busy",12);return nullptr;}
    if(Global<void*>(kRandy)) {error->assign("Device active",13);return nullptr;}
    g_pendingResize.Cancel();g_layoutActive.store(false,std::memory_order_release);g_resizeBlocked.store(false,std::memory_order_release);g_restoreWindowPending.store(false,std::memory_order_release);
    const auto originalWindow=rvk::display::CaptureWindow(window);
    RECT originalClient{};if(IsWindow(window))GetClientRect(window,&originalClient);
    g_requestedLayout.store(SizePair(width,height),std::memory_order_release);
    g_originalClientLayout.store(SizePair(uint32_t(originalClient.right),uint32_t(originalClient.bottom)),std::memory_order_release);
    const uint32_t requestedUserCaps=Global<uint32_t>(kEmulationCapUser);
    const auto& originalOptions=rvk::display::StartupOptions(rnative::IniPath());
    bool overrideRequested=originalOptions.mode!=rvk::display::WindowMode::Game || originalOptions.width ||
        !originalOptions.monitor.empty() || !originalOptions.valid;
    for(unsigned attempt=0;attempt<2;++attempt) {
        const auto& options=rvk::display::StartupOptions(rnative::IniPath());
        const auto monitor=rvk::display::Discover(window,options);
        auto selected=rvk::display::SelectSize(options,width,height,monitor);
        if(!options.valid || !monitor.preferenceFound || selected.rejected ||
           (overrideRequested && attempt==0 && !rvk::display::ApplyWindowOptions(window,options,monitor))) {
            const char* reason=!options.valid ? options.error.c_str():!monitor.preferenceFound ? "selected monitor is not connected":
                selected.rejected ? selected.reason:rvk::display::WindowOptionsFailureReason();
            if(!reason)reason="window mode application failed";
            Log("startup Display override rejected before allocation: %s; using original game mode; native DPI override not active; saved configuration unchanged",reason);
            rvk::display::DisableOverride(rnative::IniPath(),reason);
            rvk::display::RestoreWindow(originalWindow);
            selected={width,height};
        }
        const auto& active=rvk::display::StartupOptions(rnative::IniPath());
        bool effectiveWindowed=active.mode==rvk::display::WindowMode::Game ? windowed:true;
        bool ready=false;
        try {ready=InitializeGraphics(uint16_t(selected.width),uint16_t(selected.height),error,adapter,device,
                                      effectiveWindowed,window,format);} catch(...) {
            error->release();error->init();error->assign("Display failed",14);
        }
        if(ready) {
            uint32_t actualWidth=0,actualHeight=0;TargetSize(Global<void*>(kRenderTargets),actualWidth,actualHeight);
            const auto& chosen=rvk::display::StartupOptions(rnative::IniPath());
            g_actualLayout.store(SizePair(actualWidth,actualHeight),std::memory_order_release);
            g_previousLayout.store(SizePair(actualWidth,actualHeight),std::memory_order_release);
            g_layoutActive.store(chosen.valid && rvk::display::HasOverride(chosen),std::memory_order_release);
            Log("display initialized: AO requested %ux%u; client %ldx%ld; actual framebuffer %ux%u; mode %s; GetBufferSize uses actual surface description; viewport coverage must match target",
                unsigned(width),unsigned(height),Global<RECT>(kWindowRect).right-Global<RECT>(kWindowRect).left,
                Global<RECT>(kWindowRect).bottom-Global<RECT>(kWindowRect).top,actualWidth,actualHeight,
                effectiveWindowed ? "windowed/borderless":"Game fullscreen chain");
            return Construct(static_cast<uint8_t*>(vc10::Allocate(0x298)));
        }
        Log("display initialization failed: candidate %ux%u; %s",unsigned(selected.width),unsigned(selected.height),error->c_str());
        CleanupGraphicsAttempt();Global<uint32_t>(kEmulationCapUser)=requestedUserCaps;
        rvk::display::RestoreWindow(originalWindow);
        if(!overrideRequested || attempt!=0)return nullptr;
        rvk::display::DisableOverride(rnative::IniPath(),"custom framebuffer allocation failed; original game retry");
        error->release();error->init();
    }
    return nullptr;
}

displaylayout::Layout StartupLayout() {
    displaylayout::Layout layout;layout.active=g_layoutActive.load(std::memory_order_acquire);
    if(!layout.active)return layout;
    auto read=[](uint64_t pair,uint32_t& w,uint32_t& h){w=uint32_t(pair>>32);h=uint32_t(pair);};
    read(g_requestedLayout.load(std::memory_order_acquire),layout.requestedWidth,layout.requestedHeight);
    read(g_originalClientLayout.load(std::memory_order_acquire),layout.originalClientWidth,layout.originalClientHeight);
    read(g_actualLayout.load(std::memory_order_acquire),layout.actualWidth,layout.actualHeight);
    read(g_previousLayout.load(std::memory_order_acquire),layout.previousWidth,layout.previousHeight);
    RECT client{};HWND window=g_orig ? Global<HWND>(kWindow):nullptr;
    rvk::display::dpi::WindowContext coordinates(window);
    if(IsWindow(window) && GetClientRect(window,&client)) {layout.clientWidth=uint32_t(client.right);layout.clientHeight=uint32_t(client.bottom);}
    return layout;
}
bool IsMainSurface(void* surface) {
    return surface && g_layoutActive.load(std::memory_order_acquire) &&
        (surface==g_mainColor.load(std::memory_order_acquire) || surface==g_primaryColor.load(std::memory_order_acquire));
}
bool ResizeDisplayEligible() {
    // The DPI opt-in gate belongs only to installations which requested an
    // override. Default Game retains its existing resize/DPI behavior even
    // after a recoverable allocation failure.
    if(!rvk::display::HasOverride(rvk::display::RequestedOptions(rnative::IniPath())))return true;
    if(g_resizeBlocked.load(std::memory_order_acquire))return false;
    HWND window=g_orig ? Global<HWND>(kWindow):nullptr;
    // A minimized/zero client is transient. Retain its last working targets and
    // pending request without turning a valid startup choice into a rejection.
    if(!IsWindow(window) || IsIconic(window))return false;
    const auto dpi=rvk::display::dpi::Inspect(window);
    if(!dpi.observed.logicalWidth || !dpi.observed.logicalHeight)return false;
    if(dpi.decision.nativeOverride)return true;
    g_resizeBlocked.store(true,std::memory_order_release);
    g_restoreWindowPending.store(true,std::memory_order_release);
    rvk::display::DisableOverride(rnative::IniPath(),dpi.decision.reason);
    Log("display native resize retained working targets: %s; saved configuration unchanged",dpi.decision.reason);
    return false;
}
bool DeferResize(void* randy,uint32_t width,uint32_t height) {
    return ResizeDisplayEligible() && g_pendingResize.Enqueue(randy,width,height);
}
bool OwnsRandy(void* randy) {return g_pendingResize.Owns(randy);}
void DrainPendingResize() {
    if(!g_orig || rvk::presentation::InsideWindowCallback())return;
    HWND window=Global<HWND>(kWindow);
    if(window && (!IsWindow(window) || IsIconic(window)))return;
    RestoreWorkingWindow();
    if(!ResizeDisplayEligible()){RestoreWorkingWindow();return;}
    void* current=Global<void*>(kRandy);
    auto request=g_pendingResize.Take(current);
    if(request.owner && request.owner==Global<void*>(kRandy))ResizeFrameBuffer(request.owner,nullptr,int32_t(request.width),int32_t(request.height));
}

HWND DisplayWindow(){return g_orig ? Global<HWND>(kWindow):nullptr;}

void Install(HMODULE orig)
{
    g_orig = orig;
    struct Entry {
        uint32_t rva;
        void* target;
        const char* what;
    };
#define FN(f) reinterpret_cast<void*>(&f)
    const Entry entries[] = {
        {0x43F20, FN(Initialize), "Randy_t::Initialize"},
        {0x435E3, FN(MakeFrameBuffer), "Randy_t: frame buffer (FUN_100435e3)"},
        {0x41D6E, FN(TextureFormat), "Randy_t: DXT texture formats (FUN_10041d6e)"},
        {0x43365, FN(Construct), "Randy_t::Randy_t (FUN_10043365)"},
        {0x43484, FN(Destruct), "Randy_t::~Randy_t (FUN_10043484)"},
        {0x44757, FN(DeletingDestruct), "Randy_t deleting destructor (FUN_10044757)"},
        {0x43C48, FN(ResizeFrameBuffer), "Randy_t: frame buffer resized (FUN_10043c48)"},
    };
#undef FN
    int installed = 0;
    for (const Entry& e : entries) installed += Replace(orig, e.rva, e.target, e.what) ? 1 : 0;
    Log("Randy_t::Initialize: %d of %d functions native", installed, int(sizeof(entries) / sizeof(entries[0])));
}

}  // namespace rnative::randyinit
