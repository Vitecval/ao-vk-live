// Regression: deferred character records can destroy a Job while this frame's GPU cache still has its address.
#include "rvk.h"
#include <cstdio>
#include <new>
#include <type_traits>
namespace rvk {
struct SkinLifetimeCheck {
 static bool Run(Device& d){
  auto source=[](uint32_t n){auto s=std::make_shared<skin::Source>();for(uint32_t i=0;i<n;++i){skin::TriVertex v{};v.bind[0]=v.posA[0]=float(i%3);v.bind[1]=v.posA[1]=float(i/3);v.normal[2]=1;v.weightA=1;s->vertices.push_back(v);s->indices.push_back(uint16_t(i));}s->Finish();return s;};
  auto a=source(3),b=source(6);skin::Bone bone{};bone.m[0]=bone.m[4]=bone.m[8]=1;
  auto pa=std::make_shared<skin::Palette>();pa->Set(&bone,1);bone.m[9]=2;auto pb=std::make_shared<skin::Palette>();pb->Set(&bone,1);
  std::aligned_storage_t<sizeof(skin::Job),alignof(skin::Job)> storage;
  d.BeginFrame();
  auto* first=new(&storage) skin::Job;first->source=a;first->bones=pa;first->ComputeBounds();auto* out=d.SkinOnGpu(*first);bool ok=out&&out->mesh->source==a;auto offset=out?out->vertexOffset:0;first->~Job();
  auto* second=new(&storage) skin::Job;second->source=b;second->bones=pb;second->ComputeBounds();out=d.SkinOnGpu(*second);
  const bool reused=out&&out->mesh->source==b&&out->vertexOffset!=offset;
  std::printf("reused Job address selects new mesh/pose: %s (wrong mesh is never drawn)\n",reused?"PASS":"FAIL");ok&=reused;
  if(reused){auto secondOffset=out->vertexOffset;second->bones=pa;second->ComputeBounds();out=d.SkinOnGpu(*second);const bool changed=out&&out->vertexOffset!=secondOffset;std::printf("changed pose at same Job address redispatches: %s\n",changed?"PASS":"FAIL");ok&=changed;}
  second->~Job();
  std::aligned_storage_t<sizeof(skin::Palette),alignof(skin::Palette)> poseStorage;
  auto* pose=new(&poseStorage) skin::Palette;pose->Set(&bone,1);VkDeviceSize bytes;auto poseAt=d.SkinBones(*pose,&bytes);pose->~Palette();
  bone.m[9]=7;pose=new(&poseStorage) skin::Palette;pose->Set(&bone,1);auto nextAt=d.SkinBones(*pose,&bytes);
  const bool poseReused=nextAt!=poseAt;std::printf("reused Palette address uploads new bones: %s\n",poseReused?"PASS":"FAIL");ok&=poseReused;pose->~Palette();
  auto* meshJob=new(&storage) skin::Job;meshJob->source=a;meshJob->bones=pa;meshJob->ComputeBounds();d.m_drawSkin=meshJob;d.DrawMeshInfo(skin::kVertexFvf,sizeof(skin::Vertex),nullptr,3,a->indices.data(),3);auto meshKey=d.m_drawMeshKey;meshJob->~Job();
  meshJob=new(&storage) skin::Job;meshJob->source=a;meshJob->bones=pb;meshJob->ComputeBounds();d.m_drawSkin=meshJob;d.DrawMeshInfo(skin::kVertexFvf,sizeof(skin::Vertex),nullptr,3,a->indices.data(),3);
  const bool meshReused=d.m_drawMeshKey!=meshKey;std::printf("reused same-size Job address refreshes bounds: %s\n",meshReused?"PASS":"FAIL");ok&=meshReused;d.m_drawSkin=nullptr;meshJob->~Job();
  d.EndFrame();d.WaitIdle();
  ok&=!d.m_deviceLost;return ok;
 }
};
}
int main(){rvk::Device d;std::string error;if(!d.Init(nullptr,320,240,&error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}return rvk::SkinLifetimeCheck::Run(d)?0:1;}
