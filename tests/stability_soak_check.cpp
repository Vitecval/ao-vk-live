// Repeated real GPU resource/zone lifetimes; no retail client or external assets.
#include "internal.h"
#include <psapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <vector>
#ifdef AOVK_TEST_HEAP_TRACE
extern "C" bool AoAuditHeapSelfTest();
extern "C" uint64_t AoAuditHeapLiveBytes();
extern "C" uint64_t AoAuditHeapLiveCount();
extern "C" void AoAuditHeapSnapshot(const char* path);
#else
static uint64_t AoAuditHeapLiveBytes(){return 0;}
static uint64_t AoAuditHeapLiveCount(){return 0;}
static void AoAuditHeapSnapshot(const char*){}
#endif
namespace {
PFN_vkCmdPushDescriptorSetKHR originalPushDescriptors;
PFN_vkGetBufferMemoryRequirements describeAuditBuffer;
VkDevice descriptorAuditDevice;
unsigned descriptorViolations=0;
VKAPI_ATTR void VKAPI_CALL AuditDescriptors(VkCommandBuffer command,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t set,uint32_t count,const VkWriteDescriptorSet* writes){
 for(uint32_t i=0;i<count;++i){const auto& write=writes[i];const bool storage=write.descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;const bool uniform=write.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  if((storage||uniform)&&write.pBufferInfo)for(uint32_t j=0;j<write.descriptorCount;++j){const auto& buffer=write.pBufferInfo[j];if(!buffer.buffer)continue;
   VkDeviceSize range=buffer.range;
   if(range==VK_WHOLE_SIZE){VkMemoryRequirements allocation{};describeAuditBuffer(descriptorAuditDevice,buffer.buffer,&allocation);range=allocation.size>buffer.offset?allocation.size-buffer.offset:0;}
   if(buffer.offset%256||(storage&&range>(128u<<20))||(uniform&&range>16384)){
    if(descriptorViolations++<16)std::printf("descriptor limit violation: binding %u, offset %llu, range upper bound %llu, %s\n",write.dstBinding,(unsigned long long)buffer.offset,(unsigned long long)range,storage?"storage":"uniform");
   }
  }
 }
 originalPushDescriptors(command,point,layout,set,count,writes);
}
}
namespace rvk {
struct StabilityAuditCheck {
 static d3d::Matrix View(float x){
   // Camera at (x,8,-16), facing slightly down towards the middle of the meadow.
   const float z=1.f/std::sqrt(1.0625f),y=-.25f*z;
   auto m=detail::Identity();m.m[1][1]=z;m.m[1][2]=y;m.m[2][1]=-y;m.m[2][2]=z;
   m.m[3][0]=-x;m.m[3][1]=-8*z-16*y;m.m[3][2]=-8*y+16*z;return m;
 }
 static bool Run(double duration,unsigned framesPerZone,bool mixed,bool limited){
   using namespace d3d;Device::SetFixedClock(1./60.);
   Device d;std::string error;if(!d.Init(nullptr,384,256,&error)){std::printf("init: %s\n",error.c_str());return false;}
   d.SetHdr(true);d.SetShadows(true);d.SetSkyTime(1000);d.SetPixelLighting(true);
   if(limited){
    d.m_props.limits.maxStorageBufferRange=128u<<20;d.m_props.limits.minStorageBufferOffsetAlignment=256;d.m_props.limits.minUniformBufferOffsetAlignment=256;d.m_props.limits.maxUniformBufferRange=16384;d.m_props.limits.maxPushConstantsSize=128;
    descriptorAuditDevice=d.m_device;describeAuditBuffer=reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(vk::vkGetDeviceProcAddr(d.m_device,"vkGetBufferMemoryRequirements"));
    descriptorViolations=0;originalPushDescriptors=vk::vkCmdPushDescriptorSetKHR;vk::vkCmdPushDescriptorSetKHR=AuditDescriptors;
   }
   if(mixed){d.SetTaa(true,.2f);Device::SmokeParams smoke;smoke.enable=true;smoke.quality=1;smoke.resolution=8;d.SetSmokeParams(smoke);}
   auto projection=detail::Identity();projection.m[0][0]=1.15f;projection.m[1][1]=1.73f;
   projection.m[2][2]=500.f/499.9f;projection.m[2][3]=1;projection.m[3][2]=-.1f*500.f/499.9f;projection.m[3][3]=0;
   struct Ground {float x,y,z,nx,ny,nz,u,v,a,b;};std::vector<Ground> ground;
   for(int z=-4;z<4;++z)for(int x=-4;x<4;++x){
     auto v=[&](int i,int j){return Ground{float(x+i)*12,0,float(z+j)*12,0,1,0,float(i),float(j),float(i),float(j)};};
     for(auto p:{v(0,0),v(0,1),v(1,0),v(1,0),v(0,1),v(1,1)})ground.push_back(p);
   }
   struct Vertex {float x,y,z,nx,ny,nz;uint32_t color;float u,v;};
   const Vertex triangle[]={{-1,0,5,0,0,-1,0xffffffff,0,1},{1,0,5,0,0,-1,0xffffffff,1,1},{0,2,5,0,0,-1,0xffffffff,.5f,0}};
   std::vector<uint32_t> pixels(128*128,0xff487f28),normal(128*128,0xff8080ff),orm(128*128,0xffffd800);
   FILE* stats=std::fopen("soak.csv","w");if(!stats)return false;
   std::fprintf(stats,"seconds,zones,frames,private_mib,working_mib,handles,textures,texture_bytes,dead_textures,grass_pool_blades,grass_tiles,material_owners,render_targets,render_target_bytes,cpp_live_bytes,cpp_live_allocations,caster_cache,cover_meshes,caster_streaks,terrain_fits,terrain_bases,mesh_info,grass_covers,grass_chunks,plant_meshes,samplers,static_snapshots,vma_blocks,vma_allocations,vma_block_bytes,vma_live_bytes\n");
   const ULONGLONG started=GetTickCount64();unsigned zone=0;uint64_t frames=0;uint32_t expectedTextures=0;uint64_t expectedBytes=0;bool okay=true;
   while(double(GetTickCount64()-started)/1000.<duration&&!std::filesystem::exists("STOP")){
     const float x=(zone%2)*1000.f;d.SetSkyPlayfield(650+zone%2);d.SetGrassField(true,80,12,.3f,true);
     if(mixed){
       static constexpr uint32_t sizes[][2]={{384,256},{640,360},{320,240},{800,600}};
       if(!d.Resize(sizes[zone%4][0],sizes[zone%4][1])){okay=false;break;}
       d.SetHdr(zone%6!=5);d.SetRenderScale(zone%3==0?50:zone%3==1?75:100);
       Device::WeatherParams weather;weather.enable=true;weather.force=zone%4==1?1.f:0.f;weather.stormForce=zone%4==2?1.f:0.f;weather.strikeRate=60;d.SetWeatherParams(weather);
       d.SetSkyTime(double(zone%64)*400.);
     }
     std::vector<Texture*> textures;
     for(unsigned t=0;t<24;++t){
       auto* base=d.CreateTexture(128,128,pixels.data());auto* n=d.CreateTexture(128,128,normal.data());auto* o=d.CreateTexture(128,128,orm.data());
       if(!base||!n||!o){okay=false;break;}d.SetMaterialMaps(base,n,o);textures.push_back(base);
     }
     if(!okay)break;
     auto world=detail::Identity();world.m[3][0]=x;
     for(unsigned f=0;f<framesPerZone;++f){
       if(f==framesPerZone/2){const uint32_t patch=0xff646464;d.UpdateTexture(textures[0],0,16,16,1,1,&patch,4);}
       d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff607090,1);
       d.SetTransform(World,world);d.SetTransform(d3d::View,View(x));d.SetTransform(Projection,projection);
       Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={.5f,-.7f,.4f};sun.diffuse={1,1,1,1};d.SetLight(0,sun);d.LightEnable(0,true);
       d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_CULLMODE,CULL_NONE);d.SetRenderState(RS_ALPHABLENDENABLE,0);
       d.SetRenderState(RS_LIGHTING,0);d.SetRenderState(RS_AMBIENT,0xff808080);
       d.SetTexture(0,textures[0]);d.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);d.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);
       d.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);d.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
       d.SetDrawVisual(uint32_t(VisualKind::Terrain),"AnarchyGround_t");
       d.DrawPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(2<<8),ground.data(),uint32_t(ground.size()));
       d.SetDrawVisual(uint32_t(VisualKind::Static),"soak dynamic material");d.SetRenderState(RS_LIGHTING,1);
       for(auto* t:textures){d.SetTexture(0,t);d.DrawPrimitive(TriangleList,0x152,triangle,3);}
       d.SetDrawVisual(0,nullptr);
       if(mixed&&zone%4==3){for(unsigned i=0;i<2;++i){Device::SmokeSource source;source.pos[0]=x+float(i)*6;source.pos[2]=4;source.size=6;d.SmokeSourceFrame(i+1,source);}}
       d.EndFrame();++frames;Sleep(4);if(d.m_deviceLost){okay=false;break;}
     }
     d.SetTexture(0,nullptr);d.SetGrassField(false,80,12,.3f,true);
     for(auto* t:textures)d.DestroyTexture(t);
     for(unsigned f=0;f<8;++f){d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff607090,1);d.EndFrame();++frames;}
     d.WaitIdle();++zone;
     if(zone==4){expectedTextures=d.m_textureCount[0];expectedBytes=d.m_textureBytes[0];}
     if(zone>4&&(d.m_textureCount[0]!=expectedTextures||d.m_textureBytes[0]!=expectedBytes||!d.m_deadTextures.empty()||!d.m_materialOwners.empty())){
       std::printf("FAIL: residual resources after zone %u (%u textures, %llu bytes, %zu deferred, %zu owners)\n",zone,d.m_textureCount[0],(unsigned long long)d.m_textureBytes[0],d.m_deadTextures.size(),d.m_materialOwners.size());okay=false;
     }
     PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));DWORD handles=0;GetProcessHandleCount(GetCurrentProcess(),&handles);
     const double seconds=double(GetTickCount64()-started)/1000.;
     VmaTotalStatistics allocationStats{};vmaCalculateStatistics(d.m_allocator,&allocationStats);
     std::fprintf(stats,"%.3f,%u,%llu,%.3f,%.3f,%lu,%u,%llu,%zu,%u,%zu,%zu,%u,%llu,%llu,%llu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%u,%u,%llu,%llu\n",seconds,zone,(unsigned long long)frames,double(memory.PrivateUsage)/(1<<20),double(memory.WorkingSetSize)/(1<<20),handles,d.m_textureCount[0],(unsigned long long)d.m_textureBytes[0],d.m_deadTextures.size(),d.m_grassPoolBlades,d.m_grassTiles.size(),d.m_materialOwners.size(),d.m_textureCount[1],(unsigned long long)d.m_textureBytes[1],(unsigned long long)AoAuditHeapLiveBytes(),(unsigned long long)AoAuditHeapLiveCount(),d.m_casterCache.size(),d.m_coverMeshes.size(),d.m_casterStreaks.size(),d.m_terrainFits.size(),d.m_terrainBases.size(),d.m_meshInfo.size(),d.m_grassCovers.size(),d.m_grassChunks.size(),d.m_plantMeshes.size(),d.m_samplers.size(),d.m_staticGeometry.size(),allocationStats.total.statistics.blockCount,allocationStats.total.statistics.allocationCount,(unsigned long long)allocationStats.total.statistics.blockBytes,(unsigned long long)allocationStats.total.statistics.allocationBytes);std::fflush(stats);
     if(zone==10||zone%1000==0){char snapshot[64];std::snprintf(snapshot,sizeof(snapshot),"heap-zone-%u.csv",zone);AoAuditHeapSnapshot(snapshot);}
     if(zone%10==0)std::printf("soak: %.1f seconds, %u zones, %llu frames; %.1f MiB private; %lu handles; %s\n",seconds,zone,(unsigned long long)frames,double(memory.PrivateUsage)/(1<<20),handles,okay?"resources balanced":"FAILED");
     std::fflush(stdout);if(!okay||d.m_deviceLost)break;
   }
   if(limited){vk::vkCmdPushDescriptorSetKHR=originalPushDescriptors;std::printf("conservative GPU profile: %u descriptor violations\n",descriptorViolations);okay &=descriptorViolations==0;}
   AoAuditHeapSnapshot("heap-final.csv");std::fclose(stats);std::printf("SOAK %s: %u zones, %llu frames, %.1f seconds\n",okay?"PASS":"FAIL",zone,(unsigned long long)frames,double(GetTickCount64()-started)/1000.);return okay&&zone>=5;
 }
};
}
int main(int argc,char**argv){
#ifdef AOVK_TEST_HEAP_TRACE
if(argc>1&&std::strcmp(argv[1],"--heap-self-test")==0){const bool good=AoAuditHeapSelfTest();std::printf("heap allocation tracing: %s\n",good?"PASS":"FAIL");return good?0:1;}
#endif
const double seconds=argc>1?std::atof(argv[1]):60;const unsigned frames=argc>2?std::max(1,std::atoi(argv[2])):120;const bool limited=argc>3&&std::strcmp(argv[3],"--mixed-limited")==0;return rvk::StabilityAuditCheck::Run(seconds,frames,limited||(argc>3&&std::strcmp(argv[3],"--mixed")==0),limited)?0:1;}
