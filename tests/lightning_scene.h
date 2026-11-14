// Isolate the steady storm from the strike, legacy glow overdraw and near bolts.
void RunLightningScene(Device& dev,int frames,const std::string& shot)
{
    dev.SetHdr(true);dev.SetTonemap(1,1);dev.SetBloom(.4f,1);dev.SetDarkness(0,0);dev.SetSkyPlayfield(655);
    dev.SetShadows(false);dev.SetPixelLighting(false);
    Device::WeatherParams weather;weather.enable=true;weather.force=.4f;weather.stormForce=1;weather.strikeRate=0;weather.splashes=0;
    if(g_visualToggle&32){weather.force=0;weather.stormForce=0;}
    dev.SetWeatherParams(weather);dev.SetKnob(Knob::WeatherMode,0);
    if(g_visualToggle&32)dev.SetKnob(Knob::SandFog,0); // Clear-air control isolates the flash from rain/fog evolution.
    dev.SetKnob(Knob::Lightning,(g_visualToggle&8)?0:1);dev.SetKnob(Knob::StormBolt,1);dev.SetKnob(Knob::StormFlash,(g_visualToggle&16)?0:1);
    const uint32_t white=0xFFFFFFFF;Texture* tex=dev.CreateTexture(1,1,&white);
    const VtxDiffuseTex floor[]={{-60,-1,1,0xFF606B72,0,0},{-60,-1,120,0xFF606B72,0,1},
        {60,-1,1,0xFF606B72,1,0},{60,-1,120,0xFF606B72,1,1}};
    const VtxDiffuseTex glow[]={{-300,-150,160,0x20DCE8FF,0,1},{-300,450,160,0x20DCE8FF,0,0},
        {300,-150,160,0x20DCE8FF,1,1},{300,450,160,0x20DCE8FF,1,0}};
    for(int frame=0;frame<frames;++frame){
        if(frame==frames/2)dev.ProfileWindow(true,"");
        if(frame==frames-1){dev.ProfileWindow(false,"strike window: ");dev.RequestScreenshot(shot);dev.RequestFrameDump("lightning-frame.txt");}
        dev.BeginFrame();dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xFF536575,1);
        dev.SetViewport({0,0,g_width,g_height,0,1});dev.SetTransform(World,Identity());dev.SetTransform(View,Identity());
        dev.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.1f,1000));
        dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_ZFUNC,CMP_LESSEQUAL);
        dev.SetRenderState(RS_CULLMODE,CULL_NONE);dev.SetRenderState(RS_LIGHTING,0);dev.SetRenderState(RS_ALPHABLENDENABLE,0);
        dev.SetTexture(0,tex);dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);
        dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
        dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_ALPHAARG2,TA_DIFFUSE);
        dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);dev.SetDrawVisual(uint32_t(VisualKind::Static),"storm-fixture-floor");
        dev.DrawPrimitive(TriangleStrip,kFvfDiffuseTex,floor,4);
        if((g_visualToggle&1)&&frame>=frames/2){
            if(frame%25==0){const float at[]={0,5,(g_visualToggle&4)?1.f:120.f};dev.TriggerLightning(at);}
            if(g_visualToggle&2){
                dev.SetDrawVisual(uint32_t(VisualKind::Unknown),"VisualCloudlight_t");
                dev.SetRenderState(RS_ZWRITEENABLE,0);dev.SetRenderState(RS_ALPHABLENDENABLE,1);
                dev.SetRenderState(RS_SRCBLEND,BLEND_SRCALPHA);dev.SetRenderState(RS_DESTBLEND,BLEND_ONE);
                for(int layer=0;layer<40;++layer)dev.DrawPrimitive(TriangleStrip,kFvfDiffuseTex,glow,4);
            }
        }
        dev.SetDrawVisual(0,nullptr);dev.EndFrame();
    }
    dev.DestroyTexture(tex);
}
