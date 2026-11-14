// Drive the production material upload templates through controlled allocator
// failures. Ownership is counted independently of the upload implementation.
#include "material_maps.h"
#include <cstdio>
#include <unordered_set>
#include <new>
struct FailingDevice {
 unsigned creations=0,updates=0,failCreation=0,failUpdate=0;
 bool published=false;std::unordered_set<rvk::Texture*> live;
 ~FailingDevice(){for(auto* p:live)delete p;}
 rvk::Texture* CreateTexture(uint32_t,uint32_t,rvk::Format,uint32_t){
  if(++creations==failCreation)throw std::bad_alloc();auto* p=new rvk::Texture;live.insert(p);return p;
 }
 rvk::Texture* CreateTexture(uint32_t w,uint32_t h,const void*){auto* p=CreateTexture(w,h,rvk::Format::A8R8G8B8,1);UpdateTexture(p,0,0,0,w,h,nullptr,w*4);return p;}
 void UpdateTexture(rvk::Texture*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,const void*,uint32_t){if(++updates==failUpdate)throw std::bad_alloc();}
 void DestroyTexture(rvk::Texture* p){if(p){live.erase(p);delete p;}}
 void PublishMaterialMaps(rvk::Texture*,rvk::Texture* n,rvk::Texture* o,rvk::Texture* a,rvk::Texture* e,rvk::Texture* l,rvk::Texture* ln,bool,bool complete){
  published=complete;for(auto* p:{n,o,a,e,l,ln})DestroyTexture(p);
 }
};
int main(){
 rvk::maps::MipChain chain;chain.w=chain.h=4;chain.levels={{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},{1,2,3,4},{1}};
 unsigned failures=0;
 for(unsigned at=1;at<=3;++at){FailingDevice dev;dev.failUpdate=at;bool escaped=false;rvk::Texture* uploaded=nullptr;try{uploaded=rvk::maps::Upload(dev,chain);}catch(const std::bad_alloc&){escaped=true;}
  const bool okay=!escaped&&!uploaded&&dev.live.empty();std::printf("upload mip failure %u: %s (%zu textures retained)\n",at,okay?"PASS":"FAIL",dev.live.size());failures+=!okay;
 }
 rvk::maps::Decoded maps;maps.normal=maps.orm=maps.albedo=maps.emissive=maps.leaves=maps.leavesNormal=chain;
 for(bool creating:{true,false})for(unsigned at=1;at<=(creating?6u:18u);++at){FailingDevice dev;if(creating)dev.failCreation=at;else dev.failUpdate=at;
  bool escaped=false,complete=false;try{complete=rvk::maps::Replace(dev,nullptr,maps);}catch(const std::bad_alloc&){escaped=true;}
  const bool okay=!escaped&&!complete&&!dev.published&&dev.live.empty();std::printf("replace %s failure %u: %s (%zu textures retained)\n",creating?"allocation":"mip",at,okay?"PASS":"FAIL",dev.live.size());failures+=!okay;
 }
 {FailingDevice dev;const bool complete=rvk::maps::Replace(dev,nullptr,maps);const bool okay=complete&&dev.published&&dev.live.empty();std::printf("complete material publication: %s\n",okay?"PASS":"FAIL");failures+=!okay;}
 return failures?1:0;
}
