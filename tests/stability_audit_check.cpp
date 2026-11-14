// Inspect production device initialization using real queries and controlled capability loss.
#include "internal.h"
#include "zoning_policy.h"
#include <cstdio>
#include <cstring>
#include <unordered_set>
#include <cmath>
namespace {
PFN_vkGetPhysicalDeviceFeatures2 realFeatures;
unsigned createCalls;bool requestedBlend;bool missingBlend;bool missingStorage;
VKAPI_ATTR void VKAPI_CALL Features(VkPhysicalDevice gpu,VkPhysicalDeviceFeatures2* features){
    realFeatures(gpu,features);if(missingStorage)features->features.shaderStorageImageExtendedFormats=VK_FALSE;if(missingBlend)features->features.independentBlend=VK_FALSE;
}
VKAPI_ATTR VkResult VKAPI_CALL CaptureCreate(VkPhysicalDevice,const VkDeviceCreateInfo* ci,const VkAllocationCallbacks*,VkDevice*){
    ++createCalls;
    const auto* features=static_cast<const VkPhysicalDeviceFeatures2*>(ci->pNext);
    requestedBlend=features&&features->features.independentBlend;
    return VK_ERROR_INITIALIZATION_FAILED; // stop before creating resources; no device/driver state is fabricated
}
PFN_vkCreateShaderModule realShaderCreate;PFN_vkDestroyShaderModule realShaderDestroy;
unsigned shaderCalls,shaderFailure;std::unordered_set<VkShaderModule> liveShaders;
VKAPI_ATTR VkResult VKAPI_CALL CreateShader(VkDevice d,const VkShaderModuleCreateInfo* ci,const VkAllocationCallbacks* a,VkShaderModule* out){
 if(++shaderCalls==shaderFailure){*out=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_HOST_MEMORY;}
 auto result=realShaderCreate(d,ci,a,out);if(result==VK_SUCCESS)liveShaders.insert(*out);return result;
}
VKAPI_ATTR void VKAPI_CALL DestroyShader(VkDevice d,VkShaderModule shader,const VkAllocationCallbacks* a){liveShaders.erase(shader);realShaderDestroy(d,shader,a);}

PFN_vkDestroyInstance realInstanceDestroy;PFN_vkDestroySurfaceKHR realSurfaceDestroy;
unsigned destroyedInstances,destroyedSurfaces;
VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance instance,const VkAllocationCallbacks* a){++destroyedInstances;realInstanceDestroy(instance,a);}
VKAPI_ATTR void VKAPI_CALL DestroySurface(VkInstance instance,VkSurfaceKHR surface,const VkAllocationCallbacks* a){++destroyedSurfaces;realSurfaceDestroy(instance,surface,a);}

PFN_vkCmdPushDescriptorSetKHR realPushDescriptors;
VkBuffer watchedGrassPool;VkDeviceSize grassPoolBytes,grassRangeLimit;unsigned grassRangeViolations;
VKAPI_ATTR void VKAPI_CALL CheckGrassDescriptors(VkCommandBuffer cmd,VkPipelineBindPoint point,VkPipelineLayout layout,uint32_t set,uint32_t count,const VkWriteDescriptorSet* writes){
 for(uint32_t i=0;i<count;++i)if(writes[i].descriptorType==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER&&writes[i].pBufferInfo)
  for(uint32_t j=0;j<writes[i].descriptorCount;++j){const auto& b=writes[i].pBufferInfo[j];
   if(b.buffer==watchedGrassPool){const auto range=b.range==VK_WHOLE_SIZE?grassPoolBytes-b.offset:b.range;if(range>grassRangeLimit)++grassRangeViolations;}}
 realPushDescriptors(cmd,point,layout,set,count,writes);
}
PFN_vkGetInstanceProcAddr realInstanceProc;
PFN_vkGetDeviceProcAddr realDeviceProc;
PFN_vkGetPhysicalDeviceMemoryProperties realMemoryProps;
PFN_vkGetPhysicalDeviceMemoryProperties2 realMemoryProps2;
PFN_vkFlushMappedMemoryRanges realFlushMemory;
struct WatchedHostRange {VkDeviceMemory memory;VkDeviceSize offset,size;bool flushed;};
WatchedHostRange watchedHost[2]{};
unsigned hostFlushCalls=0;
void Noncoherent(VkPhysicalDeviceMemoryProperties& props){for(uint32_t i=0;i<props.memoryTypeCount;++i)props.memoryTypes[i].propertyFlags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;}
VKAPI_ATTR void VKAPI_CALL MemoryProps(VkPhysicalDevice gpu,VkPhysicalDeviceMemoryProperties* props){realMemoryProps(gpu,props);Noncoherent(*props);}
VKAPI_ATTR void VKAPI_CALL MemoryProps2(VkPhysicalDevice gpu,VkPhysicalDeviceMemoryProperties2* props){realMemoryProps2(gpu,props);Noncoherent(props->memoryProperties);}
VKAPI_ATTR VkResult VKAPI_CALL FlushMemory(VkDevice device,uint32_t count,const VkMappedMemoryRange* ranges){
 ++hostFlushCalls;
 for(uint32_t i=0;i<count;++i)for(auto& expected:watchedHost)if(ranges[i].memory==expected.memory&&ranges[i].offset<=expected.offset&&
   (ranges[i].size==VK_WHOLE_SIZE||ranges[i].offset+ranges[i].size>=expected.offset+expected.size))expected.flushed=true;
 return realFlushMemory(device,count,ranges);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MemoryInstanceProc(VkInstance instance,const char* name){
 const auto fn=realInstanceProc(instance,name);
 if(std::strcmp(name,"vkGetPhysicalDeviceMemoryProperties")==0){realMemoryProps=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(fn);return reinterpret_cast<PFN_vkVoidFunction>(MemoryProps);}
 if(std::strcmp(name,"vkGetPhysicalDeviceMemoryProperties2")==0||std::strcmp(name,"vkGetPhysicalDeviceMemoryProperties2KHR")==0){realMemoryProps2=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(fn);return reinterpret_cast<PFN_vkVoidFunction>(MemoryProps2);}
 return fn;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL MemoryDeviceProc(VkDevice device,const char* name){
 const auto fn=realDeviceProc(device,name);if(std::strcmp(name,"vkFlushMappedMemoryRanges")==0){realFlushMemory=reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(fn);return reinterpret_cast<PFN_vkVoidFunction>(FlushMemory);}return fn;
}
PFN_vkCreatePipelineLayout realLayout;
unsigned pushLimit=128,pushViolations=0;
VKAPI_ATTR VkResult VKAPI_CALL LimitPushRange(VkDevice device,const VkPipelineLayoutCreateInfo* ci,const VkAllocationCallbacks* allocator,VkPipelineLayout* out){
 for(uint32_t i=0;i<ci->pushConstantRangeCount;++i)if(ci->pPushConstantRanges[i].offset+ci->pPushConstantRanges[i].size>pushLimit){++pushViolations;*out=VK_NULL_HANDLE;return VK_ERROR_FEATURE_NOT_PRESENT;}
 return realLayout(device,ci,allocator,out);
}
PFN_vkCreateImageView realImageView;
unsigned viewCalls,failViewAt;
VKAPI_ATTR VkResult VKAPI_CALL FailImageView(VkDevice d,const VkImageViewCreateInfo* ci,const VkAllocationCallbacks* a,VkImageView* out){
 if(++viewCalls==failViewAt){*out=VK_NULL_HANDLE;return VK_ERROR_OUT_OF_DEVICE_MEMORY;}return realImageView(d,ci,a,out);
}
VkPhysicalDevice realGpu;
const auto discreteGpu=reinterpret_cast<VkPhysicalDevice>(uintptr_t(0x101));
const auto integratedGpu=reinterpret_cast<VkPhysicalDevice>(uintptr_t(0x102));
const auto softwareGpu=reinterpret_cast<VkPhysicalDevice>(uintptr_t(0x103));
PFN_vkGetPhysicalDeviceProperties realProps;
PFN_vkGetPhysicalDeviceProperties2 realProps2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties realQueues;
PFN_vkEnumerateDeviceExtensionProperties realExtensions;
PFN_vkGetPhysicalDeviceFormatProperties realFormats;
unsigned selectionProfile;
VKAPI_ATTR VkResult VKAPI_CALL EnumGpus(VkInstance,uint32_t* count,VkPhysicalDevice* out){
 if(!out){*count=3;return VK_SUCCESS;}out[0]=softwareGpu;out[1]=integratedGpu;out[2]=discreteGpu;*count=3;return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL GpuProps(VkPhysicalDevice gpu,VkPhysicalDeviceProperties* out){
 realProps(realGpu,out);out->deviceType=gpu==discreteGpu?VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:gpu==integratedGpu?VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:VK_PHYSICAL_DEVICE_TYPE_CPU;
 std::strcpy(out->deviceName,gpu==discreteGpu?"Simulated discrete candidate":gpu==integratedGpu?"Simulated integrated candidate":"Simulated software candidate");
 if(gpu==discreteGpu&&selectionProfile==2)out->limits.maxColorAttachments=4;
}
VKAPI_ATTR void VKAPI_CALL GpuProps2(VkPhysicalDevice gpu,VkPhysicalDeviceProperties2* out){realProps2(realGpu,out);GpuProps(gpu,&out->properties);}
VKAPI_ATTR void VKAPI_CALL GpuQueues(VkPhysicalDevice,uint32_t* count,VkQueueFamilyProperties* out){realQueues(realGpu,count,out);}
VKAPI_ATTR void VKAPI_CALL GpuFormats(VkPhysicalDevice,VkFormat format,VkFormatProperties* out){realFormats(realGpu,format,out);}
VKAPI_ATTR void VKAPI_CALL GpuFeatures(VkPhysicalDevice gpu,VkPhysicalDeviceFeatures2* out){realFeatures(realGpu,out);if(gpu==discreteGpu&&selectionProfile==1)out->features.independentBlend=VK_FALSE;}
VKAPI_ATTR VkResult VKAPI_CALL GpuExtensions(VkPhysicalDevice gpu,const char* layer,uint32_t* count,VkExtensionProperties* out){
 const auto result=realExtensions(realGpu,layer,count,out);
 if(out&&gpu==discreteGpu&&selectionProfile==3)for(uint32_t i=0;i<*count;++i)
  if(std::strcmp(out[i].extensionName,VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)==0)std::strcpy(out[i].extensionName,"VK_AOVK_test_missing_extension");
 return result;
}
}
namespace rvk {
struct StabilityAuditCheck {
 static bool OriginalZoning(){
  using namespace d3d;
  HWND window=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"AO-VK hidden zoning readiness fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  if(!window)return false;
  bool okay=true;
  {
   Device d;std::string error;
   if(!d.Init(window,64,64,&error)){std::printf("zoning setup: %s\n",error.c_str());DestroyWindow(window);return false;}
   d.SetHdr(false);d.SetShadows(false);
   d.m_livePeople=d.m_liveDrones=d.m_liveAir=d.m_liveWild=d.m_liveMachine=d.m_liveAtmos=0;
   auto projection=detail::Identity();projection.m[2][3]=1;projection.m[3][3]=0;
   struct Vertex {float x,y,z;uint32_t colour;};
   const Vertex triangle[]={{-.5f,-.5f,1,0xffffffff},{0,.5f,1,0xffffffff},{.5f,-.5f,1,0xffffffff}};
   auto frame=[&](bool world){
    d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff101820,1);
    d.SetTransform(World,detail::Identity());d.SetTransform(View,detail::Identity());d.SetTransform(Projection,projection);
    d.SetRenderState(RS_LIGHTING,0);d.SetRenderState(RS_CULLMODE,CULL_NONE);
    if(world)d.Draw(TriangleList,FVF_XYZ|FVF_DIFFUSE,triangle,3,nullptr,0);
    d.EndFrame();
   };
   for(unsigned transition=0;transition<3;++transition){
    const auto total=d.WorldPresents();d.ResetTemporalHistory(transition+1);
    rvk::zoning::Reveal reveal;reveal.Start(1000);reveal.Loaded(0);
    frame(false);
    const bool empty=d.FreshWorldPresents()==0&&d.WorldPresents()==total;
    okay &= empty;
    for(unsigned i=1;i<=4;++i){
     frame(true);
     const bool pass=d.TemporalEpoch()==transition+1&&d.FreshWorldPresents()==i&&d.WorldPresents()==total+i&&reveal.Ready(d.FreshWorldPresents())==(i==4);
     okay &=pass;
    }
    const bool autoDismiss=!reveal.Visible(1100,d.FreshWorldPresents());
    const bool clickDismiss=reveal.Visible(1100,d.FreshWorldPresents(),true)&&reveal.Continue(d.FreshWorldPresents())&&!reveal.Visible(1101,d.FreshWorldPresents(),true);
    okay &= autoDismiss&&clickDismiss;
    std::printf("Original transition %u: fresh %llu; empty frame ignored %u; automatic dismissal %u; continue dismissal %u\n",transition,(unsigned long long)d.FreshWorldPresents(),unsigned(empty),unsigned(autoDismiss),unsigned(clickDismiss));
   }
   d.WaitIdle();
  }
  DestroyWindow(window);return okay;
 }
 static bool HostMemoryVisibility(){
  Device d;std::string error;if(!d.CreateInstance(&error)||!d.PickDevice(&error))return false;
  realInstanceProc=vk::vkGetInstanceProcAddr;realDeviceProc=vk::vkGetDeviceProcAddr;
  vk::vkGetInstanceProcAddr=MemoryInstanceProc;vk::vkGetDeviceProcAddr=MemoryDeviceProc;
  const bool created=d.CreateLogicalDevice(&error);vk::vkGetInstanceProcAddr=realInstanceProc;vk::vkGetDeviceProcAddr=realDeviceProc;
  if(!created||!d.CreateFrames(&error)){std::printf("memory profile setup: %s\n",error.c_str());return false;}
  auto& frame=d.m_frames[d.m_frameIndex];VmaAllocationInfo ring{},post{};vmaGetAllocationInfo(d.m_allocator,frame.ringAllocation,&ring);vmaGetAllocationInfo(d.m_allocator,frame.postAllocation,&post);
  watchedHost[0]={ring.deviceMemory,ring.offset,16,false};watchedHost[1]={post.deviceMemory,post.offset,sizeof(detail::FrameLights),false};hostFlushCalls=0;
  std::memset(frame.ringData,0x5a,16);frame.ringOffset=16;std::memset(frame.postData,0,sizeof(detail::FrameLights));
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};const auto result=d.SubmitGpuWork(1,&submit,VK_NULL_HANDLE);d.WaitIdle();
  const bool okay=result==VK_SUCCESS&&watchedHost[0].flushed&&watchedHost[1].flushed;
  std::printf("noncoherent host writes before submission: %s (%u flush calls; ring %u, post %u)\n",okay?"PASS":"FAIL",hostFlushCalls,watchedHost[0].flushed,watchedHost[1].flushed);
  VkDeviceSize first=0,second=0;const auto* a=d.ArenaPlace(36,12,&first,true);const auto* b=d.ArenaPlace(36,12,&second,true);
  const bool isolated=a&&b&&a==b&&second>first&&second%12==0&&second%d.m_props.limits.nonCoherentAtomSize==0;
  std::printf("noncoherent shadow slots preserve stride and atom alignment: %s (offsets %llu / %llu, atom %llu)\n",isolated?"PASS":"FAIL",(unsigned long long)first,(unsigned long long)second,(unsigned long long)d.m_props.limits.nonCoherentAtomSize);
  std::printf("frame uniform bytes %zu (within Vulkan minimum 16384)\n",sizeof(detail::FrameLights));return okay&&isolated;
 }
 static bool Push128(){
  bool okay=true;
  for(unsigned effect=0;effect<3;++effect){
   Device d;std::string error;if(!d.CreateInstance(&error)||!d.PickDevice(&error)||!d.CreateLogicalDevice(&error)){std::printf("query setup: %s\n",error.c_str());return false;}
   d.m_props.limits.maxPushConstantsSize=128;d.CreatePipelineCache();
   realLayout=vk::vkCreatePipelineLayout;pushViolations=0;vk::vkCreatePipelineLayout=LimitPushRange;
   const bool created=effect==0?d.CreateHdrResources(&error):effect==1?d.CreateCloudResources(&error):d.CreateSmokeSeen();
   vk::vkCreatePipelineLayout=realLayout;const bool pass=created&&pushViolations==0;
   std::printf("128-byte push constants / %s: %s (%u excessive layouts; %s)\n",effect==0?"post effects":effect==1?"sky":"smoke visibility",pass?"PASS":"FAIL",pushViolations,error.c_str());std::fflush(stdout);okay &=pass;
  }
  return okay;
 }
 static bool RockDecodeFailure(){
  bool okay=true;
  for(unsigned failure=0;failure<2;++failure){
   Device d;d.m_enhancedRocks=1;d.m_rock.started=true;
   std::promise<std::shared_ptr<Device::RockMaps>> result;
   d.m_rock.pending=result.get_future();
   if(failure==0)result.set_exception(std::make_exception_ptr(std::bad_alloc{}));
   else if(failure==1)result.set_exception(std::make_exception_ptr(std::runtime_error("controlled decode failure")));
   else result.set_value(nullptr);
   bool retained=true;
   try { d.PollTerrainRock(); } catch(const std::exception& e){retained=false;std::printf("escaped worker exception: %s\n",e.what());}
   const bool pass=retained&&!d.m_rock.albedo&&!d.m_rock.normal&&!d.m_rock.orm;
   std::printf("rock decode failure %u keeps original materials: %s\n",failure,pass?"PASS":"FAIL");okay &=pass;
   // The unfixed null-result branch dereferences the null result; test it last.
  }
  return okay;
 }
 static bool SkinStorage(){
  Device d;std::string error;if(!d.Init(nullptr,192,128,&error))return false;
  d.BeginFrame();auto& frame=d.m_frames[d.m_frameIndex];
  if(frame.skinArena)vmaDestroyBuffer(d.m_allocator,frame.skinArena,frame.skinArenaAllocation);
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=192u<<20;info.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  VmaAllocationCreateInfo allocation{};allocation.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  if(vmaCreateBuffer(d.m_allocator,&info,&allocation,&frame.skinArena,&frame.skinArenaAllocation,nullptr)!=VK_SUCCESS)return false;
  frame.skinArenaSize=info.size;frame.skinArenaOffset=144u<<20;
  watchedGrassPool=frame.skinArena;grassPoolBytes=frame.skinArenaSize;grassRangeLimit=128u<<20;grassRangeViolations=0;
  realPushDescriptors=vk::vkCmdPushDescriptorSetKHR;vk::vkCmdPushDescriptorSetKHR=CheckGrassDescriptors;
  auto source=std::make_shared<skin::Source>();
  constexpr uint32_t count=96;
  for(uint32_t i=0;i<count;++i){skin::TriVertex v{};v.bind[0]=v.posA[0]=float(i);v.bind[1]=v.posA[1]=float(i%3);v.normal[2]=1;v.weightA=1;source->vertices.push_back(v);source->indices.push_back(uint16_t(i));}source->Finish();
  skin::Bone bone{};bone.m[0]=bone.m[4]=bone.m[8]=1;bone.m[9]=2;
  auto bones=std::make_shared<skin::Palette>();bones->Set(&bone,1);bone.m[9]=1;auto previous=std::make_shared<skin::Palette>();previous->Set(&bone,1);
  skin::Job job;job.source=source;job.bones=bones;job.prevBones=previous;job.ComputeBounds();
  auto* output=d.SkinOnGpu(job);if(!output||!d.SkinSmoothOnGpu(job,*output)){vk::vkCmdPushDescriptorSetKHR=realPushDescriptors;return false;}
  vk::vkCmdPushDescriptorSetKHR=realPushDescriptors;
  VkBuffer readback=VK_NULL_HANDLE;VmaAllocation readbackAllocation=nullptr;VmaAllocationInfo mapped{};
  info.size=count*56;info.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;allocation.usage=VMA_MEMORY_USAGE_AUTO;allocation.flags=VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT|VMA_ALLOCATION_CREATE_MAPPED_BIT;
  if(vmaCreateBuffer(d.m_allocator,&info,&allocation,&readback,&readbackAllocation,&mapped)!=VK_SUCCESS)return false;
  d.EndRendering();VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};barrier.srcStageMask=VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;barrier.srcAccessMask=VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;barrier.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&barrier;vk::vkCmdPipelineBarrier2(frame.main,&dependency);
  VkBufferCopy regions[]={{output->vertexOffset,0,count*32},{output->prevOffset,count*32,count*12},{output->smoothOffset,count*44,count*12}};
  vk::vkCmdCopyBuffer(frame.main,frame.skinArena,readback,3,regions);d.EndFrame();d.WaitIdle();vmaInvalidateAllocation(d.m_allocator,readbackAllocation,0,VK_WHOLE_SIZE);
  std::vector<skin::Vertex> expected(count);skin::SkinVertices(source->vertices.data(),count,expected.data(),*bones,false,nullptr,nullptr);
  auto* actual=static_cast<const float*>(mapped.pMappedData);float largest=0;
  const auto check=[&](float a,float b){largest=std::max(largest,std::abs(a-b));};
  for(uint32_t i=0;i<count;++i){for(uint32_t k=0;k<8;++k)check(actual[i*8+k],reinterpret_cast<const float*>(&expected[i])[k]);check(actual[count*8+i*3],float(i)+1);check(actual[count*8+i*3+1],float(i%3));check(actual[count*8+i*3+2],0);check(actual[count*11+i*3],0);check(actual[count*11+i*3+1],0);check(actual[count*11+i*3+2],1);}
  vmaDestroyBuffer(d.m_allocator,readback,readbackAllocation);
  std::printf("skin 192 MiB arena / 128 MiB range: %u oversized bindings; largest GPU/CPU difference %.8g\n",grassRangeViolations,largest);
  return grassRangeViolations==0&&largest<1e-5f;
 }
 static bool GrassStorage(bool limited){
  using namespace d3d;Device::SetFixedClock(1./60.);Device d;std::string error;
  if(!d.Init(nullptr,384,256,&error))return false;
  d.SetHdr(true);d.SetShadows(true);d.SetSkyTime(1000);d.SetSkyPlayfield(650);d.SetGrassField(true,80,5,.3f,true);
  if(!d.GrowGrassPool(1u<<22))return false;
  VmaVirtualAllocationCreateInfo reservation{};reservation.size=3u<<20;VmaVirtualAllocation reserved;VkDeviceSize offset;
  if(vmaVirtualAllocate(d.m_grassPoolBlock,&reservation,&reserved,&offset)!=VK_SUCCESS)return false;
  watchedGrassPool=d.m_grassPool;grassPoolBytes=VkDeviceSize(d.m_grassPoolBlades)*48;
  grassRangeLimit=limited?(128u<<20):d.m_props.limits.maxStorageBufferRange;
  d.m_props.limits.maxStorageBufferRange=uint32_t(grassRangeLimit);grassRangeViolations=0;
  realPushDescriptors=vk::vkCmdPushDescriptorSetKHR;vk::vkCmdPushDescriptorSetKHR=CheckGrassDescriptors;
  struct Ground {float x,y,z,nx,ny,nz,u,v,a,b;};std::vector<Ground> ground;
  for(int z=-4;z<4;++z)for(int x=-4;x<4;++x){auto v=[&](int i,int j){return Ground{float(x+i)*12,0,float(z+j)*12,0,1,0,float(i),float(j),float(i),float(j)};};for(auto p:{v(0,0),v(0,1),v(1,0),v(1,0),v(0,1),v(1,1)})ground.push_back(p);}
  std::vector<uint32_t> pixels(128*128,0xff487f28);auto* texture=d.CreateTexture(128,128,pixels.data());
  auto projection=detail::Identity();projection.m[0][0]=1.15f;projection.m[1][1]=1.73f;projection.m[2][2]=500.f/499.9f;projection.m[2][3]=1;projection.m[3][2]=-.1f*500.f/499.9f;projection.m[3][3]=0;
  const float z=1.f/std::sqrt(1.0625f),y=-.25f*z;auto view=detail::Identity();view.m[1][1]=z;view.m[1][2]=y;view.m[2][1]=-y;view.m[2][2]=z;view.m[3][1]=-8*z-16*y;view.m[3][2]=-8*y+16*z;
  for(unsigned f=0;f<120;++f){
   if(f==119)d.RequestScreenshot(limited?"grass-limited.bmp":"grass-reference.bmp");
   d.BeginFrame();d.Clear(CLEAR_TARGET|CLEAR_ZBUFFER,0xff607090,1);d.SetTransform(World,detail::Identity());d.SetTransform(d3d::View,view);d.SetTransform(Projection,projection);
   Light sun{};sun.type=LIGHT_DIRECTIONAL;sun.direction={.5f,-.7f,.4f};sun.diffuse={1,1,1,1};d.SetLight(0,sun);d.LightEnable(0,true);
   d.SetRenderState(RS_ZENABLE,1);d.SetRenderState(RS_ZWRITEENABLE,1);d.SetRenderState(RS_CULLMODE,CULL_NONE);d.SetRenderState(RS_ALPHABLENDENABLE,0);d.SetRenderState(RS_LIGHTING,0);d.SetRenderState(RS_AMBIENT,0xff808080);
   d.SetTexture(0,texture);d.SetTextureStageState(0,TSS_TEXCOORDINDEX,0);d.SetTextureStageState(0,TSS_COLOROP,TOP_MODULATE);d.SetTextureStageState(0,TSS_COLORARG1,TA_TEXTURE);d.SetTextureStageState(0,TSS_COLORARG2,TA_DIFFUSE);
   d.SetDrawVisual(uint32_t(VisualKind::Terrain),"AnarchyGround_t");d.DrawPrimitive(TriangleList,FVF_XYZ|FVF_NORMAL|(2<<8),ground.data(),uint32_t(ground.size()));d.SetDrawVisual(0,nullptr);d.EndFrame();
  }
  vk::vkCmdPushDescriptorSetKHR=realPushDescriptors;
  std::printf("grass %s: %u descriptor range violations, %llu blades, %llu-byte pool, %llu-byte limit\n",limited?"128 MiB limit":"reference",grassRangeViolations,(unsigned long long)d.m_grassBlades,(unsigned long long)grassPoolBytes,(unsigned long long)grassRangeLimit);
  d.SetTexture(0,nullptr);d.DestroyTexture(texture);return grassRangeViolations==0&&d.m_grassBlades>0;
 }
 static bool TargetFailure(){
  Device d;std::string error;if(!d.Init(nullptr,192,128,&error)){std::printf("target init: %s\n",error.c_str());return false;}
  bool okay=true;
  for(unsigned fail=1;fail<=7;++fail){
   const auto oldMain=d.m_main;const auto oldTarget=d.m_target;const auto oldScene=d.m_scene;const auto oldDepth=d.m_depth;
   realImageView=vk::vkCreateImageView;viewCalls=0;failViewAt=fail;vk::vkCreateImageView=FailImageView;
   const bool result=d.Resize(256,160);vk::vkCreateImageView=realImageView;
   const bool pass=!result&&d.m_main==oldMain&&d.m_target==oldTarget&&d.m_scene==oldScene&&d.m_depth==oldDepth&&d.m_width==192&&d.m_height==128;
   std::printf("resize view failure %u: %s (returned %u)\n",fail,pass?"PASS":"FAIL",unsigned(result));okay &= pass;
   if(!pass)return false; // old implementation cannot safely render after its failed resize
   d.BeginFrame();d.Clear(d3d::CLEAR_TARGET|d3d::CLEAR_ZBUFFER,0xff506070,1);d.EndFrame();
  }
  const bool changed=d.Resize(256,160)&&d.m_width==256&&d.m_scene&&d.m_depthView;
  std::printf("resize after failures: %s\n",changed?"PASS":"FAIL");
  if(!changed)return false;
  d.SetHdr(true);d.SetTaa(true,0);d.SetRenderScale(50);
  const auto oldMain=d.m_main;const auto oldTarget=d.m_target;
  realImageView=vk::vkCreateImageView;viewCalls=0;failViewAt=3;vk::vkCreateImageView=FailImageView;
  d.ApplyRenderScale();vk::vkCreateImageView=realImageView;
  const bool scaleRetained=d.m_renderScale==100&&d.m_main==oldMain&&d.m_target==oldTarget;
  std::printf("render scale retains targets on failure: %s\n",scaleRetained?"PASS":"FAIL");
  // A subsequent request may retry after the allocation backoff.
  d.m_frameNumber+=301;d.ApplyRenderScale();
  const bool scaleChanged=d.m_renderScale==50&&d.m_scene&&d.m_scene->Width()==128;
  std::printf("render scale recovers after failure: %s\n",scaleChanged?"PASS":"FAIL");
  auto* external=d.CreateRenderTarget(32,32);d.m_target=external;d.SetRenderScale(75);d.ApplyRenderScale();
  const bool externalRetained=d.m_target==external&&d.m_renderScale==75;d.m_target=d.m_main;d.DestroyTexture(external);
  std::printf("external target retained on scale change: %s\n",externalRetained?"PASS":"FAIL");
  return okay&&changed&&scaleRetained&&scaleChanged&&externalRetained;
 }
 static bool DepthFailure(){
  Device d;std::string error;if(!d.CreateInstance(&error)||!d.PickDevice(&error)||!d.CreateLogicalDevice(&error))return false;
  realImageView=vk::vkCreateImageView;viewCalls=0;failViewAt=1;vk::vkCreateImageView=FailImageView;
  const bool first=d.EnsureDepth(32,32);vk::vkCreateImageView=realImageView;
  bool okay=!first&&!d.m_depth&&!d.m_depthView;
  std::printf("empty depth image-view failure: %s (returned %u, image %u, view %u)\n",okay?"PASS":"FAIL",unsigned(first),unsigned(d.m_depth!=VK_NULL_HANDLE),unsigned(d.m_depthView!=VK_NULL_HANDLE));
  // Repair the old implementation's invalid partial depth before continuing.
  if(d.m_depth&&!d.m_depthView){vmaDestroyImage(d.m_allocator,d.m_depth,d.m_depthAllocation);d.m_depth=VK_NULL_HANDLE;d.m_depthAllocation=nullptr;d.m_depthWidth=d.m_depthHeight=0;}
  if(!d.EnsureDepth(32,32))return false;const auto oldImage=d.m_depth;const auto oldView=d.m_depthView;
  viewCalls=0;failViewAt=1;vk::vkCreateImageView=FailImageView;const bool grown=d.EnsureDepth(64,64);vk::vkCreateImageView=realImageView;
  const bool retained=!grown&&d.m_depth==oldImage&&d.m_depthView==oldView;okay &= retained;
  std::printf("existing depth survives failed growth: %s (returned %u)\n",retained?"PASS":"FAIL",unsigned(grown));
  return okay;
 }
 static bool StorageFeatureLoss(){
  Device d;std::string error;if(!d.CreateInstance(&error)||!d.PickDevice(&error))return false;
  realFeatures=vk::vkGetPhysicalDeviceFeatures2;missingStorage=true;vk::vkGetPhysicalDeviceFeatures2=Features;
  const bool created=d.CreateLogicalDevice(&error);vk::vkGetPhysicalDeviceFeatures2=realFeatures;missingStorage=false;
  if(!created){std::printf("device setup: %s\n",error.c_str());return false;}
  const bool smoke=d.CreateSmokeResources(&error);
  std::printf("extended storage unavailable: smoke creation %u; %s\n",unsigned(smoke),error.c_str());
  return smoke; // Feature bit adds a format guarantee; these actual formats remain supported.
 }
 static bool Limits(){
  Device d;std::string error;if(!d.CreateInstance(&error)||!d.PickDevice(&error))return false;
  std::printf("GPU storage range %u; uniform range %u; push descriptors/scene images %u; storage alignment %llu\n",d.m_props.limits.maxStorageBufferRange,d.m_props.limits.maxUniformBufferRange,Device::kMaxBindlessImages,(unsigned long long)d.m_props.limits.minStorageBufferOffsetAlignment);
  return true;
 }
 static bool DeviceSelection(){
  bool okay=true;
  for(selectionProfile=0;selectionProfile<4;++selectionProfile){
   Device d;std::string error;
   if(!d.CreateInstance(&error)||!d.PickDevice(&error)){std::printf("selection setup: %s\n",error.c_str());return false;}
   realGpu=d.m_physical;realProps=vk::vkGetPhysicalDeviceProperties;realProps2=vk::vkGetPhysicalDeviceProperties2;
   realQueues=vk::vkGetPhysicalDeviceQueueFamilyProperties;realExtensions=vk::vkEnumerateDeviceExtensionProperties;
   realFormats=vk::vkGetPhysicalDeviceFormatProperties;realFeatures=vk::vkGetPhysicalDeviceFeatures2;
   const auto realEnum=vk::vkEnumeratePhysicalDevices;
   vk::vkEnumeratePhysicalDevices=EnumGpus;vk::vkGetPhysicalDeviceProperties=GpuProps;vk::vkGetPhysicalDeviceProperties2=GpuProps2;
   vk::vkGetPhysicalDeviceQueueFamilyProperties=GpuQueues;vk::vkEnumerateDeviceExtensionProperties=GpuExtensions;
   vk::vkGetPhysicalDeviceFormatProperties=GpuFormats;vk::vkGetPhysicalDeviceFeatures2=GpuFeatures;
   const bool selected=d.PickDevice(&error);const bool pass=selected&&d.m_physical==(selectionProfile==0?discreteGpu:integratedGpu);
   std::printf("hybrid selection profile %u: %s (%s)\n",selectionProfile,pass?"PASS":"FAIL",d.m_info.gpu.c_str());okay &= pass;
   vk::vkEnumeratePhysicalDevices=realEnum;vk::vkGetPhysicalDeviceProperties=realProps;vk::vkGetPhysicalDeviceProperties2=realProps2;
   vk::vkGetPhysicalDeviceQueueFamilyProperties=realQueues;vk::vkEnumerateDeviceExtensionProperties=realExtensions;
   vk::vkGetPhysicalDeviceFormatProperties=realFormats;vk::vkGetPhysicalDeviceFeatures2=realFeatures;
   d.m_physical=realGpu;
  }
  return okay;
 }
 static bool CacheRoundTrip(){
   Device d;std::string error;
   if(!d.CreateInstance(&error)||!d.PickDevice(&error)||!d.CreateLogicalDevice(&error)){std::printf("query setup: %s\n",error.c_str());return false;}
   d.CreatePipelineCache();d.SavePipelineCache("isolated path roundtrip");
   std::printf("cache persisted bytes %zu; ACP %u; path %s\n",d.m_pipelineCacheSaved,GetACP(),d.PipelineCacheFilePath().c_str());
   return d.m_pipelineCacheSaved>0;
 }
 static bool PartialInit(){
   bool okay=true;
   for(bool surface:{false,true}){
     HWND window=surface?CreateWindowExW(0,L"STATIC",L"AO-VK isolated init fixture",WS_POPUP,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr):nullptr;
     if(surface&&!window)return false;
     VkInstance instance=VK_NULL_HANDLE;VkSurfaceKHR savedSurface=VK_NULL_HANDLE;
     destroyedInstances=destroyedSurfaces=0;
     {
       Device d;d.m_window=window;std::string error;
       if(!d.CreateInstance(&error)){std::printf("partial init setup: %s\n",error.c_str());if(window)DestroyWindow(window);return false;}
       instance=d.m_instance;savedSurface=d.m_surface;
       realInstanceDestroy=vk::vkDestroyInstance;realSurfaceDestroy=vk::vkDestroySurfaceKHR;
       vk::vkDestroyInstance=DestroyInstance;vk::vkDestroySurfaceKHR=DestroySurface;
       // Simulate failure before a logical GPU device was created.
     }
     vk::vkDestroyInstance=realInstanceDestroy;vk::vkDestroySurfaceKHR=realSurfaceDestroy;
     const bool pass=destroyedInstances==1&&destroyedSurfaces==unsigned(surface);
     std::printf("partial init %s: %s (%u instances, %u surfaces destroyed)\n",surface?"window surface":"instance only",pass?"PASS":"FAIL",destroyedInstances,destroyedSurfaces);
     if(!destroyedSurfaces&&savedSurface)realSurfaceDestroy(instance,savedSurface,nullptr);
     if(!destroyedInstances&&instance)realInstanceDestroy(instance,nullptr);
     if(window)DestroyWindow(window);okay &= pass;
   }
   return okay;
 }
 static bool ShaderFailures(){
    bool okay=true;
    for(bool shadow:{false,true})for(unsigned fail=1;fail<=(shadow?2u:5u);++fail){
      Device d;std::string error;
      if(!d.CreateInstance(&error)||!d.PickDevice(&error)||!d.CreateLogicalDevice(&error)){std::printf("query setup: %s\n",error.c_str());return false;}
      d.m_shadowSize=64;
      realShaderCreate=vk::vkCreateShaderModule;realShaderDestroy=vk::vkDestroyShaderModule;
      liveShaders.clear();shaderCalls=0;shaderFailure=fail;
      vk::vkCreateShaderModule=CreateShader;vk::vkDestroyShaderModule=DestroyShader;
      const bool result=shadow?d.CreateShadowResources(&error):d.CreatePipelines(&error);
      const size_t leaked=liveShaders.size();
      const bool pass=!result&&shaderCalls==fail&&leaked==0;
      std::printf("%s shader failure %u: %s (%zu shader modules left at return)\n",shadow?"shadow":"main",fail,pass?"PASS":"FAIL",leaked);
      for(auto shader:liveShaders)realShaderDestroy(d.m_device,shader,nullptr);liveShaders.clear();
      vk::vkCreateShaderModule=realShaderCreate;vk::vkDestroyShaderModule=realShaderDestroy;
      okay &= pass;
    }
    return okay;
 }
 static bool DeviceFeatures(){
    bool okay=true;
    for(unsigned profile:{0u,1u,2u}){
      Device d;std::string error;
      if(!d.CreateInstance(&error)||!d.PickDevice(&error)){std::printf("query setup: %s\n",error.c_str());return false;}
      realFeatures=vk::vkGetPhysicalDeviceFeatures2;auto realCreate=vk::vkCreateDevice;
      const bool missing=profile==1;
      if(profile==2)d.m_props.limits.maxColorAttachments=4;
      missingBlend=missing;createCalls=0;requestedBlend=false;
      vk::vkGetPhysicalDeviceFeatures2=Features;vk::vkCreateDevice=CaptureCreate;
      const bool created=d.CreateLogicalDevice(&error);
      vk::vkGetPhysicalDeviceFeatures2=realFeatures;vk::vkCreateDevice=realCreate;
      const bool pass=!created&&(missing ? createCalls==0 && error.find("independent")!=std::string::npos : profile==2 ? createCalls==0 && error.find("5 color")!=std::string::npos : createCalls==1&&requestedBlend);
      std::printf("independent attachment blending %s: %s (create calls %u, requested %u, %s)\n",missing?"unavailable":profile==2?"only four render targets":"supported",pass?"PASS":"FAIL",createCalls,unsigned(requestedBlend),error.c_str());
      okay &= pass;
    }
    return okay;
 }
};
}
int main(int argc,char**argv){SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);return (argc>1&&std::strcmp(argv[1],"--original-zoning")==0 ? rvk::StabilityAuditCheck::OriginalZoning() : argc>1&&std::strcmp(argv[1],"--host-memory")==0 ? rvk::StabilityAuditCheck::HostMemoryVisibility() : argc>1&&std::strcmp(argv[1],"--push-128")==0 ? rvk::StabilityAuditCheck::Push128() : argc>1&&std::strcmp(argv[1],"--rock-decode-failure")==0 ? rvk::StabilityAuditCheck::RockDecodeFailure() : argc>1&&std::strcmp(argv[1],"--skin-storage")==0 ? rvk::StabilityAuditCheck::SkinStorage() : argc>1&&std::strcmp(argv[1],"--grass-storage-limited")==0 ? rvk::StabilityAuditCheck::GrassStorage(true) : argc>1&&std::strcmp(argv[1],"--grass-storage-reference")==0 ? rvk::StabilityAuditCheck::GrassStorage(false) : argc>1&&std::strcmp(argv[1],"--target-failure")==0 ? rvk::StabilityAuditCheck::TargetFailure() : argc>1&&std::strcmp(argv[1],"--depth-failure")==0 ? rvk::StabilityAuditCheck::DepthFailure() : argc>1&&std::strcmp(argv[1],"--no-extended-storage")==0 ? rvk::StabilityAuditCheck::StorageFeatureLoss() : argc>1&&std::strcmp(argv[1],"--limits")==0 ? rvk::StabilityAuditCheck::Limits() : argc>1&&std::strcmp(argv[1],"--device-selection")==0 ? rvk::StabilityAuditCheck::DeviceSelection() : argc>1&&std::strcmp(argv[1],"--cache-roundtrip")==0 ? rvk::StabilityAuditCheck::CacheRoundTrip() : argc>1&&std::strcmp(argv[1],"--partial-init")==0 ? rvk::StabilityAuditCheck::PartialInit() : argc>1&&std::strcmp(argv[1],"--shader-failures")==0 ? rvk::StabilityAuditCheck::ShaderFailures():rvk::StabilityAuditCheck::DeviceFeatures())?0:1;}
