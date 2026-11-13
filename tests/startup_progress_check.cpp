#include "startup.h"
#include <cstdio>
#include <thread>

int main() {
    using namespace rvk::startup;
    Begin(true); Stage("Loading cached pipelines (controlled slow-driver fixture)");
    Sleep(3000);
    HWND h=FindWindowW(L"AOVKPreview23Startup",nullptr);
    if(!h || !IsWindowVisible(h)) { std::puts("delayed window missing"); return 1; }
    for(int i=0;i<10;++i) {
        DWORD_PTR reply;
        if(!SendMessageTimeoutW(h,WM_NULL,0,0,SMTO_ABORTIFHUNG,200,&reply)) { std::puts("message pump frozen"); return 2; }
        Sleep(30);
    }
    Finish();
    if(IsWindow(h)) return 3;
    Begin(true); Stage("Fast launch fixture"); Sleep(30); Finish();
    if(FindWindowW(L"AOVKPreview23Startup",nullptr)) return 4;
    Begin(true);
    std::thread failure([] { Finish("Injected initialization failure; fixture only"); });
    Sleep(3000); h=FindWindowW(L"AOVKPreview23Startup",nullptr);
    if(!h) { failure.join(); return 5; }
    PostMessageW(h,WM_CLOSE,0,0); failure.join();
    std::puts("startup progress passed: delayed visibility, independent responsive pump, fast-launch suppression, close and failure path");
}
