#include "living.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <string>
using namespace rvk::living;
static int failures=0;
void Check(bool value,const char* message){if(!value){++failures;std::fprintf(stderr,"FAILED: %s\n",message);}}
int main(int argc,char**argv){
    Route corridor;corridor.linear=true;corridor.points={{0,40,0},{100,40,0}};
    Check(corridor.Finish()&&corridor.length==100,"aerial line has exact validated length");
    for(int i=0;i<=100;++i){Point t,p=corridor.At(float(i),&t);Check(std::fabs(p.x-i)<.0001&&p.y==40&&p.z==0&&t.x==100,"aerial route stays within swept straight corridor");}
    Check(corridor.At(-20).x==0&&corridor.At(120).x==100,"aerial endpoints clamp without spline overshoot");
    Scene roster;roster.playfield=566;roster.meshes.resize(20);
    for(uint32_t i=0;i<20;++i)roster.appearances.push_back({i,i<12?0u:i<16?1u:i<19?2u:3u});
    for(int i=0;i<40;++i){Actor a;a.route.points={{float(i%8)*7,0,float(i/8)*7}};roster.actors.push_back(a);}
    auto second=roster;SelectAppearances(roster,2);SelectAppearances(second,2);
    uint32_t armoredCount=0,heavyCount=0;std::array<bool,20> used{};
    for(size_t i=0;i<roster.actors.size();++i){auto m=roster.actors[i].mesh;Check(m==second.actors[i].mesh,"appearance assignment is deterministic");used[m]=true;armoredCount+=m>=16;heavyCount+=m==19;}
    Check(std::count(used.begin(),used.end(),true)>=12&&armoredCount>=6&&armoredCount<=14&&heavyCount<=4,"varied roster keeps occasional armor distribution");
    SelectAppearances(second,0);for(auto& a:second.actors)Check(a.mesh<16,"armor Off never selects armored appearances");
    Timeline clock;clock.Advance(100);const double before=clock.Advance(100.02);
    Check(std::fabs(before-.02)<1e-6,"simulation follows ordinary frame time");
    Check(clock.Advance(110)==before,"focus/loading suspension cannot teleport actors");
    Check(std::fabs(clock.Advance(110.02)-before-.02)<1e-6,"movement resumes smoothly after suspension");
    const double resumed=clock.time;Check(clock.Advance(109)==resumed,"backwards clock never rewinds route progress");
    clock.Advance(111);const double stalled=clock.Advance(111.2);Check(std::fabs(stalled-resumed-.05)<1e-6,"slow frame catch-up is bounded");
    Route r;r.points={{-5,0,-5},{5,0,-5},{5,0,5},{-5,0,5}};Check(r.Finish(),"route builds");
    float maxStep=0,minStep=1e9;Point p=r.At(0),t;
    for(int i=1;i<=1000;++i){Point q=r.At(r.length*i/1000,&t);float step=std::hypot(std::hypot(q.x-p.x,q.y-p.y),q.z-p.z);maxStep=std::max(maxStep,step);minStep=std::min(minStep,step);p=q;}
    Check(maxStep/minStep<1.02,"arc length table maintains speed to 2 percent");
    auto a=r.At(.001f),b=r.At(r.length+.001f),c=r.At(-.001f),d=r.At(r.length-.001f);
    Check(std::hypot(a.x-b.x,a.z-b.z)<.0001f&&std::hypot(c.x-d.x,c.z-d.z)<.0001f,"loop wrap continuous in both directions");
    Check(Fade(0,100)==0&&Fade(2,100)>0&&Fade(4,100)==1&&Fade(100,100)==0,"camera and distance fade");
    Route bad;bad.points={{0,0,0},{0,0,0},{0,0,0},{0,0,0}};Check(!bad.Finish(),"degenerate route rejected");
    float vp[4][4]={};for(int i=0;i<4;++i)vp[i][i]=1;
    Check(InFrustum(vp,{0,0,.5f},.1f)&&!InFrustum(vp,{3,0,.5f},.1f)&&!InFrustum(vp,{0,0,-3},.1f),"frustum, including near plane");
    Actor person;person.route=r;person.speed=1.2f;person.walkSeconds=10;person.pauseSeconds=3;
    Check(Locomotion(0,person).speed==0&&Locomotion(1,person).speed>1.19f,"pedestrian eases into full speed");
    Check(Locomotion(11,person).speed==0&&Locomotion(11,person).walkWeight==0,"pause selects a real idle clip");
    Check(std::fabs(Locomotion(10,person).distance-Locomotion(12.99,person).distance)<1e-6,"route does not advance during idle");
    Check(std::fabs(Locomotion(13.001,person).distance-Locomotion(12.999,person).distance)<.0001,"pause boundary has no teleport");
    double last=0;for(int i=0;i<10000;++i){auto p=Locomotion(i*.01,person);Check(p.distance>=last&&p.walkWeight>=0&&p.walkWeight<=1,"deterministic nonnegative locomotion");last=p.distance;}
    Check(std::fabs(TurnTowards(3.13f,-3.13f,.016f,1.6f)-3.13f)<.026f,"turn takes shortest angular path across pi");
    Check(std::fabs(TurnTowards(0,2,.016f,1.6f)-.0256f)<1e-6f,"turn angular velocity is bounded");
    Mesh gait;gait.walkFrames=4;gait.stride=2;gait.gaitDistance={0,.25f,1,1.25f,2};
    Check(std::fabs(GaitFrame(gait,.625)-1.5f)<1e-5&&GaitFrame(gait,2)==0,"gait phase follows travelled distance rather than wall time");
    Scene ground;ground.groundWidth=ground.groundHeight=2;ground.groundCell=4;ground.ground={0,4,8,12};float elevation=0;
    Check(GroundHeight(ground,2,2,elevation)&&std::fabs(elevation-6)<.0001f,"independent ground interpolation");
    Check(!GroundHeight(ground,4,0,elevation)&&!GroundHeight(ground,-1,2,elevation),"ground field never wraps out of bounds");
    ground.ground={0,0,0,1};Check(GroundHeight(ground,1,1,elevation)&&elevation==0,"standable ground uses the native triangle split, not bilinear floating");
    struct Vertex{float x,y,z,nx,ny,nz,u,v;};Vertex tri[3]={{0,0,0,0,1,0,0,0},{1,0,0,0,1,0,1,0},{0,1,0,0,1,0,0,1}};uint16_t ix[]={0,1,2},reverse[]={0,2,1};float transform[4][4]={};for(int i=0;i<4;++i)transform[i][i]=1;
    auto hash=GeometryHash(tri,3,32,ix,3,transform);Check(hash&&hash!=GeometryHash(tri,3,32,reverse,3,transform),"exact mechanical fingerprint includes triangle order");
    auto gate=std::filesystem::temp_directory_path()/"living-runtime-policy.lrs";
    {std::ofstream f(gate);f<<"LRSCENE4 566 1 0 1 1 static-survey-only\nmissing-asset.lrm\n";}
    Scene blocked;std::string policy;Check(!Load(gate.string(),blocked,policy,566,true)&&policy.find("static-only")!=std::string::npos,"runtime refuses uncertain routes before opening mesh files");
    Check(!Load(gate.string(),blocked,policy,567,false)&&policy.find("playfield mismatch")!=std::string::npos,"wrong-playfield rejection precedes asset IO");std::filesystem::remove(gate);
    const auto reviewDir=std::filesystem::temp_directory_path()/("living-review-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(reviewDir);const auto reviewScene=(reviewDir/"566.lrs").string();
    Check(!LocalReviewEnabled(reviewScene),"local review defaults disabled");
    {std::ofstream f(reviewDir/"review.ini");f<<"[LivingRubiKa]\nReviewUnverifiedRoutes=0\n";}Check(!LocalReviewEnabled(reviewScene),"local review requires explicit opt-in");
    {std::ofstream f(reviewDir/"review.ini");f<<"[LivingRubiKa]\nReviewUnverifiedRoutes=1\n";}Check(LocalReviewEnabled(reviewScene),"explicit client-local opt-in survives launcher environment loss");
    Check(!LocalReviewEnabled((reviewDir/"other-client"/"566.lrs").string()),"review opt-in stays local to its pack directory");
    {std::ofstream f(reviewDir/"review.ini");f<<std::string(2049,'x');}Check(!LocalReviewEnabled(reviewScene),"oversize local-review configuration rejected");
    std::filesystem::remove(reviewDir/"review.ini");std::filesystem::remove(reviewDir);
    // A synthetic installed-client cache, with an independently known SHA256
    // for its empty index. Production callers always use the audited AO pin.
    const auto localDir=std::filesystem::current_path()/("living-local-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto generation=localDir/"randy-vk/living-local/generation-synthetic";
    std::filesystem::create_directories(generation);
    std::filesystem::create_directories(localDir/"cd_image/data/db");
    std::filesystem::create_directories(localDir/"cd_image/data/statels");
    auto put=[](const std::filesystem::path& path,const std::string& value){std::ofstream f(path,std::ios::binary);f<<value;};
    const std::string emptyHash="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    const std::string scene="LRSCENE5 566 1 0 1 1 synthetic-local-test\nmissing.lrm\n1 0 1 0 1 0 0 100 0 1 10 30 10 16 7 0 0 0 0 0 -1 0\n0\n-\n";
    put(localDir/"cd_image/data/db/ResourceDatabase.idx","");
    put(localDir/"cd_image/data/statels/566.pf","");
    put(generation.parent_path()/"active.txt","generation-synthetic\n");
    put(generation/"566.lrs",scene);put(generation/"ground-566.lrg","");put(generation/"review.ini","");
    put(generation/"cache.receipt","LRLOCAL1 "+emptyHash+" 4\n566.lrs 4ed442eb6bdb7d1e0407c21a653f01d1c61aa200401e869eb1c3031c0da56d5d\nground-566.lrg "+emptyHash+"\nreview.ini "+emptyHash+"\nsource-566.pf "+emptyHash+"\n");
    LocalAssets resolved;std::string localError;
    Check(ResolveInstalled(localDir.string(),566,resolved,localError,emptyHash),localError.c_str());
    Check(resolved.verified.empty(),"unverified mesh is never handed to the loader");
    Scene partial;
    Check(Load(resolved.scene,partial,localError,566,false,&resolved.verified),localError.c_str());
    Check(partial.meshes.size()==1&&!partial.meshes[0].available&&partial.diagnostics.size()==1,"missing optional model skips its instances with a diagnostic");
    Check(!Load(resolved.scene,partial,localError,566,false),"legacy fixture load stays strict");
    Check(!ResolveInstalled(localDir.string(),567,resolved,localError,emptyHash),"another playfield cannot reuse Newland route");
    put(generation/"review.ini","unverified override");
    Check(!ResolveInstalled(localDir.string(),566,resolved,localError,emptyHash),"modified review file rejected");put(generation/"review.ini","");
    put(generation/"566.lrs",scene+"modified");
    Check(!ResolveInstalled(localDir.string(),566,resolved,localError,emptyHash),"modified scene rejected before loading");put(generation/"566.lrs",scene);
    put(localDir/"cd_image/data/db/ResourceDatabase.idx","updated");
    Check(!ResolveInstalled(localDir.string(),566,resolved,localError,emptyHash),"changed installation index rejected");
    put(generation.parent_path()/"active.txt","../../outside\n");
    Check(!ResolveInstalled(localDir.string(),566,resolved,localError,emptyHash),"cache generation cannot traverse directories");
    if(std::filesystem::weakly_canonical(localDir).parent_path()==std::filesystem::canonical(std::filesystem::current_path()))std::filesystem::remove_all(localDir);
    else Check(false,"synthetic cache cleanup must remain inside test workspace");
    if(argc>2&&std::string(argv[1])=="--scene"){Scene s;std::string e;const bool ok=Load(argv[2],s,e);Check(ok,e.c_str());if(!ok)return 1;std::printf("Scene: %u, %zu actors, %zu meshes, %.2f MiB\n",s.playfield,s.actors.size(),s.meshes.size(),s.bytes/1048576.);}
    else if(argc>1){Scene s;std::string e;bool loaded=Load(argv[1],s,e);Check(loaded,e.c_str());if(!loaded)return 1;Check(s.fixture&&s.playfield==FixturePlayfield,"fixture isolated from live zones");Check(s.actors.size()==5&&s.meshes.size()==3,"complete fixture inventory");Check(!s.meshes.empty()&&s.meshes[0].bones==4&&s.meshes[0].frameCount==90,"independent skeletal asset loaded");
        auto temp=std::filesystem::temp_directory_path()/"living-reject.lrs";
        for(const auto& text:{"LRSCENE2 566 0 0 1 1 unknown\n", "LRSCENE2 566 1 1 1 1 fixture-provenance\n", "LRSCENE2 566 1 0 1 1 reviewed-route\n../../game.lrm\n"}){
            {std::ofstream f(temp);f<<text;}Scene rejected;Check(!Load(temp.string(),rejected,e),"unverified, mismatched fixture or traversal rejected");}
        std::filesystem::remove(temp);
    }
    std::printf("Living Rubi-Ka core checks: %s (%d failures)\n",failures?"FAILED":"passed",failures);return failures?1:0;
}
