inline int worldSkipPart=-1;
// Private original-world capture support, only compiled into the test executable.
#include <fstream>
#include <map>
#include <cstring>
struct WorldPart { rvk::d3d::Matrix matrix;rvk::Texture* texture=nullptr;bool terrain=false;uint32_t flags=0,ref=96,count=0;std::shared_ptr<const std::vector<uint8_t>> vertices,indices; };
static std::vector<WorldPart> worldParts;
static int worldGroundMode=0;
static rvk::Texture* worldLightmap=nullptr;
static bool LoadWorld(rvk::Device& d,const std::string& path){
    std::vector<uint8_t> bytes;
    if(path==":fixture:"){std::string value;if(!rvk::living::FixtureResource("world.tree",value))return false;bytes.assign(value.begin(),value.end());}
    else {std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return false;auto size=f.tellg();if(size<12||size>256*1024*1024)return false;bytes.resize(size_t(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),size))return false;}
    size_t at=0;
    auto take=[&](void* p,size_t n){if(n>bytes.size()-at)throw std::runtime_error("truncated private world");std::memcpy(p,bytes.data()+at,n);at+=n;};
    auto u32=[&](){uint32_t n;take(&n,4);return n;};std::map<std::string,rvk::Texture*> textures;
    try{char magic[4];take(magic,4);if(std::memcmp(magic,"RVKT",4)||u32()!=2)return false;auto count=u32();if(count>50000)return false;
        for(uint32_t i=0;i<count;++i){WorldPart p;p.flags=u32();p.ref=u32();take(&p.matrix,64);auto w=u32(),h=u32();if(!w||!h||w>512||h>512)return false;
            std::string key(size_t(w)*h*4+8,'\0');std::memcpy(key.data(),&w,4);std::memcpy(key.data()+4,&h,4);take(key.data()+8,size_t(w)*h*4);
            auto& texture=textures[key];if(!texture)texture=d.CreateTexture(w,h,key.data()+8);p.texture=texture;p.count=u32();if(!p.count||p.count>65535)return false;
            auto v=std::make_shared<std::vector<uint8_t>>(size_t(p.count)*32);take(v->data(),v->size());p.vertices=v;p.terrain=w==64&&h==64&&p.count==4&&p.matrix.m[3][0]==0&&p.matrix.m[3][1]==0&&p.matrix.m[3][2]==0;auto n=u32();if(n>2000000||n%3)return false;
            auto idx=std::make_shared<std::vector<uint8_t>>(size_t(n)*2);take(idx->data(),idx->size());for(size_t j=0;j<idx->size();j+=2){uint16_t x;std::memcpy(&x,idx->data()+j,2);if(x>=p.count)return false;}if(p.terrain&&n==6){float heights[4];for(int j=0;j<4;++j)std::memcpy(&heights[j],v->data()+j*32+4,4);bool across=(std::fabs(heights[0]-heights[2])>4||std::fabs(heights[1]-heights[3])>4)&&std::fabs(heights[0]-heights[2])<std::fabs(heights[1]-heights[3])-.01f;if(!across){uint16_t q[6]={0,3,1,1,3,2};std::memcpy(idx->data(),q,12);}}p.indices=idx;if(n%2){uint16_t padding;take(&padding,2);}worldParts.push_back(std::move(p));
        }return at==bytes.size();
    }catch(const std::exception& e){std::fprintf(stderr,"world fixture: %s\n",e.what());return false;}
}
static bool PrepareWorldGround(rvk::Device& d,const std::string& path,int mode){
    if(!mode)return true;std::ifstream f(path,std::ios::binary);uint32_t h[4];float cell;f.read(reinterpret_cast<char*>(h),16);f.read(reinterpret_cast<char*>(&cell),4);
    if(!f||h[0]!=0x3152474c||h[2]<2||h[3]<2||h[2]>1025||h[3]>1025)return false;
    auto g=std::make_shared<rvk::NativeGround>();g->id=h[1];g->cellsX=h[2]-1;g->cellsZ=h[3]-1;g->cellX=g->cellZ=cell;g->patch=32;g->heights.resize(size_t(h[2])*h[3]);f.read(reinterpret_cast<char*>(g->heights.data()),g->heights.size()*4);if(!f)return false;
    g->codes.resize(size_t(g->cellsX)*g->cellsZ);g->hiddenCells.assign(g->codes.size(),1);g->tiles.push_back(nullptr);std::map<rvk::Texture*,uint16_t> tiles;
    for(auto& p:worldParts)if(p.terrain){
        float pos[3];std::memcpy(pos,p.vertices->data(),12);int x=int(std::round(pos[0]/cell)),z=int(std::round(pos[2]/cell));if(x<0||z<0||x>=int(g->cellsX)||z>=int(g->cellsZ))return false;
        auto& tile=tiles[p.texture];if(!tile){tile=uint16_t(g->tiles.size());g->tiles.push_back(p.texture);}g->codes[size_t(z)*g->cellsX+x]=tile;g->hiddenCells[size_t(z)*g->cellsX+x]=0;
        auto v=std::make_shared<std::vector<uint8_t>>(p.count*40);
        for(uint32_t i=0;i<p.count;++i){float src[8],dst[10];std::memcpy(src,p.vertices->data()+i*32,32);std::memcpy(dst,src,32);dst[8]=src[0]/(cell*g->cellsX);dst[9]=src[2]/(cell*g->cellsZ);std::memcpy(v->data()+i*40,dst,40);}p.vertices=v;
    }
    auto lm=std::make_shared<std::vector<uint8_t>>(size_t((g->cellsX+1)/2)*g->cellsZ,255);d.SetNativeGround(g);d.SetNativeGroundLightmap(lm,g->cellsX,g->cellsZ,0);d.SetNativeGroundOn(mode==1);d.SetKnob(rvk::Knob::TerraBlend,mode==2?2.f:0.f);worldGroundMode=mode;
    uint32_t white=0xffbcbcbc;worldLightmap=d.CreateTexture(1,1,&white);return true;
}
static void DrawWorld(rvk::Device& d){using namespace rvk::d3d;Material mat{};mat.diffuse=mat.ambient={1,1,1,1};mat.power=16;d.SetMaterial(mat);
    for(const auto& p:worldParts){if(worldSkipPart>=0&&size_t(worldSkipPart)==size_t(&p-worldParts.data()))continue;d.SetTransform(World,p.matrix);d.SetTexture(0,p.texture);d.SetRenderState(RS_ALPHATESTENABLE,p.flags&2?1:0);d.SetRenderState(RS_ALPHAREF,p.ref);d.SetRenderState(RS_ALPHAFUNC,CMP_GREATER);
        if(p.terrain&&worldGroundMode){
            d.SetDrawVisual(uint32_t(rvk::VisualKind::Terrain),"cached-original-ground",0,0);d.SetRenderState(RS_LIGHTING,0);d.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);d.SetRenderState(RS_ALPHABLENDENABLE,0);d.DrawSharedIndexed(TriangleList,0x212,p.vertices,0,p.count,p.indices,uint32_t(p.indices->size()/2));
            d.SetRenderState(RS_LIGHTING,1);d.SetTexture(0,worldLightmap);d.SetTextureStageState(0,TSS_TEXCOORDINDEX,1);d.SetRenderState(RS_ALPHABLENDENABLE,1);d.SetRenderState(RS_SRCBLEND,BLEND_ZERO);d.SetRenderState(RS_DESTBLEND,BLEND_SRCCOLOR);d.DrawSharedIndexed(TriangleList,0x212,p.vertices,0,p.count,p.indices,uint32_t(p.indices->size()/2));
            d.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);d.SetRenderState(RS_ALPHABLENDENABLE,0);
        }else{d.SetDrawVisual(uint32_t(rvk::VisualKind::Static),"cached-original-statel",0,0);d.DrawSharedIndexed(TriangleList,0x112,p.vertices,0,p.count,p.indices,uint32_t(p.indices->size()/2));}
    }
}
