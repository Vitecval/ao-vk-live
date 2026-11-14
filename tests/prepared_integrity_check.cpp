// Read-only validation of prepared files using the published cache format.
#include "material_cache.h"
#include "material_cache_legacy.h"
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
using namespace rvk::maps;
namespace {
bool ValidateLegacy(std::ifstream& in,std::streamoff length,const std::filesystem::path& file,uint64_t& decoded,std::string& why){
 legacy::Header h{};in.seekg(0);size_t parsed=0;uint64_t key=0;const auto stem=file.stem().string();
 try{key=std::stoull(stem,&parsed,16);}catch(...){why="filename";return false;}
 if(stem.size()!=16||parsed!=16||length<std::streamoff(sizeof(h))||!in.read(reinterpret_cast<char*>(&h),sizeof(h))||h.magic!=0x4d4b5652||h.version!=3||h.key!=key||h.bytes>legacy::Limit||uint64_t(length)!=sizeof(h)+h.bytes||h.height>1||h.alpha>1){why="legacy metadata";return false;}
 uint64_t total=0,hash=14695981039346656037ull;std::vector<uint8_t> raw;
 for(const auto& chain:h.chains){
  if((!chain.levels&&(chain.w||chain.h))||(chain.levels&&(!chain.w||!chain.h||chain.w>2048||chain.h>2048||chain.levels!=LevelCount(chain.w,chain.h)))){why="legacy mip chain";return false;}
  uint32_t w=chain.w,y=chain.h;
  for(unsigned l=0;l<chain.levels;++l){const size_t bytes=size_t(w)*y*4;raw.resize(bytes);if(!in.read(reinterpret_cast<char*>(raw.data()),bytes)){why="legacy payload";return false;}hash=legacy::Hash(raw.data(),bytes,hash);total+=bytes;w=std::max(w/2,1u);y=std::max(y/2,1u);}
 }
 if(total!=h.bytes||hash!=h.checksum||(h.height&&!h.chains[0].levels)){why="legacy checksum/totals";return false;}
 decoded=total;return true;
}
bool Validate(const std::filesystem::path& file,uint64_t& decoded,std::string& why,uint32_t& version){
 using namespace prepared;
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in){why="open";return false;}
 const auto length=in.tellg();Header h{};uint32_t prefix[2]{};
 if(length<8||length>std::streamoff(sizeof(h)+Limit)){why="file size";return false;}
 in.seekg(0);if(!in.read(reinterpret_cast<char*>(prefix),sizeof(prefix))){why="header";return false;}version=prefix[1];
 if(prefix[0]==0x4d4b5652&&version==3)return ValidateLegacy(in,length,file,decoded,why);
 if(length<std::streamoff(sizeof(h))){why="header size";return false;}
 in.seekg(0);if(!in.read(reinterpret_cast<char*>(&h),sizeof(h))){why="header";return false;}
 const auto stem=file.stem().string();size_t parsed=0;uint64_t key=0;
 try{key=std::stoull(stem,&parsed,16);}catch(...){why="filename";return false;}
 if(stem.size()!=16||parsed!=16||h.key!=key||h.magic!=0x4d4b5652||h.version!=Version||h.storedBytes>Limit||h.decodedBytes>Limit||uint64_t(length)!=sizeof(h)+h.storedBytes||h.height>1||!h.maxSize||h.maxSize>2048||!h.maxAlbedoSize||h.maxAlbedoSize>2048){why="metadata";return false;}
 const auto expected=h.checksum;h.checksum=0;auto hash=Hash(&h,sizeof(h));uint64_t stored=0,rawTotal=0;Codec codec(true);std::vector<uint8_t> raw,packed;
 for(unsigned i=0;i<6;++i){const auto& chain=h.chains[i];if(chain.levels>MaxLevels||(!chain.levels&&(chain.w||chain.h))||(chain.levels&&(!chain.w||!chain.h||chain.w>2048||chain.h>2048||chain.levels!=LevelCount(chain.w,chain.h)))){why="mip chain";return false;}
  uint32_t w=chain.w,y=chain.h;
  for(unsigned l=0;l<MaxLevels;++l){const auto encoded=h.sizes[i][l],bytes=encoded&~Compressed;
   if(l>=chain.levels){if(encoded){why="trailing level";return false;}continue;}
   const size_t count=size_t(w)*y*4;if(!bytes||bytes>count||(!(encoded&Compressed)&&bytes!=count)){why="level size";return false;}
   raw.resize(count);
   if(encoded&Compressed){packed.resize(bytes);SIZE_T written=0;if(!in.read(reinterpret_cast<char*>(packed.data()),bytes)||!codec.Open()||!Decompress(codec.handle,packed.data(),bytes,raw.data(),count,&written)||written!=count){why="decompression";return false;}}
   else if(!in.read(reinterpret_cast<char*>(raw.data()),count)){why="payload";return false;}
   hash=Hash(raw.data(),count,hash);stored+=bytes;rawTotal+=count;w=std::max(w/2,1u);y=std::max(y/2,1u);
  }
 }
 if(stored!=h.storedBytes||rawTotal!=h.decodedBytes||(h.height&&!h.chains[0].levels)||hash!=expected){why="checksum/totals";return false;}
 decoded=rawTotal;return true;
}
}
int wmain(int argc,wchar_t**argv){
 if(argc!=3){std::puts("prepared_integrity_check CACHE_DIRECTORY REPORT_CSV");return 2;}
 const std::filesystem::path directory(argv[1]);std::ofstream report{std::filesystem::path(argv[2])};if(!report)return 2;
 report<<"file,version,stored_bytes,decoded_bytes,result\n";unsigned count=0,bad=0;uint64_t bytes=0,decoded=0;auto start=std::chrono::steady_clock::now();
 try{for(const auto& entry:std::filesystem::directory_iterator(directory)){
  if(!entry.is_regular_file()||entry.path().extension()!=L".rvkm")continue;
  uint64_t raw=0;uint32_t version=0;std::string why;bool okay=Validate(entry.path(),raw,why,version);++count;bad+=!okay;bytes+=entry.file_size();decoded+=raw;
  report<<entry.path().filename().string()<<','<<version<<','<<entry.file_size()<<','<<raw<<','<<(okay?"valid":why)<<'\n';
  if(count%250==0){std::printf("Prepared cache: %u checked; %u invalid; %.1f GiB read, %.1f GiB decoded; %.1f seconds\n",count,bad,double(bytes)/(1ull<<30),double(decoded)/(1ull<<30),std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());std::fflush(stdout);report.flush();}
 }}catch(const std::exception& e){std::fprintf(stderr,"Validation stopped: %s\n",e.what());return 2;}
 std::printf("Prepared cache %s: %u files; %u invalid; %llu stored / %llu decoded bytes\n",bad?"FAIL":"PASS",count,bad,(unsigned long long)bytes,(unsigned long long)decoded);return bad?1:0;
}
