// Decode side-loaded artwork before launching AO. Never opens the game or changes source PNGs.
#include "material_cache.h"
#include "preview23_artwork.h"
#include "terrain_materials.h"
#include "building_families.h"
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <regex>

namespace fs=std::filesystem;
int main(int argc,char** argv) try {
    fs::path client,output,catalog; std::set<std::pair<uint32_t,uint32_t>> selected;
    unsigned shard=0,shards=1;
    for (int i=1;i<argc;++i) {
        std::string option=argv[i];
        if (i+1>=argc) throw std::runtime_error("Missing argument");
        if (option=="--client") client=fs::absolute(argv[++i]);
        else if (option=="--output") output=fs::absolute(argv[++i]);
        else if (option=="--catalog-only") catalog=fs::absolute(argv[++i]);
        else if (option=="--shard") {
            if(std::sscanf(argv[++i],"%u/%u",&shard,&shards)!=2||!shards||shards>8||shard>=shards)throw std::runtime_error("Invalid preparation shard");
        }
        else if (option=="--capture") {
            std::ifstream in(argv[++i]); if(!in)throw std::runtime_error("Cannot read capture");
            std::string line; const std::regex id("rdb:([0-9]+) type:([0-9]+)");
            while(std::getline(in,line)) {std::smatch m;if(std::regex_search(line,m,id)) {
                uint32_t type=uint32_t(std::stoul(m[2]));
                if(type==1010016||type==1010017)type=1010004;
                if(type==1010021||type==1010022)type=1010006;
                if(type==1010019||type==1010020)type=1010011;
                selected.emplace(type,uint32_t(std::stoul(m[1])));
            }}
        } else throw std::runtime_error("Unknown option: "+option);
    }
    if(client.empty()||output.empty()) {std::cerr<<"Usage: ao-vk-prepare-materials --client <AO folder> --output <cache folder> [--capture <frame.txt>]\n";return 2;}
    std::vector<fs::path> folders;
    if(fs::is_directory(client/"randy-vk/materials"))folders.push_back(client/"randy-vk/materials");
    if(fs::is_directory(client/"randy-vk/packs"))for(const auto& e:fs::directory_iterator(client/"randy-vk/packs"))if(e.is_directory())folders.push_back(e.path());
    std::sort(folders.begin(),folders.end());
    unsigned built=0,cached=0,failed=0;uint64_t bytes=0;
    std::ofstream recipes;
    if(!catalog.empty()){recipes.open(catalog,std::ios::binary);if(!recipes)throw std::runtime_error("Cannot create recipe catalogue");recipes<<"RVK-RECIPES-1\n";}
    const auto began=std::chrono::steady_clock::now();
    const std::regex pattern("([0-9]+)_([0-9]+)_(d|n|h|orm|ao|r|m|e|leaves|leaves_n)\\.png",std::regex::icase);
    for(const auto& folder:folders) {
        std::map<std::pair<uint32_t,uint32_t>,std::map<std::string,std::string>> sets;
        for(const auto& entry:fs::directory_iterator(folder)) {
            if(!entry.is_regular_file())continue;
            std::smatch m;std::string name=entry.path().filename().string();
            if(!std::regex_match(name,m,pattern))continue;
            auto id=std::make_pair(uint32_t(std::stoul(m[1])),uint32_t(std::stoul(m[2])));
            if(!selected.empty()&&!selected.count(id))continue;
            sets[id][m[3]]=entry.path().string();
        }
        const std::string name=folder.filename().string();
        const bool high=name=="ao-cyber-world"||name=="ao-cyber-icc"||name=="ao-icc-enhanced-original";
        const bool modern=high||name=="ao-modern-icc"||name=="ao-cyber-characters";
        for(auto& [id,files]:sets) {
            rvk::maps::MaterialFiles f;
            f.normal=files["n"];f.height=files["h"];f.packed=files["orm"];
            f.parts[0]=files["ao"];f.parts[1]=files["r"];f.parts[2]=files["m"];
            f.albedo=files["d"];f.emissive=files["e"];f.leaves=files["leaves"];f.leavesNormal=files["leaves_n"];
            f.maxSize=modern?512:2048;
            f.preserveAlbedoAlpha=(high&&id.first!=1010006)||name=="ao-cyber-characters";
            // Match FilesFor's eligibility rules. In particular, an unreviewed
            // ground repaint is never loaded by the game and needs no cache.
            const bool completedGround=name=="ao-cyber-world"&&rvk::materials::Preview23CompletedGround(id.first,id.second)&&
                !f.albedo.empty()&&!f.normal.empty()&&!f.packed.empty();
            if(modern&&rvk::materials::IsGroundType(id.first)&&!rvk::materials::AuthoredGround(id.first,id.second)&&!completedGround)continue;
            const bool modern2=name=="ao-cyber-world"||name=="ao-cyber-icc";
            if(modern2&&rvk::materials::Preview23CompletedWorld(id.first,id.second)&&
                (f.albedo.empty()||f.normal.empty()||f.packed.empty()))continue;
            if(modern2&&rvk::buildings::Managed(id.first,id.second)&&!rvk::materials::Preview23CompletedWorld(id.first,id.second)){
                const auto* resource=rvk::buildings::ResourceOf(id.first,id.second);
                if(!rvk::buildings::FamilyOf(id.first,id.second)||!resource)continue;
                const std::string* required[]={&f.albedo,&f.normal,&f.packed,&f.emissive,&f.height};bool valid=true;
                for(unsigned i=0;i<5;++i)if((resource->maps&(1u<<i))&&required[i]->empty())valid=false;
                if(!valid)continue;
            }
            if(rvk::materials::DeckGround(id.first,id.second)) {
                const bool completed=name=="ao-cyber-world"&&rvk::materials::Preview23CompletedGround(id.first,id.second)&&
                    !f.albedo.empty()&&!f.normal.empty()&&!f.packed.empty();
                if(!completed)f.albedo.clear();
                f.normal.clear();f.height.clear();f.emissive.clear();
            }
            if(!f.Any())continue;
            // Store the highest runtime choice once. Every smaller cap and
            // alpha policy reads these exact filtered levels from the same file.
            {
                if(high&&!rvk::materials::IsGroundType(id.first)){f.maxAlbedoSize=2048;f.maxAlbedoPixels=1024u*1024u;}
                if(rvk::maps::prepared::Key(f)%shards!=shard)continue;
                if(recipes.is_open()){
                    const auto key=rvk::maps::prepared::Key(f);
                    std::ifstream cache(rvk::maps::prepared::Path(output,key),std::ios::binary);rvk::maps::prepared::Header header{};
                    if(!cache.read(reinterpret_cast<char*>(&header),sizeof(header))||header.key!=key||header.version!=4){++failed;continue;}
                    char hex[17];std::snprintf(hex,sizeof(hex),"%016llx",static_cast<unsigned long long>(key));recipes<<hex;
                    for(const auto* path:{&f.normal,&f.height,&f.packed,&f.parts[0],&f.parts[1],&f.parts[2],&f.albedo,&f.emissive,&f.leaves,&f.leavesNormal})
                        recipes<<'\t'<<(path->empty()?std::string{}:fs::relative(*path,client).generic_string());
                    recipes<<"\n";++cached;continue;
                }
                rvk::maps::Decoded d;
                if(rvk::maps::prepared::Load(output,f,d)){++cached;continue;}
                d=rvk::maps::Decode(f);
                if(!rvk::maps::prepared::Save(output,f,d)){++failed;std::cerr<<"Failed "<<name<<" "<<id.first<<":"<<id.second<<" "<<d.errors<<"\n";continue;}
                rvk::maps::Decoded verified;
                if(!rvk::maps::prepared::Load(output,f,verified)){++failed;std::cerr<<"Verification failed "<<name<<" "<<id.first<<":"<<id.second<<"\n";continue;}
                bytes+=fs::file_size(rvk::maps::prepared::Path(output,rvk::maps::prepared::Key(f)));++built;
                if(built<=3||built%50==0)std::cout<<"Prepared "<<built<<" sets, "<<(bytes>>20)<<" MiB; "<<name<<" "<<id.first<<":"<<id.second<<std::endl;
            }
        }
    }
    std::cout<<"Finished: "<<built<<" prepared, "<<cached<<" reused, "<<failed<<" failed; "<<(bytes>>20)<<" MiB; "
        <<std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<<" seconds.\n";
    return failed?1:0;
} catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
