// Newland room sizing must ignore AO's legacy extreme-point bounding-box bug.
// Runs only with isolated retail DLLs, using the real VertexBuffer_c ABI.
#include "native/native.h"
#include "native/orig_api.gen.h"
#include "native/mesh_data.h"
#include "native/vc10.h"
#include <array>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <limits>
#include <algorithm>
using namespace rnative;
struct V3 {float x,y,z;};
struct FakeVb;
HRESULT __stdcall Lock(FakeVb*,DWORD,void**,DWORD*);
HRESULT __stdcall Unlock(FakeVb*);
void* table[8]={nullptr,nullptr,nullptr,reinterpret_cast<void*>(&Lock),reinterpret_cast<void*>(&Unlock)};
struct FakeVb {void** vtable=table;std::vector<V3> points;int locks=0,unlocks=0;};
HRESULT __stdcall Lock(FakeVb* v,DWORD,void** out,DWORD* size){++v->locks;*out=v->points.data();if(size)*size=DWORD(v->points.size()*12);return 0;}
HRESULT __stdcall Unlock(FakeVb* v){++v->unlocks;return 0;}
struct Impl {FakeVb* vb;uint32_t fvf=2,flags=0,memory=2,bytes=0,stride=12,count=0;};
template<class T>T& At(void* p,size_t n){return *reinterpret_cast<T*>(static_cast<unsigned char*>(p)+n);}
struct Part {
 FakeVb vb;Impl impl{&vb};Impl* wrapper=&impl;
 std::array<unsigned char,0x4C> tri{};std::array<unsigned char,0x24> mesh{};
 Part(std::initializer_list<V3> p):vb{table,p}{
  impl.bytes=uint32_t(vb.points.size()*12);impl.count=uint32_t(vb.points.size());
  At<uint32_t>(tri.data(),0x34)=impl.count;At<uint32_t>(tri.data(),0x40)=2;
  At<void*>(tri.data(),0x48)=&wrapper;At<void*>(mesh.data(),0x20)=tri.data();
 }
};
struct Data {
 std::array<unsigned char,0x70> bytes{};std::vector<void*> parts;
 Data(std::initializer_list<Part*> p){
  for(auto v:p)parts.push_back(v->mesh.data());
  auto& a=At<vc10::Vector<void*>>(bytes.data(),0x4C);
  a.first=parts.data();a.last=a.end=a.first+parts.size();a.allocator=0;
 }
};
void LogTest(const char* format,...){va_list a;va_start(a,format);std::vprintf(format,a);va_end(a);std::puts("");}
int main(int argc,char** argv){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 HMODULE original=LoadLibraryExA(argc>1?argv[1]:"randy31.dll",nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
 if(!original){std::printf("cannot load isolated stock DLL: %lu\n",GetLastError());return 2;}
 SetLog(LogTest);SetOriginal(original);if(!orig::Init())return 2;
 meshdata::Install(original);
 // Min-X is deliberately at a high Y, and min-Y at Z=0. AO's historical
 // extrema produce a flattened Z box. The expected true box is known exactly.
 Part a{{-4,4,0},{4,0,0},{0,-3,-6},{1,6,3}};
 Part b{{-8,2,-1},{10,3,5}};
 Data data{&a,&b};float actual[6]{};int failures=0;
 auto check=[&](bool ok,const char* name){std::printf("%s: %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;};
 using Legacy=void(__cdecl*)(const float*,uint32_t,float*,float*,float*,float*);
 auto legacy=reinterpret_cast<Legacy>(reinterpret_cast<unsigned char*>(original)+0x29B7E);
 float center[3],radius,lo[3],hi[3];legacy(&a.vb.points[0].x,uint32_t(a.vb.points.size()),center,&radius,lo,hi);
 check(lo[1]!=-3 || lo[2]!=-6,"fixture reproduces the original extreme-point bug");
 const float expected[6]={-8,-3,-6,10,6,5};
 check(meshdata::WindowBounds(data.bytes.data(),actual)&&std::equal(actual,actual+6,expected),"true bounds include every material part and all XYZ extrema");
 std::printf("true bounds %.1f %.1f %.1f / %.1f %.1f %.1f\n",actual[0],actual[1],actual[2],actual[3],actual[4],actual[5]);
 check(a.vb.locks==1&&a.vb.unlocks==1&&b.vb.locks==1&&b.vb.unlocks==1,"one balanced read-only vertex access per part");
 check(meshdata::WindowBounds(data.bytes.data(),actual)&&a.vb.locks==1&&b.vb.locks==1,"same data reuses copied bounds without relocking vertices");
 Data empty{};check(!meshdata::WindowBounds(empty.bytes.data(),actual),"empty resource fails closed");
 Part flat{{-3,0,2},{3,0,2},{3,4,2}};Data facade{&flat};
 check(meshdata::WindowBounds(facade.bytes.data(),actual)&&actual[2]==2&&actual[5]==2,"planar facade retains exact true bounds for reviewed global glazing");
 Part line{{0,0,0},{1,0,0}};Data invalidLine{&line};
 check(!meshdata::WindowBounds(invalidLine.bytes.data(),actual),"line resource cannot become a room");
 Part bad{{0,0,0},{1,1,1},{2,std::numeric_limits<float>::quiet_NaN(),3}};Data invalid{&bad};
 check(!meshdata::WindowBounds(invalid.bytes.data(),actual)&&bad.vb.locks==bad.vb.unlocks,"nonfinite position fails closed and unlocks");
 Part unsupported{{0,0,0},{1,1,1}};At<uint32_t>(unsupported.tri.data(),0x40)=4;Data transformed{&unsupported};
 check(!meshdata::WindowBounds(transformed.bytes.data(),actual)&&unsupported.vb.locks==0,"pretransformed position format is refused");
 std::printf("%s (%d failures)\n",failures?"FAILED":"all passed",failures);return failures?1:0;
}
