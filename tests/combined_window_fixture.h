// Controlled window identity beside real animated cosmetic actors. Offline only.
#include "material_maps.h"
#include "mesh_id.h"
inline rvk::Texture* combinedWall=nullptr;
inline int combinedWindows=0;
inline void PrepareCombinedWindow(rvk::Device& d,const char* file,int mode){
    combinedWindows=mode;
    rvk::maps::Image image;std::string error;
    if(!rvk::maps::LoadImage(file,&image,&error))throw std::runtime_error(error);
    std::vector<uint32_t> pixels(size_t(image.w)*image.h);
    for(size_t i=0;i<pixels.size();++i)pixels[i]=0xff000000u|uint32_t(image.rgba[i*4])<<16|uint32_t(image.rgba[i*4+1])<<8|image.rgba[i*4+2];
    combinedWall=d.CreateTexture(image.w,image.h,pixels.data());d.SetTextureRdbId(combinedWall,297408,1010004);
    d.SetKnob(rvk::Knob::WindowDepth,float(mode));d.SetKnob(rvk::Knob::WindowActivity,1);
}
inline void DrawCombinedWindow(rvk::Device& d){
    if(!combinedWall)return;
    using namespace rvk;using namespace rvk::d3d;
    struct V{float x,y,z,nx,ny,nz;uint32_t colour;float u,v;};
    // ICC window material on controlled architecture, placed behind Newland pedestrians.
    V vertices[]={{-6,0,0,0,0,-1,0xffffffff,0,1},{6,0,0,0,0,-1,0xffffffff,1,1},{-6,8,0,0,0,-1,0xffffffff,0,0},{6,8,0,0,0,-1,0xffffffff,1,0}};
    uint16_t indices[]={0,2,1,1,2,3};
    static bool registered=false;
    if(!registered){
        const auto* bytes=reinterpret_cast<const uint8_t*>(vertices);
        uint64_t geo=meshid::Geo(bytes,sizeof(V),4,int(offsetof(V,u)),indices,6),raw=meshid::Raw(0x152,bytes,sizeof(V),4,indices,6);
        const char name[]="Combined controlled window witness";uint32_t header[]={1,1,sizeof(name)},identity[]={1010001,296208},offset=0;uint16_t pieces[]={0,1};
        FILE* f=std::fopen("combined-window-model.bin","wb");if(!f)throw std::runtime_error("window identity file");
        std::fwrite("AOMESHID",1,8,f);std::fwrite(header,4,3,f);std::fwrite(&geo,8,1,f);std::fwrite(&raw,8,1,f);std::fwrite(identity,4,2,f);std::fwrite(pieces,2,2,f);std::fwrite(&offset,4,1,f);std::fwrite(name,1,sizeof(name),f);std::fclose(f);d.LoadMeshIds("combined-window-model.bin");registered=true;
    }
    Matrix world{};for(int i=0;i<4;++i)world.m[i][i]=1;
    if(worldParts.empty()){world.m[3][2]=8.6f;} // CC0 laboratory fixture, no original world cache needed
    else {world.m[3][0]=420;world.m[3][1]=25;world.m[3][2]=300;}
    d.SetTransform(World,world);
    float bounds[]={-6,0,0,6,8,8};d.SetWindowHouseBounds(bounds);d.SetDrawVisual(uint32_t(VisualKind::Static),"Combined controlled window witness");
    Material material{};material.diffuse=material.ambient={1,1,1,1};d.SetMaterial(material);d.SetTexture(0,combinedWall);d.SetTexture(1,nullptr);
    d.SetRenderState(RS_ALPHATESTENABLE,0);d.SetRenderState(RS_LIGHTING,1);d.SetRenderState(RS_ALPHABLENDENABLE,0);d.SetRenderState(RS_CULLMODE,CULL_NONE);
    d.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);d.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);d.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);d.SetTextureStageState(1,TSS_COLOROP,TOP_DISABLE);
    d.DrawIndexedPrimitive(TriangleList,0x152,vertices,4,indices,6);d.SetWindowHouseBounds(nullptr);
}
