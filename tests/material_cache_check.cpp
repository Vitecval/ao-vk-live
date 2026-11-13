#include "material_cache.h"
#include "material_cache_legacy.h"
#include <iostream>
#include <stdexcept>
#include <chrono>
using namespace rvk::maps;
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void Same(Decoded& a,Decoded& b){
    auto x=prepared::Chains(a),y=prepared::Chains(b);
    for(size_t i=0;i<x.size();++i)Require(x[i]->w==y[i]->w&&x[i]->h==y[i]->h&&x[i]->levels==y[i]->levels,"Prepared pixels differ");
    Require(a.height==b.height&&a.preserveAlbedoAlpha==b.preserveAlbedoAlpha,"Material flags differ");
}
int main(int argc,char** argv)try{
    if(argc==4&&std::string(argv[1])=="--probe"){
        const std::string prefix=argv[2];MaterialFiles f;
        auto path=[&](const char* suffix){auto p=prefix+suffix;return std::filesystem::exists(p)?p:std::string{};};
        f.normal=path("_n.png");f.height=path("_h.png");f.packed=path("_orm.png");f.albedo=path("_d.png");f.emissive=path("_e.png");
        f.parts[0]=path("_ao.png");f.parts[1]=path("_r.png");f.parts[2]=path("_m.png");f.leaves=path("_leaves.png");f.leavesNormal=path("_leaves_n.png");
        const std::string pack=std::filesystem::path(prefix).parent_path().filename().string();f.maxSize=512;
        f.preserveAlbedoAlpha=pack!="ao-modern-icc";
        if(pack!="ao-cyber-characters"&&pack!="ao-modern-icc"){f.maxAlbedoSize=2048;f.maxAlbedoPixels=1024*1024;}
        Decoded loaded;if(!prepared::Load(argv[3],f,loaded))return 2;
        auto fresh=Decode(f);Require(!fresh.decodeFailed,"Probe source decode failed");Same(fresh,loaded);
        f.maxSize=f.maxAlbedoSize=256;f.maxAlbedoPixels=256*256;
        Require(prepared::Load(argv[3],f,loaded),"Probe 256-pixel reuse failed");fresh=Decode(f);Same(fresh,loaded);
        std::cout<<"PASS: cached native and 256-pixel mip data match "<<prefix<<"\n";return 0;
    }
    Require(argc==3||argc==4,"Usage: material_cache_check <source material prefix> <scratch folder> [legacy cache folder]");
    const std::filesystem::path scratch=std::filesystem::absolute(argv[2]);
    std::filesystem::create_directories(scratch);
    MaterialFiles f;const std::string source=argv[1];
    auto copy=[&](const char* suffix){
        auto path=scratch/(std::string("source")+suffix);
        std::filesystem::copy_file(source+suffix,path,std::filesystem::copy_options::overwrite_existing);
        return path.string();
    };
    f.normal=copy("_n.png");f.packed=copy("_orm.png");f.albedo=copy("_d.png");
    f.maxSize=512;f.maxAlbedoSize=2048;f.maxAlbedoPixels=1024*1024;f.preserveAlbedoAlpha=true;
    if(argc==4){auto original=f;original.normal=source+"_n.png";original.packed=source+"_orm.png";original.albedo=source+"_d.png";
        Decoded previous;bool found=legacy::Load(argv[3],original,previous);
        if(!found){original.maxAlbedoSize=original.maxAlbedoPixels=0;found=legacy::Load(argv[3],original,previous);}
        Require(found,"Existing v3 cache failed compatibility read");auto fresh=Decode(original);Same(fresh,previous);}
    auto d=Decode(f);Require(!d.decodeFailed&&d.Bytes()>0,"Source decode failed");
    const auto root=scratch/"cache";Decoded loaded;
    Require(prepared::Save(root,f,d),"Save failed");Require(prepared::Load(root,f,loaded),"Load failed");Same(d,loaded);
    double decodeMs=0,preparedMs=0;
    for(int i=0;i<5;++i){
        auto start=std::chrono::steady_clock::now();auto original=Decode(f);
        decodeMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        start=std::chrono::steady_clock::now();Decoded hit;Require(prepared::Load(root,f,hit),"Timed read failed");
        preparedMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();Same(original,hit);
    }
    std::cout<<"Same material: PNG preparation "<<decodeMs/5<<" ms; prepared read "<<preparedMs/5<<" ms (5 warm filesystem samples)\n";
    const auto key=prepared::Key(f); auto other=f;other.maxSize=256;other.maxAlbedoSize=256;other.maxAlbedoPixels=256*256;
    Require(prepared::Load(root,other,loaded),"Smaller quality failed to reuse mip chains");auto lower=Decode(other);Same(lower,loaded);
    other=f;other.preserveAlbedoAlpha=false;Require(prepared::Load(root,other,loaded),"Alpha policy failed to reuse pixels");auto alpha=Decode(other);Same(alpha,loaded);
    other=f;other.maxSize=1024;Require(!prepared::Load(root,other,loaded),"Insufficient prepared quality was accepted");
    {std::ifstream file(prepared::Path(root,key),std::ios::binary);prepared::Header h;file.read(reinterpret_cast<char*>(&h),sizeof(h));
     Require(h.storedBytes<h.decodedBytes,"Lossless compression did not reduce the representative material");
     std::cout<<"Prepared bytes "<<h.storedBytes<<"; decoded bytes "<<h.decodedBytes<<"\n";}
    auto path=prepared::Path(root,key);
    {std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);file.seekp(-1,std::ios::end);char bad='!';file.write(&bad,1);}
    Require(!prepared::Load(root,f,loaded),"Corrupt payload accepted");
    Require(prepared::Save(root,f,d),"Repair failed");Require(prepared::Load(root,f,loaded),"Repaired cache rejected");
    {std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary);prepared::Header h;file.read(reinterpret_cast<char*>(&h),sizeof(h));h.chains[0].w=0xffffffffu;file.seekp(0);file.write(reinterpret_cast<char*>(&h),sizeof(h));}
    Require(!prepared::Load(root,f,loaded),"Oversized dimensions accepted");
    Require(prepared::Save(root,f,d),"Repair failed");
    std::filesystem::resize_file(path,sizeof(prepared::Header)+4);
    Require(!prepared::Load(root,f,loaded),"Truncated cache accepted");
    Require(prepared::Save(root,f,d),"Repair failed");
    {std::ofstream changed(f.albedo,std::ios::binary|std::ios::app);changed.put('x');}
    Require(prepared::Key(f)!=key&&!prepared::Load(root,f,loaded),"Changed source accepted stale cache");
    std::cout<<"PASS: exact material mip/alpha round trip; source/quality invalidation; corrupt, oversized and truncated cache fallback\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
