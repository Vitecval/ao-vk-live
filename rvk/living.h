// Renderer-owned data only. No game pointers, scene nodes or server identifiers.
#pragma once
#include "skin.h"
#include <array>
#include <string>
#include <map>
#include <istream>

namespace rvk { class Texture; }
namespace rvk::living {
constexpr uint32_t FixturePlayfield = 0xfffffffeu;
constexpr size_t MaxBytes = 64u << 20; // hard cap including decoded source maps; shared materials counted once
struct Point { float x=0,y=0,z=0; };
struct Route {
    bool linear=false,lowFlight=false;
    std::vector<Point> points;
    std::vector<float> arc;
    float length=0;
    bool Finish();
    Point At(float distance, Point* tangent=nullptr) const;
};
struct Mesh {
    std::shared_ptr<const std::vector<uint8_t>> vertices;
    std::array<std::shared_ptr<const std::vector<uint16_t>>,3> indices;
    std::array<std::shared_ptr<const std::vector<uint8_t>>,3> indexBytes;
    std::shared_ptr<skin::Source> skin;
    std::vector<skin::Bone> frames,poseScratch;
    std::vector<skin::TriVertex> contacts; // bounded original boot/foot vertices; no IK or game collision
    uint32_t vertexCount=0, bones=0, frameCount=0, dimension=0;
    float fps=0, radius=0;
    uint32_t walkFrames=0, materialOwner=~0u;
    uint32_t idleFrames=0;
    struct Clip {uint32_t kind=0,first=0,count=0;float seconds=0;};
    std::vector<Clip> activities;
    std::array<uint32_t,4> mapDimensions{};
    float walkSeconds=0, idleSeconds=0, stride=0;
    std::vector<float> gaitDistance;
    bool cutout=false,available=true;
    std::array<std::vector<uint32_t>,4> pixels; // ARGB albedo, GL normal, ORM, emission
    Texture* texture=nullptr;
};
struct Actor {
    uint32_t group=0, mesh=0, tier=1, motion=0;
    uint32_t appearanceMask=~0u;
    float scale=1,speed=1,phase=0,range=100,hover=0;
    float walkSeconds=16,pauseSeconds=5,heading=0;
    float idleYaw=0,lightRange=0;
    float visibilityAlpha=0;
    uint32_t lightColour=0, replacementMask=0;
    int32_t parent=-1;double activationTime=-1;
    bool surface=false;float supportHeight=0;uint32_t replacementStable=0;
    double headingTime=-1;float facingYaw=0;
    Route route;
    std::shared_ptr<const skin::Palette> previous;
    std::shared_ptr<skin::Job> job; // retained through the frame; GPU caches validate lifetime serials
    float world[4][4]={};
    uint64_t lastFrame=~uint64_t(0);
    struct Human {
        double last=-1,distance=0,routeDistance=0;float elapsed=0,wait=0,speed=0,goal=0,progress=0;
        int32_t node=-1,next=-1,previousNode=-1,activity=-1;uint32_t serial=0,seed=0;
        float direction=1;Point from,to,tangent{0,0,1};bool resting=false;
    } human;
};
struct Replacement {
    uint32_t vertices=0,indices=0;
    Point origin;
    uint64_t hash=0,lastSeen=~uint64_t(0),suppressedFrame=~uint64_t(0);
    bool witnessed=false;float boundsMin[3]{},boundsMax[3]{};
};
struct Timeline {
    double time=0,last=-1;
    double Advance(double wall); // pause on suspension; cap catch-up after a slow render frame
};
struct Scene {
    Timeline timeline;
    uint32_t playfield=0;
    bool fixture=false;
    bool liveVerified=false;
    std::vector<Mesh> meshes;
    std::vector<Actor> actors;
    struct Appearance {uint32_t mesh=0,role=0;};
    std::vector<Appearance> appearances;
    bool requestedVariety=false;
    uint32_t armoredLevel=0;
    bool humanBehavior=false;
    uint32_t behaviorSeed=1;
    struct WalkNode {Point point;uint32_t restMask=0;};
    struct WalkEdge {uint32_t a=0,b=0,mask=0;};
    std::vector<WalkNode> walkNodes;
    std::vector<WalkEdge> walkEdges;
    std::vector<Route> flightLanes;
    std::vector<Replacement> replacements;uint32_t replacementStable=0;
    std::vector<uint64_t> diagnosticCandidates; // bounded distinct near-equipment hashes for live diagnosis
    uint32_t groundWidth=0,groundHeight=0;
    float groundCell=0;
    std::vector<float> ground;
    size_t bytes=0;
    std::vector<std::string> diagnostics;
};
bool LocalReviewEnabled(const std::string& scenePath);
bool Load(const std::string& path, Scene& out, std::string& error,uint32_t expectedPlayfield=0,bool requireLiveVerification=false,const std::vector<std::string>* verifiedAssets=nullptr);
struct LocalAssets { std::string scene; std::vector<std::string> verified; };
inline constexpr const char* InstalledIndex="9f3633726602ead5003bd3aec82f67f63462ff1080a5676f91c2f149ddfbf6d9";
bool ResolveInstalled(const std::string& client,uint32_t playfield,LocalAssets& result,std::string& error,const std::string& expectedIndex=InstalledIndex,bool variety=false);
void SelectAppearances(Scene& scene,uint32_t armoredLevel);
// Reserved fixture playfield only. Binary stdin resources allow renderer
// evidence without storing extracted assets outside the selected installation.
bool ReadFixtureResources(std::istream& input,std::string& error);
bool FixtureResource(const std::string& name,std::string& bytes);
float Fade(float distance,float range);
bool InFrustum(const float viewProjection[4][4], Point p,float radius);
struct Movement { double distance=0; float speed=0, walkWeight=0; };
struct HumanPose {Point point,tangent;Movement movement;int32_t activity=-1;float activityTime=0,activityWeight=0;};
HumanPose AdvanceHuman(Scene& scene,Actor& actor,double time,uint32_t actorIndex);
struct FlightPose {Point point,tangent;float alpha=0,bank=0;bool active=false;};
FlightPose PhasefrontFlight(const Scene& scene,const Actor& actor,double time,uint32_t actorIndex);
Movement Locomotion(double time,const Actor& actor);
float GaitFrame(const Mesh& mesh,double distance);
float TurnTowards(float previous,float desired,float seconds,float radiansPerSecond);
float ContactOffset(const Scene& scene,const Mesh& mesh,const skin::Palette& palette,const float world[4][4],bool authoredSurface=false);
bool GroundHeight(const Scene& scene,float x,float z,float& height);
uint64_t GeometryHash(const void* vertices,uint32_t vertexCount,uint32_t stride,
                      const uint16_t* indices,uint32_t indexCount,const float world[4][4]);
}
