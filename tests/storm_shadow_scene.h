// Three windows: steady scenery, transient weather, then recovery without
// toggling sun shadows. Weather-off exercises the legacy path as well.
void RunStormShadowScene(Device& dev,int frames,const std::string& shot)
{
    dev.SetHdr(true);dev.SetTonemap(1,1);dev.SetBloom(0,1);dev.SetDarkness(0,0);
    dev.SetShadows(true);dev.SetShadowParams(1,200,4);dev.SetShadowResolution(1024,128);
    dev.SetPixelLighting(false);dev.SetShadowMemory(1);
    Device::WeatherParams weather;weather.enable=false;dev.SetWeatherParams(weather);
    const uint32_t white=0xFFFFFFFF;Texture* tex=dev.CreateTexture(1,1,&white);
    const VtxDiffuseTex floor[]={{-60,-1,1,0xFF909090,0,0},{-60,-1,120,0xFF909090,0,1},
        {60,-1,1,0xFF909090,1,0},{60,-1,120,0xFF909090,1,1}};
    const VtxDiffuseTex wall[]={{-4,-1,15,0xFF909090,0,0},{-4,5,15,0xFF909090,0,1},
        {4,-1,15,0xFF909090,1,0},{4,5,15,0xFF909090,1,1}};
    const char* names[]={"VisualCloudlight_t<RRefFrame_t","VisualLightning_t<RRefFrame_t","VisualSingleCloud_t<RRefFrame_t"};
    const int first=frames/3,second=frames*2/3;
    for(int frame=0;frame<frames;++frame){
        if(frame==first/2||frame==first||frame==second)dev.ProfileWindow(true,"");
        if(frame==first-1||frame==second-1||frame==frames-1){
            const char* phase=frame==first-1?"before":frame==second-1?"during":"after";
            dev.ProfileWindow(false,(std::string(phase)+": ").c_str());
            dev.RequestFrameDump(std::string(phase)+".txt");
            dev.RequestScreenshot(frame==frames-1?shot:std::string(phase)+".bmp");
        }
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFF536575,1);
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(World,Identity());dev.SetTransform(View,Identity());
        dev.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.1f,1000));
        Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.diffuse={1,1,1,1};sun.direction={.5f,-.8f,.2f};
        dev.SetLight(0,sun);dev.LightEnable(0,true);
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_LIGHTING,0);dev.SetRenderState(RS_ALPHABLENDENABLE,0);
        dev.SetTexture(0,tex);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);
        dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_ALPHAARG2,TA_DIFFUSE);
        dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
        dev.SetDrawVisual(uint32_t(VisualKind::Static),"storm-shadow-floor");dev.DrawPrimitive(TriangleStrip,kFvfDiffuseTex,floor,4);
        dev.SetDrawVisual(uint32_t(VisualKind::Static),"storm-shadow-wall");dev.DrawPrimitive(TriangleStrip,kFvfDiffuseTex,wall,4);
        if((g_visualToggle&1)&&frame>=first&&frame<second){
            // Deliberately depth-writing weather: eligibility must follow the
            // visual's meaning even when it arrives with solid render states.
            for(int i=0;i<3;++i){
                auto world=Identity();world.m[3][0]=30.f+8.f*((frame-first)/70);world.m[3][1]=30;
                dev.SetTransform(World,world);dev.SetDrawVisual(uint32_t(VisualKind::Other),names[i]);
                dev.DrawPrimitive(TriangleStrip,kFvfDiffuseTex,wall,4);
            }
        }
        dev.SetDrawVisual(0,nullptr);dev.EndFrame();
    }
    dev.DestroyTexture(tex);
}
