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
        selectedPhrase=rvk::zoning::kLoadingPhraseCount;
        showContinue=true;continueOpacity=0;PaintScreen(dc,client);GdiFlush();
        if(std::memcmp(pixels,baseline.data(),baseline.size()*4)!=0)return 36; // Readiness never replaces/clears the picture.
        continueOpacity=.5f;PaintScreen(dc,client);GdiFlush();
        const auto* half=static_cast<uint32_t*>(pixels);
        const std::vector<uint32_t> halfway(half,half+baseline.size());
        continueOpacity=1;PaintScreen(dc,client);GdiFlush();
        size_t promptPixels=0;
        const auto* prompt=static_cast<uint32_t*>(pixels);
        for(LONG y=0;y<client.bottom;++y)for(LONG x=0;x<client.right;++x){
            const size_t at=size_t(y)*client.right+x;
            if(prompt[at]==baseline[at])continue;
            ++promptPixels;
            if(x<art.left||x>=art.right||y<art.bottom-std::clamp(int(width)/64,14,28)-std::max(4L,height/65)-1||y>=art.bottom)return 26;
        }
        if(promptPixels<50)return 27;
        size_t halfPixels=0;
        for(size_t i=0;i<baseline.size();++i){
            if(halfway[i]!=baseline[i])++halfPixels;
            if(prompt[i]==baseline[i] && halfway[i]!=baseline[i])return 37;
            for(int c=0;c<24;c+=8){
                const auto a=(baseline[i]>>c)&255u,b=(prompt[i]>>c)&255u,m=(halfway[i]>>c)&255u;
                if(m<std::min(a,b)||m>std::max(a,b))return 38;
            }
        }
        if(halfPixels<50 || halfway==std::vector<uint32_t>(prompt,prompt+baseline.size()))return 39;
        if(argc==3){
            const std::string path=std::string(argv[2])+"/continue-"+std::to_string(client.right)+"x"+std::to_string(client.bottom)+".bmp";
            FILE* file=std::fopen(path.c_str(),"wb");if(!file)return 28;
            BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
            header.bfSize=header.bfOffBits+client.right*client.bottom*4;
            std::fwrite(&header,sizeof(header),1,file);std::fwrite(&info.bmiHeader,sizeof(BITMAPINFOHEADER),1,file);
            std::fwrite(pixels,size_t(client.right)*client.bottom*4,1,file);std::fclose(file);
        }
        showContinue=false;
        SelectObject(dc,old); DeleteObject(bitmap);
    }
    DeleteDC(dc);
    // Input is accepted only for the ready transition and its foreground owner.
    const HWND inputOwner=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"hidden zoning input fixture",WS_POPUP,
        0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!inputOwner)return 29;
    owner=inputOwner;generation=100;readyGeneration=0;waitForClick=true;enabled=true;
    if(ZoningMessage(inputOwner,WM_KEYDOWN,VK_SPACE,0))return 19;
    readyGeneration=100;
    if(ZoningInput(inputOwner,WM_KEYDOWN,'W',LPARAM(1)<<30,true) ||
       ZoningInput(inputOwner,WM_SYSKEYDOWN,VK_F4,0,true) || ZoningInput(inputOwner,WM_KEYDOWN,VK_SHIFT,0,true))return 20;
    if(!ZoningInput(inputOwner,WM_KEYDOWN,VK_SPACE,0,true) || continuedGeneration.load()!=100)return 21;
    generation=101;
    if(ZoningInput(inputOwner,WM_LBUTTONDOWN,0,0,true))return 22;
    readyGeneration=101;
    if(!ZoningInput(inputOwner,WM_LBUTTONDOWN,0,0,true) || continuedGeneration.load()!=101)return 23;
    if(ZoningMessage(nullptr,WM_LBUTTONDOWN,0,0))return 24;
    if(ZoningInput(inputOwner,WM_KEYDOWN,VK_RETURN,0,false))return 30;
    waitForClick=false;
    if(ZoningMessage(inputOwner,WM_KEYDOWN,VK_RETURN,0))return 25;
    owner=nullptr;generation=0;readyGeneration=0;continuedGeneration=0;
    DestroyWindow(inputOwner);
    std::puts("ready foreground zoning accepts fresh keys and clicks; early/repeated/stale/system/background input ignored");
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
    waitForClick=true;worlds=4;
    for(int i=0;i<200 && readyGeneration.load()!=serial;++i)Sleep(10);
    if(readyGeneration.load()!=serial || !captureInput.load())return 31;
    const auto samePicture=pictureSerial;const auto sameSelection=selected;const auto samePhrase=selectedPhrase;
    Sleep(1000);
    if(readyGeneration.load()!=serial || IsWindowVisible(popup) || GetForegroundWindow()!=foreground)return 32;
    if(pictureSerial!=samePicture||selected!=sameSelection||selectedPhrase!=samePhrase)return 40;
    if(!ZoningInput(parent,WM_LBUTTONDOWN,0,0,true))return 33;
    for(int i=0;i<200 && captureInput.load();++i)Sleep(10);
    if(captureInput.load() || readyGeneration.load()!=0)return 34;
    BeginLoading();EndLoading();waitForClick=false;worlds=4;
    Sleep(400);
    if(readyGeneration.load()!=0 || captureInput.load())return 35;
    stopping = true;
    if (WaitForSingleObject(thread, 2000) != WAIT_OBJECT_0) return 10;
    CloseHandle(thread); CloseHandle(painted); DestroyWindow(parent);
    std::puts("independent zoning thread closes cleanly, suppresses hidden/background client and never takes focus");
    std::puts("ready screen holds until input, releases after fading, and the next transition with the option off finishes automatically");
}
