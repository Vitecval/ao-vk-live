// Read-only, installation-local cache resolver. No game pointers or database writes.
#include "living.h"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <algorithm>

namespace rvk::living {
namespace {
namespace fs=std::filesystem;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
bool Name(const std::string& value){return !value.empty()&&value.size()<80&&value.find("..") == std::string::npos&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos;}
void NoLinks(fs::path path){
    for(;!path.empty();){
        const auto attr=GetFileAttributesW(path.c_str());
        Require(attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT),"local cache path missing or traverses a link");
        auto parent=path.parent_path();if(parent==path)break;path=parent;
    }
}
std::string Hash(const fs::path& path){
    NoLinks(path);Require(fs::file_size(path)<=128u*1024u*1024u,"local resource exceeds hash budget");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    struct Cleanup{BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    Require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA256 provider unavailable");
    Require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0,"SHA256 initialization failed");
    std::ifstream stream(path,std::ios::binary);Require(bool(stream),"local resource unavailable");
    std::array<unsigned char,65536> buffer{};
    while(stream){stream.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto count=stream.gcount();if(count)Require(BCryptHashData(hash,buffer.data(),ULONG(count),0)>=0,"SHA256 read failed");}
    Require(stream.eof(),"local resource read failed");
    std::array<unsigned char,32> digest{};Require(BCryptFinishHash(hash,digest.data(),digest.size(),0)>=0,"SHA256 completion failed");
    std::string result;for(auto c:digest){result+="0123456789abcdef"[c>>4];result+="0123456789abcdef"[c&15];}return result;
}
}
bool ResolveInstalled(const std::string& client,uint32_t playfield,LocalAssets& out,std::string& error,const std::string& expectedIndex,bool variety){
    try{
        Require(!client.empty(),"AO installation path unavailable");
        const auto base=fs::absolute(fs::path(client));NoLinks(base);
        const auto root=base/"randy-vk/living-local";NoLinks(root);
        std::string generation,extra;const auto pointer=root/"active.txt";NoLinks(pointer);
        Require(fs::file_size(pointer)<128,"invalid local cache pointer");std::ifstream active(pointer);active>>generation;
        Require(Name(generation)&&generation.starts_with("generation-")&&!(active>>extra),"invalid local cache generation");
        const auto dir=root/generation;NoLinks(dir);
        const auto receipt=dir/"cache.receipt";NoLinks(receipt);Require(fs::file_size(receipt)<65536,"local receipt exceeds budget");
        std::ifstream file(receipt);std::string magic,index;uint32_t count=0;file>>magic>>index>>count;
        Require(magic=="LRLOCAL1"&&index==expectedIndex&&count>0&&count<=256,"unsupported local cache identity");
        Require(Hash(base/"cd_image/data/db/ResourceDatabase.idx")==index,"installed AO resource index changed; reprepare local assets");
        std::map<std::string,std::string> hashes;
        for(uint32_t i=0;i<count;++i){std::string name,hash;Require(bool(file>>name>>hash)&&Name(name)&&hash.size()==64&&hash.find_first_not_of("0123456789abcdef")==std::string::npos,"invalid local resource receipt");Require(hashes.emplace(name,hash).second,"duplicate local resource receipt");}
        Require(!(file>>extra),"trailing local receipt data");
        const auto placement="source-"+std::to_string(playfield)+".pf";
        Require(hashes.count(placement)&&Hash(base/"cd_image/data/statels"/(std::to_string(playfield)+".pf"))==hashes.at(placement),"installed placement data changed; old routes disabled");
        const auto scene=std::to_string(playfield)+(variety?"-variety.lrs":".lrs");
        Require(hashes.count(scene)&&Hash(dir/scene)==hashes.at(scene),"playfield route missing or modified");
        std::ifstream routes(dir/scene);std::string provenance;uint32_t pf,verified,fixture,meshes,actors;
        Require(bool(routes>>magic>>pf>>verified>>fixture>>meshes>>actors>>provenance)&&(magic=="LRSCENE5"||magic=="LRSCENE6"||magic=="LRSCENE7")&&pf==playfield&&!fixture&&meshes>0&&meshes<=(magic=="LRSCENE5"?16u:48u),"local route header mismatch");
        LocalAssets result;result.scene=(dir/scene).string();
        // Verify only the current playfield's immutable meshes, once on loading.
        for(uint32_t i=0;i<meshes;++i){std::string name;Require(bool(routes>>name)&&Name(name),"invalid local mesh reference");
            try{if(hashes.count(name)&&Hash(dir/name)==hashes.at(name))result.verified.push_back(name);}catch(const std::exception&){}
        }
        const auto ground="ground-"+std::to_string(playfield)+".lrg";
        Require(hashes.count(ground)&&Hash(dir/ground)==hashes.at(ground),"local terrain support missing or modified");
        // Review opt-in must be created by preparation; a legacy review.ini alone is insufficient.
        Require(hashes.count("review.ini")&&Hash(dir/"review.ini")==hashes.at("review.ini"),"local route review policy modified; reprepare with explicit review option");
        out=std::move(result);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
