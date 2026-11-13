#include "rvk_backend.h"
#include "rvk_settings.h"
#include "rvk_zoning.h"
#include "native/native.h"
#include "native/client_build.h"
#include "zoning_policy.h"
#include "zoning_phrases.h"
#include "stb/stb_image.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <string>
#include <random>
#include <vector>

namespace rvkproxy {
namespace {
using TeleportFn = void(__fastcall*)(void*, void*, bool);
TeleportFn original = nullptr;
using GetEngineFn = void*(__cdecl*)();
using GetStatusFn = bool(__fastcall*)(void*, void*);
GetEngineFn getEngine = nullptr;
GetStatusFn getStatus = nullptr;
std::atomic<HWND> owner{nullptr};
std::atomic<bool> enabled{true}, stopping{false}, loading{false};
std::atomic<bool> initialPicture{true};
std::atomic<uint64_t> generation{0}, resetSerial{0}, worlds{0}, loadedWorlds{0}, loadedGeneration{0};
HANDLE thread = nullptr, painted = nullptr;
std::string directory;
bool attempted = false;
struct Picture { int width = 0, height = 0; std::string path; std::vector<uint32_t> pixels; };
std::vector<Picture> pictures;
size_t selected = 0;
size_t selectedPhrase = rvk::zoning::kLoadingPhraseCount;

// Phrase and artwork are painted on this same window, so both share the fade.
// Keep the complete image; outline the phrase above its baked-in AO wordmark.
int PaintPhrase(HDC dc, const RECT& client, bool paint) {
    if (selectedPhrase >= rvk::zoning::kLoadingPhraseCount || client.right < 40 || client.bottom < 40) return 0;
    const int width = client.right-client.left, height = client.bottom-client.top;
    const int inset = std::max(12, width / 10);
    const int fontSize = std::clamp(width / 64, 14, 28);
    HFONT font = CreateFontW(-fontSize, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    RECT text{client.left+inset, 0, client.right-inset, 0};
    constexpr UINT flags = DT_CENTER | DT_WORDBREAK | DT_NOPREFIX;
    DrawTextW(dc, rvk::zoning::kLoadingPhrases[selectedPhrase], -1, &text, flags | DT_CALCRECT);
    const int textHeight = text.bottom;
    if (paint) {
        const int bottom = client.top+int(height*.83f);
        text = {client.left+inset, bottom-textHeight, client.right-inset, bottom};
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(8, 13, 18));
        const int outline = fontSize >= 24 ? 2 : 1;
        for (int y=-outline; y<=outline; ++y) for (int x=-outline; x<=outline; ++x) {
            if (!x && !y) continue;
            RECT edge=text; OffsetRect(&edge,x,y);
            DrawTextW(dc, rvk::zoning::kLoadingPhrases[selectedPhrase], -1, &edge, flags);
        }
        SetTextColor(dc, RGB(242, 242, 236));
        DrawTextW(dc, rvk::zoning::kLoadingPhrases[selectedPhrase], -1, &text, flags);
    }
    SelectObject(dc, old); DeleteObject(font);
    return textHeight;
}

void BeginLoading() {
    if (!loading.exchange(true)) {
        ++resetSerial;
        if (!stopping && enabled.load()) {
            if (painted) ResetEvent(painted);
            ++generation;
            // Give the independent window a bounded chance to paint before AO blocks.
            if (painted) WaitForSingleObject(painted, 50);
        }
    }
}

void EndLoading() {
    if (loading.load()) {
        worlds = 0;
        loadedWorlds = 0;
        loadedGeneration = generation.load();
        ++resetSerial;
        loading = false;
    }
}

void __fastcall Teleport(void* self, void* edx, bool value) {
    if (value) BeginLoading();
    original(self, edx, value);
    if (!value) EndLoading();
}

void LoadPictures() {
    WIN32_FIND_DATAA data{};
    HANDLE find = FindFirstFileA((directory + "*.png").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return;
    std::vector<std::string> paths;
    do { if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) paths.push_back(directory + data.cFileName); }
    while (FindNextFileA(find, &data));
    FindClose(find);
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        int w = 0, h = 0, channels = 0;
        if (!stbi_info(path.c_str(), &w, &h, &channels) || w < 1 || h < 1 || w > 4096 || h > 4096) continue;
        if (size_t(w)*h > 8u*1024*1024) continue;
        Picture image; image.width = w; image.height = h; image.path = path;
        pictures.push_back(std::move(image));
        if (pictures.size() == 32) break;
    }
}

void SelectPicture(size_t index) {
    if (pictures.empty()) return;
    selected = index % pictures.size();
    if (!pictures[selected].pixels.empty()) return;
    for (auto& picture : pictures) std::vector<uint32_t>().swap(picture.pixels);
    Picture& image = pictures[selected];
    int w = 0, h = 0, channels = 0;
    unsigned char* rgba = stbi_load(image.path.c_str(), &w, &h, &channels, 4);
    if (!rgba) return;
    if (w != image.width || h != image.height) { stbi_image_free(rgba); return; }
    image.pixels.resize(size_t(w)*h);
    for (size_t i = 0; i < image.pixels.size(); ++i)
        image.pixels[i] = uint32_t(rgba[i*4+2]) | (uint32_t(rgba[i*4+1]) << 8) | (uint32_t(rgba[i*4]) << 16);
    stbi_image_free(rgba);
}

void PaintScreen(HDC dc, const RECT& r) {
    HBRUSH bg = CreateSolidBrush(RGB(18, 27, 34)); FillRect(dc, &r, bg); DeleteObject(bg);
    RECT art = r;
    if (!pictures.empty() && !pictures[selected].pixels.empty()) {
        const Picture& image = pictures[selected % pictures.size()];
        const double scale = std::min(double(art.right) / image.width, double(art.bottom) / image.height);
        const int width = int(image.width * scale), height = int(image.height * scale);
        const LONG left=(art.right-width)/2, top=(art.bottom-height)/2;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = image.width; info.bmiHeader.biHeight = -image.height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, (art.right-width)/2, (art.bottom-height)/2, width, height, 0, 0, image.width, image.height,
                     image.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        art = {left,top,left+width,top+height};
    } else {
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(184, 202, 211));
        HFONT font = CreateFontA(-std::max(18L, r.bottom/24), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        HGDIOBJ old = SelectObject(dc, font);
        DrawTextA(dc, "ANARCHY ONLINE", -1, &art, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, old); DeleteObject(font);
    }
    PaintPhrase(dc, art, true);
}

LRESULT CALLBACK WindowProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(h, &ps);
        RECT r{}; GetClientRect(h, &r);
        PaintScreen(dc, r);
        EndPaint(h, &ps);
        if (painted) SetEvent(painted);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

DWORD WINAPI WindowMain(void*) {
    LoadPictures();
    SelectPicture(0);
    HINSTANCE module = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.hInstance = module; wc.lpfnWndProc = WindowProc; wc.lpszClassName = L"AOVKZoningScreen";
    RegisterClassW(&wc);
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName,
        L"AO-VK Zoning", WS_POPUP, 0, 0, 1, 1, owner.load(), nullptr, module, nullptr);
    if (!h) return 0;
    SetLayeredWindowAttributes(h, 0, 0, LWA_ALPHA);
    rvk::zoning::Reveal reveal;
    rvk::zoning::Fade fade;
    rvk::zoning::PhraseRotation phrases;
    LARGE_INTEGER seed{}; QueryPerformanceCounter(&seed);
    std::mt19937 random(uint32_t(seed.QuadPart) ^ uint32_t(seed.QuadPart >> 32) ^ GetCurrentProcessId());
    uint64_t seen = 0;
    bool shown = false;
    while (!stopping.load()) {
        const uint64_t serial = generation.load();
        if (serial != seen) {
            seen = serial; reveal.Start(GetTickCount64());
            SelectPicture(initialPicture.load() ? 0 : size_t(serial-1));
            selectedPhrase = phrases.Next(random);
            InvalidateRect(h, nullptr, FALSE);
        }
        if (reveal.active && !loading.load() && loadedGeneration.load() == serial && !reveal.settling)
            reveal.Loaded(loadedWorlds.load());
        HWND parent = owner.load();
        DWORD foregroundPid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
        RECT r{};
        const float opacity = fade.Step(GetTickCount64(), enabled.load() && reveal.Visible(GetTickCount64(), worlds.load()));
        bool visible = enabled.load() && opacity > 0 && parent &&
            IsWindow(parent) && IsWindowVisible(parent) && !IsIconic(parent) && foregroundPid == GetCurrentProcessId() &&
            GetClientRect(parent, &r) && r.right > 0 && r.bottom > 0;
        if (visible) {
            SetLayeredWindowAttributes(h, 0, BYTE(opacity * 255.f + .5f), LWA_ALPHA);
            POINT point{}; ClientToScreen(parent, &point);
            RECT old{}; GetWindowRect(h, &old);
            if (!shown || old.left != point.x || old.top != point.y || old.right-old.left != r.right || old.bottom-old.top != r.bottom) {
                SetWindowPos(h, HWND_TOP, point.x, point.y, r.right, r.bottom, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                InvalidateRect(h, nullptr, FALSE);
            }
            if (serial != 0 && painted && WaitForSingleObject(painted, 0) != WAIT_OBJECT_0) InvalidateRect(h, nullptr, FALSE);
        } else if (shown) ShowWindow(h, SW_HIDE);
        shown = visible;
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    DestroyWindow(h); pictures.clear();
    return 0;
}
}

#ifndef AOVK_ZONING_FIXTURE
void ZoningFrame() {
    if (!g_rvk.device || !g_rvk.window) return;
    owner = g_rvk.window;
    enabled = rvk_settings::Get("RVK_ZoneScreen") != 0;
    if (!loading.load()) initialPicture = g_rvk.device->WorldPresents() == 0;
    const uint64_t currentReset = resetSerial.load();
    worlds = g_rvk.device->TemporalEpoch() == currentReset ? g_rvk.device->FreshWorldPresents() : 0;
    if (!attempted) {
        HMODULE n3 = GetModuleHandleA("N3.dll");
        if (!n3) return;
        attempted = true;
        auto* at = reinterpret_cast<uint8_t*>(GetProcAddress(n3, "?SetTeleportStatus@n3Engine_t@@QAEX_N@Z"));
        const uint8_t expected[] = {0x55, 0x8b, 0xec, 0x8a, 0x45, 0x08};
        if (rnative::IdentifyClientModule(n3, rnative::ClientModule::N3) == rnative::ClientBuild::Unknown ||
            !at || std::memcmp(at, expected, sizeof(expected)) != 0) {
            RvkLog("zoning: unsupported N3 teleport entry; screen disabled"); return;
        }
        directory = rvk_settings::IniPath();
        const size_t slash = directory.find_last_of("\\/");
        directory = (slash == std::string::npos ? "" : directory.substr(0, slash+1)) + "randy-vk\\loading\\";
        painted = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        stopping = false;
        thread = CreateThread(nullptr, 0, WindowMain, nullptr, 0, nullptr);
        original = reinterpret_cast<TeleportFn>(rnative::HookAt(at, sizeof(expected), nullptr, 0,
            reinterpret_cast<void*>(Teleport), "zoning teleport status"));
        getEngine = reinterpret_cast<GetEngineFn>(GetProcAddress(n3, "?GetInstance@n3EngineClient_t@@SAPAV1@XZ"));
        auto* getter = reinterpret_cast<uint8_t*>(GetProcAddress(n3, "?GetTeleportStatus@n3Engine_t@@QAE_NXZ"));
        const uint8_t getBytes[] = {0x8a, 0x41, 0x58, 0xc3};
        if (getter && std::memcmp(getter, getBytes, sizeof(getBytes)) == 0) getStatus = reinterpret_cast<GetStatusFn>(getter);
        RvkLog("zoning: teleport hook %s; loading pictures from %s", original ? "installed" : "unavailable", directory.c_str());
    }
    // Initial login can set the flag before the first renderer frame installs the hook.
    if (getEngine && getStatus) if (void* engine = getEngine()) {
        if (getStatus(engine, nullptr)) BeginLoading(); else EndLoading();
    }
    static uint64_t consumed = 0;
    static uint64_t deviceSerial = 0;
    const uint64_t serial = resetSerial.load();
    if (serial != consumed || loading.load() || deviceSerial != g_rvk.deviceSerial) {
        consumed = serial;
        deviceSerial = g_rvk.deviceSerial;
        g_rvk.device->ResetTemporalHistory(serial);
    }
}

void ZoningStop() {
    stopping = true;
    owner = nullptr;
    if (thread && WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0) {
        CloseHandle(thread); thread = nullptr;
        if (painted) { CloseHandle(painted); painted = nullptr; }
    }
}
#endif
}
