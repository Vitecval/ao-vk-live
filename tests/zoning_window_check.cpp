#define AOVK_ZONING_FIXTURE
#include "../proxy/ddraw/rvk_zoning.cpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    using namespace rvkproxy;
    if (argc < 2 || argc > 3) return 1;
    directory = std::string(argv[1]) + "\\";
    LoadPictures();
    if (pictures.size() != 14) return 2;
    for (size_t i = 0; i < pictures.size(); ++i) {
        SelectPicture(i);
        const auto& picture = pictures[i];
        if (picture.pixels.empty()) return 11;
        for (size_t j = 0; j < pictures.size(); ++j) if (j != i && !pictures[j].pixels.empty()) return 12;
        if (picture.width < 1600 || picture.height < 900 || std::abs(double(picture.width)/picture.height-1.8) > .06) return 3;
        double sum = 0; size_t dark = 0, bright = 0;
        for (uint32_t pixel : picture.pixels) {
            const double luma = .2126*((pixel>>16)&255) + .7152*((pixel>>8)&255) + .0722*(pixel&255);
            sum += luma; dark += luma < 5; bright += luma > 250;
        }
        const double mean = sum/picture.pixels.size();
        if (mean < 35 || mean > 180 || double(dark)/picture.pixels.size() > .02 || double(bright)/picture.pixels.size() > .02) return 4;
        std::printf("artwork %dx%d: mean luminance %.1f, crushed %.3f%%, clipped %.3f%%\n", picture.width, picture.height,
            mean, 100.*dark/picture.pixels.size(), 100.*bright/picture.pixels.size());
    }
    // Exercise the production painter against every supplied phrase at common
    // aspect ratios, including long Unicode copy and compact game windows.
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return 13;
    for (const auto& size : {std::pair<LONG,LONG>{640,480}, {1280,720}, {1920,1080}, {2560,1080}, {720,1280}}) {
        RECT client{0,0,size.first,size.second};
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = client.right; info.bmiHeader.biHeight = -client.bottom;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) return 14;
        HGDIOBJ old = SelectObject(dc, bitmap);
        SelectPicture(0);
        const auto& picture=pictures[selected];
        const double scale=std::min(double(client.right)/picture.width,double(client.bottom)/picture.height);
        const LONG width=LONG(picture.width*scale),height=LONG(picture.height*scale);
        const RECT art{(client.right-width)/2,(client.bottom-height)/2,(client.right+width)/2,(client.bottom+height)/2};
        selectedPhrase=rvk::zoning::kLoadingPhraseCount;
        PaintScreen(dc,client); GdiFlush();
        const auto* first=static_cast<const uint32_t*>(pixels);
        const std::vector<uint32_t> baseline(first,first+size_t(client.right)*client.bottom);
        for (selectedPhrase = 0; selectedPhrase < rvk::zoning::kLoadingPhraseCount; ++selectedPhrase) {
            const int textHeight = PaintPhrase(dc, art, false);
            if (textHeight < 14 || textHeight > height / 2) return 15;
            PaintScreen(dc, client); GdiFlush();
            size_t changed=0;
            auto* rgb=static_cast<uint32_t*>(pixels);
            for (LONG y=0;y<client.bottom;++y) for (LONG x=0;x<client.right;++x) {
                const size_t pixel=size_t(y)*client.right+x;
                if (rgb[pixel]==baseline[pixel]) continue;
                ++changed;
                // Only the outlined text may change. The AO logo, full image
                // edges and letterbox background must remain pixel-identical.
                if (x<art.left || x>=art.right || y<art.top || y>art.top+int(height*.84f)) return 16;
            }
            if (changed<100) return 18;
            if (argc == 3 && selectedPhrase == 2) {
                const std::string path = std::string(argv[2])+"/phrase-"+std::to_string(client.right)+"x"+std::to_string(client.bottom)+".bmp";
                FILE* file = std::fopen(path.c_str(), "wb"); if (!file) return 17;
                BITMAPFILEHEADER header{}; header.bfType=0x4d42;
                header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
                header.bfSize=header.bfOffBits+client.right*client.bottom*4;
                std::fwrite(&header,sizeof(header),1,file); std::fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,file);
                std::fwrite(pixels,size_t(client.right)*client.bottom*4,1,file); std::fclose(file);
            }
        }
        SelectObject(dc,old); DeleteObject(bitmap);
    }
    DeleteDC(dc);
    selectedPhrase = rvk::zoning::kLoadingPhraseCount;
    std::puts("all 50 Unicode phrases paint and wrap at five aspect ratios over the complete uncropped image, above its untouched logo");
    pictures.clear();
    HWND foreground = GetForegroundWindow();
    HWND parent = CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"AO-VK hidden zoning fixture", WS_POPUP,
        0, 0, 640, 360, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!parent) return 5;
    owner = parent;
    painted = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    thread = CreateThread(nullptr, 0, WindowMain, nullptr, 0, nullptr);
    if (!thread) return 6;
    BeginLoading();
    HWND popup = nullptr;
    for (int i = 0; i < 200 && !popup; ++i) { Sleep(10); popup = FindWindowW(L"AOVKZoningScreen", L"AO-VK Zoning"); }
    if (!popup || IsWindowVisible(popup) || GetForegroundWindow() != foreground) return 7;
    const auto serial = generation.load();
    BeginLoading();
    if (generation.load() != serial) return 8;
    EndLoading();
    if (loading.load() || loadedGeneration.load() != serial || worlds.load() != 0) return 9;
    stopping = true;
    if (WaitForSingleObject(thread, 2000) != WAIT_OBJECT_0) return 10;
    CloseHandle(thread); CloseHandle(painted); DestroyWindow(parent);
    std::puts("independent zoning thread closes cleanly, suppresses hidden/background client and never takes focus");
}
