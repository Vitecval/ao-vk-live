// Controlled Vulkan-only fixture. Creates only its own small no-activate window.
#include "rvk.h"
#include "threaded.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {
PFN_vkGetDeviceProcAddr realGet;
unsigned injectedCalls=0;
VkResult VKAPI_PTR RejectPipeline(VkDevice,VkPipelineCache,uint32_t,const VkComputePipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline*) {
    ++injectedCalls; return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}
PFN_vkVoidFunction VKAPI_PTR GetProc(VkDevice device,const char* name) {
    if(std::strcmp(name,"vkCreateComputePipelines")==0)return reinterpret_cast<PFN_vkVoidFunction>(&RejectPipeline);
    return realGet(device,name);
}
}
template<class RenderDevice> int Run(int argc,char**argv) {
    const bool inject=argc>1 && std::string(argv[1])=="--fail-init";
    const bool native=argc>1 && (std::string(argv[1])=="--native-framegen" || std::string(argv[1])=="--native-threaded");
    const bool fg=native || (argc>1 && std::string(argv[1])=="--framegen");
    const unsigned width=native?1914:768,height=native?1051:512;
    using namespace rvk;using namespace rvk::d3d;
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"AOVKFsrLaboratory";RegisterClassW(&wc);
    RECT bounds{0,0,LONG(width),LONG(height)};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"AO-VK isolated FSR fixture",WS_OVERLAPPEDWINDOW,40,40,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!window)return 1;
    {
        RenderDevice d;std::string error;
        if(native){ShowWindow(window,SW_SHOWNOACTIVATE);SetWindowPos(window,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
        if(!d.Init(window,width,height,&error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
        realGet=vk::vkGetDeviceProcAddr;
        if(inject)vk::vkGetDeviceProcAddr=GetProc;
        d.SetHdr(true);d.SetTaa(true,.4f);d.SetRenderScale(native?100:67);d.SetFrameGen(fg&&!native);
        Matrix identity{};for(int i=0;i<4;++i)identity.m[i][i]=1;
        Matrix projection{};projection.m[0][0]=projection.m[1][1]=1;projection.m[2][2]=100.f/99.9f;projection.m[2][3]=1;projection.m[3][2]=-.1f*100.f/99.9f;
        struct V{float x,y,z,nx,ny,nz;uint32_t color;float u,v;};
        const V vertices[]={{-1,-1,3,0,0,-1,0xffffffff,0,1},{1,-1,3,0,0,-1,0xffffffff,1,1},{0,1,3,0,0,-1,0xffffffff,.5f,0}};
        Material material{};material.diffuse=material.ambient={1,1,1,1};
        for(int frame=0;frame<72;++frame){
            if(!native&&frame%12==0)d.SetUpscaler(unsigned(frame/12)%3);
            if(!native&&(frame==24 || frame==48))d.SetRenderScale(frame==24?50:80);
            if(native&&frame==12)d.SetFrameGen(true);
            if(!native&&!inject && frame==30){SetWindowLongPtrW(window,GWL_STYLE,WS_POPUP);if(!d.Resize(640,480))return 3;}
            if(!native&&!inject && frame==42){SetWindowLongPtrW(window,GWL_STYLE,WS_OVERLAPPEDWINDOW);if(!d.Resize(768,512))return 4;}
            if(fg && frame==54)d.SetFrameGen(false);
            if(fg && frame==60)d.SetFrameGen(true);
            MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
            d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff17212b,1);
            d.SetTransform(World,identity);d.SetTransform(View,identity);d.SetTransform(Projection,projection);
            d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_LIGHTING,1);d.SetRenderState(RS_AMBIENT,0xff808080);d.SetRenderState(RS_CULLMODE,CULL_NONE);
            d.SetMaterial(material);d.SetDrawVisual(uint32_t(VisualKind::Static),"controlled FSR lifecycle fixture");
            d.DrawPrimitive(TriangleList,0x152,vertices,3);
            if(native){d.InterfaceBegin(true);d.InterfaceEnd();}
            d.EndFrame();
            if(native){std::fprintf(stderr,"native toggle frame %d completed\n",frame);Sleep(16);}
        }
        if(inject){vk::vkGetDeviceProcAddr=realGet;if(!injectedCalls){std::puts("failure injection was not exercised");return 5;}std::printf("injected pipeline failures: %u; contexts must be logged once per mode\n",injectedCalls);}
    }
    DestroyWindow(window);
    std::puts("controlled FSR lifecycle completed; no AO client was used");
    return 0;
}
int main(int argc,char**argv) {
    if(argc>1 && std::string(argv[1])=="--native-threaded")return Run<rvk::ThreadedDevice>(argc,argv);
    return Run<rvk::Device>(argc,argv);
}
