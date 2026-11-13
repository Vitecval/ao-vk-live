bool g_windowFixture=false; int g_windowMode=0; float g_windowAngle=0, g_windowRepeat=1;
float g_windowWallWidth=16,g_windowWallHeight=8,g_windowWallOffsetY=0;
float g_windowOriginX=0,g_windowOriginZ=0;bool g_windowBaked=false;
int g_windowModel=0,g_windowModel2=0;
int g_windowActivity=1;
bool g_windowBatchProbe=false;
bool g_windowCorner=false;
bool g_windowNoHouse=false;
bool g_windowMirrorY=false,g_windowHouseY=false;
bool g_windowZUp=false,g_windowBay=false;
float g_windowHouseMinY=0,g_windowHouseMaxY=4.5f;
float g_windowEyeY=3,g_windowRepeatV=0,g_windowHouseDepth=6;
bool g_windowFlatHouse=false,g_windowUnlit=false,g_windowEmissive=false;
int g_windowAlpha=255;
int g_windowAddressU=1,g_windowAddressV=1;
bool g_windowHouseBox=false;float g_windowCustomHouse[6]={};
bool g_wearFixture=false,g_wearFull=true,g_wearPbr=true,g_wearClassic=false,g_wearBlend=false,g_wearCombined=false,g_wearSweep=false,g_wearHotOff=false,g_wearDiscard=false;
int g_wearId=6088,g_wearDebug=0,g_wearLevel=0,g_wearVisual=3;
float g_wearDistance=12;
std::string g_wearMapRoot,g_wearCatalog,g_wearOriginal;
bool g_wearNight=false;
template<class RenderDevice>
void RunWeatheringFixture(RenderDevice& dev,int frames,const std::string& shot){
 auto load=[&](int id,const std::string& root){
 const std::string stem="1010004_"+std::to_string(id);
 auto file=[&](const char* ext){std::string p=root+"/"+stem+ext;return GetFileAttributesA(p.c_str())!=INVALID_FILE_ATTRIBUTES?p:std::string();};
 maps::Image image;std::string error;
 std::vector<uint32_t> original(128*128,0xff989b91);
 if(!g_wearOriginal.empty() || !file("_d.png").empty()){
  if(!maps::LoadImage(g_wearOriginal.empty()?file("_d.png"):g_wearOriginal,&image,&error)){std::printf("fixture image failed: %s\n",error.c_str());std::exit(2);}
  original.resize(size_t(image.w)*image.h);
  for(size_t i=0;i<original.size();++i)original[i]=uint32_t(g_windowAlpha)<<24|uint32_t(image.rgba[i*4])<<16|uint32_t(image.rgba[i*4+1])<<8|image.rgba[i*4+2];
 }
 Texture* wall=dev.CreateTexture(image.w?image.w:128,image.h?image.h:128,original.data());
 dev.SetTextureRdbId(wall,uint32_t(id),1010004);
 if(g_wearFull){maps::MaterialFiles f;f.albedo=file("_d.png");f.normal=file("_n.png");f.packed=file("_orm.png");f.emissive=file("_e.png");auto d=maps::Decode(f);maps::Attach(dev,wall,d);}
 return wall;
 };
 std::vector<Texture*> walls;
 if(g_wearCatalog.empty())walls.push_back(load(g_wearId,g_wearMapRoot));
 else {
  FILE* f=std::fopen(g_wearCatalog.c_str(),"r");if(!f){std::printf("catalog open failed\n");std::exit(2);}
  char line[2048];while(std::fgets(line,sizeof(line),f)){char* tab=std::strchr(line,'\t');if(!tab)continue;*tab++=0;tab[std::strcspn(tab,"\r\n")]=0;walls.push_back(load(std::atoi(line),tab));}
  std::fclose(f);if(walls.empty())std::exit(2);
 }
 Texture* wall=walls[0];
 // Opaque lit architectural controls; no gameplay, terrain, or UI overlays.
 VtxMesh v[4]={{-8,0,0,0,0,-1,0xffffffff,0,1},{8,0,0,0,0,-1,0xffffffff,1,1},{-8,8,0,0,0,-1,0xffffffff,0,0},{8,8,0,0,0,-1,0xffffffff,1,0}};
 if(g_windowFixture)for(auto& a:v){a.x*=g_windowWallWidth/16; a.y=a.y*g_windowWallHeight/8+g_windowWallOffsetY;}
 uint16_t ix[]={0,2,1,1,2,3};
 Material plain{{1,1,1,1},{1,1,1,1},{0,0,0,0},{0,0,0,0},0};
 dev.SetSkyPlayfield(g_windowFixture?g_skyZone:655);dev.SetKnob(Knob::BuildWear,float(g_wearLevel));dev.SetKnob(Knob::WearDebug,float(g_wearDebug));
 Device::PbrSettings pbr;pbr.enabled=g_wearPbr;pbr.albedoMaps=true;dev.SetPbr(pbr);dev.SetPixelLighting(!g_wearClassic);
 dev.SetGrassField(false,24,3,.5f,true);
 for(auto& a:v){a.u*=g_windowRepeat;a.v*=g_windowRepeatV>0?g_windowRepeatV:g_windowRepeat;}
 if(g_windowBaked)for(auto& a:v){a.x+=g_windowOriginX;a.z+=g_windowOriginZ;}
 if(g_windowMirrorY)for(auto& a:v){a.y=-a.y;a.ny=-a.ny;}
 if(g_windowZUp)for(auto& a:v){float y=a.y,ny=a.ny;a.y=-a.z;a.z=y;a.ny=-a.nz;a.nz=ny;}
 if(g_windowBay)for(auto& a:v)a.u=.3f+a.u*.7f;
 if(g_windowModel){
  // Controlled identity witness for shared-material safety tests, not a claim
  // that the quad is a captured retail building model.
  const auto* bytes=reinterpret_cast<const uint8_t*>(v);
  uint64_t geo=meshid::Geo(bytes,sizeof(VtxMesh),4,int(offsetof(VtxMesh,u)),ix,6);
  uint64_t raw=meshid::Raw(kFvfMesh,bytes,sizeof(VtxMesh),4,ix,6);
  const char name[]="Controlled identity witness";
  uint32_t header[]={1u,g_windowModel2?2u:1u,uint32_t(sizeof(name))};
  FILE* f=std::fopen("window-test-models.bin","wb");if(!f)std::exit(2);
  std::fwrite("AOMESHID",1,8,f);std::fwrite(header,4,3,f);
  for(uint32_t n=0;n<header[1];++n){
   uint32_t identity[]={1010001u,uint32_t(n?g_windowModel2:g_windowModel)};
   uint16_t pieces[]={0,1};uint32_t offset=0;
   std::fwrite(&geo,8,1,f);std::fwrite(&raw,8,1,f);std::fwrite(identity,4,2,f);
   std::fwrite(pieces,2,2,f);std::fwrite(&offset,4,1,f);
  }
  std::fwrite(name,1,sizeof(name),f);std::fclose(f);dev.LoadMeshIds("window-test-models.bin");
 }
 dev.SetKnob(Knob::WindowDepth,float(g_windowMode));
 dev.SetKnob(Knob::WindowActivity,float(g_windowActivity));

 for(int frame=0;frame<frames;++frame){
  if(g_wearSweep){
   const int segment=frames/8,phase=frame/segment,local=frame%segment;
   const int levels[]={0,1,0,2,0,1,0,2};
   if(local==0)dev.SetKnob(g_windowFixture?Knob::WindowDepth:Knob::BuildWear,float(levels[std::min(phase,7)]));
   if(local==segment/2)dev.ProfileWindow(true,"");
   if(local==segment-1){char label[80];std::snprintf(label,sizeof(label),"wear-phase-%d-level-%d: ",phase,levels[std::min(phase,7)]);dev.ProfileWindow(false,label);}
  }else if(frame==frames/2)dev.ProfileWindow(true,"");
  if(frame==frames-1){if(!g_wearSweep)dev.ProfileWindow(false,"wear: ");dev.RequestScreenshot(shot);dev.RequestFrameDump("wear-frame.txt");}
  if(g_wearHotOff && frame==frames/2)dev.SetKnob(g_windowFixture?Knob::WindowDepth:Knob::BuildWear,0);
  dev.BeginFrame();dev.SetViewport({0,0,g_width,g_height,0,1});dev.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff697b88,1);
  Matrix placement=Identity();if(!g_windowBaked){placement.m[3][0]=g_windowOriginX;placement.m[3][2]=g_windowOriginZ;}
  if(g_windowMirrorY)placement.m[1][1]=-1;
  if(g_windowZUp){placement.m[1][1]=0;placement.m[1][2]=-1;placement.m[2][1]=g_windowMirrorY?-1.f:1.f;placement.m[2][2]=0;}
  dev.SetTransform(World,placement);dev.SetTransform(View,LookAtLH({g_windowOriginX+std::sin(g_windowAngle)*g_wearDistance,g_windowEyeY,g_windowOriginZ-std::cos(g_windowAngle)*g_wearDistance},{g_windowOriginX,g_windowEyeY,g_windowOriginZ},{0,1,0}));
  dev.SetTransform(Projection,PerspectiveLH(kPi/3,float(g_width)/g_height,.1f,g_wearDistance>180?1200.f:200.f));
  dev.SetRenderState(RS_ZENABLE,1);dev.SetRenderState(RS_ZWRITEENABLE,1);dev.SetRenderState(RS_CULLMODE,CULL_NONE);
  dev.SetRenderState(RS_ALPHABLENDENABLE,g_wearBlend?1:0);dev.SetRenderState(RS_SRCBLEND,BLEND_SRCALPHA);dev.SetRenderState(RS_DESTBLEND,BLEND_INVSRCALPHA);
  dev.SetRenderState(RS_ALPHATESTENABLE,0);dev.SetRenderState(RS_LIGHTING,1);dev.SetRenderState(RS_AMBIENT,g_wearNight?0xff282e40:0xff909090);
  if(g_windowUnlit)dev.SetRenderState(RS_LIGHTING,0);
  plain.emissive=g_windowEmissive?Color{.4f,.4f,.4f,0}:Color{0,0,0,0};
  dev.SetMaterial(plain);dev.SetTexture(0,wall);dev.SetTexture(1,g_wearCombined?wall:nullptr);dev.SetDrawVisual(uint32_t(g_wearVisual),"Weathering material control");
  if(g_windowFixture){
   float house[]={-g_windowWallWidth*.5f,0,0,g_windowWallWidth*.5f,std::max(4.5f,g_windowWallHeight+g_windowWallOffsetY),g_windowFlatHouse?0:g_windowHouseDepth};
   if(g_windowHouseY){house[1]=g_windowHouseMinY;house[4]=g_windowHouseMaxY;}
   if(g_windowHouseBox)std::memcpy(house,g_windowCustomHouse,sizeof(house));
   if(g_windowMirrorY){float lo=house[1];house[1]=-house[4];house[4]=-lo;}
   if(g_windowZUp){float y0=house[1],y1=house[4];house[1]=-house[5];house[4]=-house[2];house[2]=y0;house[5]=y1;}
   if(g_windowBaked){house[0]+=g_windowOriginX;house[3]+=g_windowOriginX;house[2]+=g_windowOriginZ;house[5]+=g_windowOriginZ;}
   dev.SetWindowHouseBounds(g_windowNoHouse?nullptr:house);
  }
  Light sun{};sun.type=LIGHT_DIRECTIONAL;if(g_wearNight)sun.diffuse={.08f,.10f,.16f,1};else sun.diffuse={.45f,.43f,.4f,1};sun.direction={.3f,g_wearNight?.6f:-.6f,.7f};dev.SetLight(0,sun);dev.LightEnable(0,true);
  dev.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
  dev.SetTextureStageState(0,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(0,TSS_ALPHAARG1,TA_TEXTURE);
  for(int stage=0;stage<2;++stage){dev.SetTextureStageState(stage,TSS_ADDRESSU,g_windowAddressU);dev.SetTextureStageState(stage,TSS_ADDRESSV,g_windowAddressV);}
  dev.SetTextureStageState(0,TSS_MINFILTER,TFN_LINEAR);dev.SetTextureStageState(0,TSS_MAGFILTER,TFG_LINEAR);
  dev.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
  if(g_wearCombined){
   dev.SetTextureStageState(1,TSS_TEXCOORDINDEX,0); // This mesh has one UV set, used by both stages.
   dev.SetTextureStageState(0,TSS_COLOROP,TOP_ADD);dev.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE|0x20);
   dev.SetTextureStageState(1,TSS_COLOROP,TOP_MODULATE);dev.SetTextureStageState(1,TSS_COLORARG1,TA_TEXTURE);dev.SetTextureStageState(1,TSS_COLORARG2,TA_CURRENT);
   dev.SetTextureStageState(1,TSS_ALPHAOP,TOP_SELECTARG1);dev.SetTextureStageState(1,TSS_ALPHAARG1,TA_CURRENT);
   dev.SetTextureStageState(1,TSS_MINFILTER,TFN_LINEAR);dev.SetTextureStageState(1,TSS_MAGFILTER,TFG_LINEAR);
  }
  if(g_wearDiscard){dev.SetTextureStageState(1,TSS_COLOROP,TOP_SELECTARG1);dev.SetTextureStageState(1,TSS_COLORARG1,TA_DIFFUSE);}
  for(size_t i=0;i<walls.size();++i){
   if(walls.size()>1){
    const int cols=g_windowBatchProbe?2:12,rows=int((walls.size()+cols-1)/cols);
    Matrix w=Identity();w.m[0][0]=1.f/cols;w.m[1][1]=1.f/rows;
    w.m[3][0]=-8.f+(float(i%cols)+.5f)*16.f/cols;w.m[3][1]=float(i/cols)*8.f/rows;
    dev.SetTransform(World,w);dev.SetTexture(0,walls[i]);dev.SetTexture(1,g_wearCombined?walls[i]:nullptr);
   }
   dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,v,4,ix,6);
   if(g_windowFixture&&(g_windowCorner||g_windowBay)&&walls.size()==1){
    VtxMesh side[4];std::memcpy(side,v,sizeof(side));
    for(int j=0;j<4;++j){
     side[j].x=g_windowWallWidth*.5f+(g_windowBaked?g_windowOriginX:0);
      side[j].y=(j>=2?g_windowWallHeight:0)+g_windowWallOffsetY;
      side[j].z=((j&1)?(g_windowBay?2.4f:6.f):0.f)+(g_windowBaked?g_windowOriginZ:0);
     side[j].nx=1;side[j].ny=0;side[j].nz=0;
      if(g_windowBay)side[j].u=(j&1)?.3f:0.f;
      if(g_windowMirrorY){side[j].y=-side[j].y;side[j].ny=-side[j].ny;}
      if(g_windowZUp){float y=side[j].y,ny=side[j].ny;side[j].y=-side[j].z;side[j].z=y;side[j].ny=-side[j].nz;side[j].nz=ny;}
    }
    dev.DrawIndexedPrimitive(TriangleList,kFvfMesh,side,4,ix,6);
   }
  }
  dev.EndFrame();
 }
 for(auto* t:walls)dev.DestroyTexture(t);
}
