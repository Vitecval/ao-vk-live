#include "startup.h"
#include "rvk.h"
#include <atomic>
#include <cstdio>
#include <cstring>

namespace rvk::startup {
namespace {
SRWLOCK lock = SRWLOCK_INIT;
char operation[256] = "Preparing renderer...";
char failure[768] = {};
HANDLE stopEvent = nullptr, uiThread = nullptr;
std::atomic<bool> active{false}, stopRequested{false};
std::atomic<uint64_t> driverUs{0}, pipelines{0};
int64_t began = 0;
int64_t Now() { LARGE_INTEGER n; QueryPerformanceCounter(&n); return n.QuadPart; }
uint64_t Micros(int64_t delta) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return uint64_t(delta * 1000000 / f.QuadPart); }
LRESULT CALLBACK WindowProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_CLOSE) { DestroyWindow(h); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT r; GetClientRect(h, &r);
        HBRUSH bg = CreateSolidBrush(RGB(18, 29, 38)); FillRect(dc, &r, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(210, 227, 236));
        HFONT font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ old = SelectObject(dc, font);
        RECT title{24, 22, 530, 58}; DrawTextW(dc, L"AO-VK Live - Preparing Vulkan Renderer", -1, &title, DT_LEFT);
        char status[1024];
        AcquireSRWLockShared(&lock);
        std::snprintf(status, sizeof(status), "%s", failure[0] ? failure : operation);
        const bool failed = failure[0] != 0;
        ReleaseSRWLockShared(&lock);
        RECT body{24, 70, 526, 145}; DrawTextA(dc, status, -1, &body, DT_LEFT | DT_WORDBREAK);
        if (!failed) {
            // Indeterminate by design: the driver does not report remaining compilation work.
            RECT track{24, 152, 526, 156}; HBRUSH dim = CreateSolidBrush(RGB(38, 57, 68));
            FillRect(dc, &track, dim); DeleteObject(dim);
            int x = 24 + int((GetTickCount64() / 12) % 442);
            RECT pulse{x, 152, x + 60, 156}; HBRUSH accent = CreateSolidBrush(RGB(79, 191, 205));
            FillRect(dc, &pulse, accent); DeleteObject(accent);
        }
        SelectObject(dc, old); DeleteObject(font); EndPaint(h, &ps); return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}
DWORD WINAPI UiMain(void*) {
    // A separate message pump stays responsive even inside a blocking Vulkan driver call.
    if (WaitForSingleObject(stopEvent, 2500) == WAIT_OBJECT_0) return 0;
    const HINSTANCE module = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = module;
    wc.lpszClassName = L"AOVKPreview23Startup"; wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&wc);
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"AO-VK Live",
        WS_CAPTION | WS_SYSMENU, (GetSystemMetrics(SM_CXSCREEN)-560)/2,
        (GetSystemMetrics(SM_CYSCREEN)-220)/2, 560, 220, nullptr, nullptr, module, nullptr);
    if (!h) return 0;
    ShowWindow(h, SW_SHOWNOACTIVATE);
    bool done = false;
    while (!done && !stopRequested.load()) {
        DWORD wait = MsgWaitForMultipleObjects(1, &stopEvent, FALSE, 80, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) break;
        MSG msg;
        for (int count=0;count<128 && !stopRequested.load() && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE);++count) {
            if (msg.message == WM_QUIT) { done = true; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        InvalidateRect(h, nullptr, FALSE);
        Sleep(16); // Bound paint traffic even when other windows continually send messages.
    }
    if (IsWindow(h)) DestroyWindow(h);
    return 0;
}
}
bool Active() { return active.load(std::memory_order_relaxed); }
uint64_t DriverMicroseconds() { return driverUs.load(); }
void Begin(bool progressWindow) {
    if (active.exchange(true)) return;
    began = Now(); driverUs = 0; pipelines = 0; stopRequested=false;
    AcquireSRWLockExclusive(&lock); failure[0]=0; std::strcpy(operation,"Preparing renderer..."); ReleaseSRWLockExclusive(&lock);
    Log("startup-event: renderer entry pid=%lu tid=%lu", GetCurrentProcessId(), GetCurrentThreadId());
    if (progressWindow && !uiThread) {
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent) uiThread = CreateThread(nullptr, 0, UiMain, nullptr, 0, nullptr);
        if (!uiThread && stopEvent) { CloseHandle(stopEvent); stopEvent = nullptr; }
    }
}
void Stage(const char* value) {
    if (!Active()) return;
    AcquireSRWLockExclusive(&lock); std::snprintf(operation, sizeof(operation), "%s", value); ReleaseSRWLockExclusive(&lock);
    Log("startup-event: +%llu ms tid=%lu stage=%s", static_cast<unsigned long long>(Micros(Now()-began)/1000), GetCurrentThreadId(), value);
}
void Finish(const char* error) {
    if (!active.exchange(false)) return;
    Log("startup-event: +%llu ms %s; driver=%.3f ms pipelines=%llu", static_cast<unsigned long long>(Micros(Now()-began)/1000),
        error ? error : "first successful presentation (responsiveness not yet established)", driverUs.load()/1000.0,
        static_cast<unsigned long long>(pipelines.load()));
    if (error) {
        AcquireSRWLockExclusive(&lock); std::snprintf(failure, sizeof(failure), "Renderer initialization failed: %s\nSee randy-vk.log. Close this window to dismiss.", error); ReleaseSRWLockExclusive(&lock);
        // Bounded error visibility; no modal interaction on the rendering thread.
        if (uiThread) WaitForSingleObject(uiThread, 10000);
    }
    stopRequested=true;
    if (stopEvent) SetEvent(stopEvent);
    if (uiThread) { WaitForSingleObject(uiThread, INFINITE); CloseHandle(uiThread); uiThread = nullptr; }
    if (stopEvent) { CloseHandle(stopEvent); stopEvent = nullptr; }
}
DriverCall::DriverCall(const char* name) : m_operation(name), m_begin(Now()), m_active(Active()) {}
DriverCall::~DriverCall() {
    if (!m_active) return;
    const auto us = Micros(Now()-m_begin); driverUs += us;
    if (us >= 250000) Log("startup-driver: tid=%lu %s %.3f ms", GetCurrentThreadId(), m_operation, us/1000.0);
}
VkResult GraphicsPipelines(VkDevice d, VkPipelineCache c, uint32_t n, const VkGraphicsPipelineCreateInfo* i, const VkAllocationCallbacks* a, VkPipeline* p) {
    DriverCall timer("vkCreateGraphicsPipelines");
    const VkResult r = vk::vkCreateGraphicsPipelines(d,c,n,i,a,p);
    if (Active() && r == VK_SUCCESS) {
        pipelines += n; char text[256]; std::snprintf(text,sizeof(text),"Preparing graphics pipelines: %llu completed",static_cast<unsigned long long>(pipelines.load()));
        AcquireSRWLockExclusive(&lock); std::strcpy(operation,text); ReleaseSRWLockExclusive(&lock);
    }
    return r;
}
VkResult ComputePipelines(VkDevice d, VkPipelineCache c, uint32_t n, const VkComputePipelineCreateInfo* i, const VkAllocationCallbacks* a, VkPipeline* p) {
    DriverCall timer("vkCreateComputePipelines"); return vk::vkCreateComputePipelines(d,c,n,i,a,p);
}
}
