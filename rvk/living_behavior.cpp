#include "living.h"
#include <algorithm>
#include <cmath>

namespace rvk::living {
namespace {
uint32_t Mix(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
float Unit(uint32_t x){return float(Mix(x)&65535)/65535.f;}
float Smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
float Length(Point a,Point b){return std::hypot(std::hypot(a.x-b.x,a.z-b.z),a.y-b.y);}
Point Lerp(Point a,Point b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
const Mesh::Clip* Clip(const Mesh& m,uint32_t kind){for(const auto& c:m.activities)if(c.kind==kind)return &c;return nullptr;}
uint32_t BodyBit(const Scene& s,const Actor& a){if(s.appearances.empty())return ~0u;for(size_t i=0;i<s.appearances.size();++i)if(s.appearances[i].mesh==a.mesh)return 1u<<i;return 0;}
}

HumanPose AdvanceHuman(Scene& s,Actor& a,double time,uint32_t index){
    auto& h=a.human;const auto& mesh=s.meshes[a.mesh];const uint32_t bit=BodyBit(s,a);
    if(h.last<0){
        h.seed=Mix(s.behaviorSeed^s.playfield*773u^index*997u);h.from=a.route.At(0);h.to=h.from;
        float best=.12f;for(size_t i=0;i<s.walkNodes.size();++i){float d=Length(h.from,s.walkNodes[i].point);if(d>=best)continue;
            bool connected=false;for(const auto& e:s.walkEdges)if((e.a==i||e.b==i)&&(e.mask&bit)==bit){connected=true;break;}
            if(connected){best=d;h.node=int32_t(i);}}
        if(h.node>=0)h.from=h.to=s.walkNodes[h.node].point;
        h.resting=true;h.wait=Unit(h.seed)*7;h.last=time;h.activity=-1;
    }
    const float dt=float(std::clamp(time-h.last,0.,.05));h.last=time;
    auto startWalk=[&]{
        ++h.serial;const uint32_t key=Mix(h.seed+h.serial*67);h.activity=-1;h.elapsed=0;h.speed=0;h.progress=0;
        if(h.node>=0){
            std::vector<uint32_t> choices;
            for(const auto& e:s.walkEdges)if((e.mask&bit)==bit&&(e.a==uint32_t(h.node)||e.b==uint32_t(h.node))){uint32_t next=e.a==uint32_t(h.node)?e.b:e.a;
                bool occupied=false;for(const auto& other:s.actors)if(&other!=&a&&other.group==0&&other.human.last>=0&&other.human.resting&&other.human.node==int32_t(next)){occupied=true;break;}
                if(!occupied)choices.push_back(next);}
            if(choices.size()>1)std::erase(choices,uint32_t(h.previousNode));
            if(choices.empty()){h.resting=true;h.wait=3+Unit(key)*5;return;}
            h.next=int32_t(choices[Mix(key)%choices.size()]);h.from=s.walkNodes[h.node].point;h.to=s.walkNodes[h.next].point;h.goal=Length(h.from,h.to);
        }else{h.direction=Unit(key+7)<.32f?-h.direction:h.direction;h.goal=3+Unit(key+11)*11;}
        h.resting=false;
    };
    if(h.resting){
        h.elapsed+=dt;if(h.elapsed>=h.wait)startWalk();
    }else{
        h.elapsed+=dt;const float maximum=std::clamp(a.speed,.65f,1.6f)*(.80f+.35f*Unit(h.seed+91));
        const float remaining=std::max(0.f,h.goal-h.progress);
        h.speed=std::min({maximum,maximum*Smooth(h.elapsed/.8f),std::sqrt(1.8f*remaining)});
        const float step=std::min(remaining,h.speed*dt);h.progress+=step;h.distance+=step;
        if(h.node<0){h.routeDistance+=step*h.direction;h.from=a.route.At(float(h.routeDistance));}
        if(h.goal-h.progress<.008f){
            if(h.next>=0){h.previousNode=h.node;h.node=h.next;h.next=-1;h.from=h.to;}
            h.resting=true;h.elapsed=0;h.speed=0;const uint32_t key=Mix(h.seed+h.serial*97);
            h.wait=5+Unit(key)*22;h.activity=-1;
            // Rest points are flat, ground-level locations validated with the
            // complete sitting envelope, never guessed seats on scenery.
            const bool rest=h.node>=0&&(s.walkNodes[h.node].restMask&bit)==bit;
            if(rest&&Clip(mesh,3)&&Clip(mesh,4)&&Clip(mesh,5)&&Unit(key+3)<.24f){h.activity=3;h.wait=22+Unit(key+4)*25;}
            else if(Unit(key+5)<.40f){uint32_t kind=Mix(key+6)%3;if(Clip(mesh,kind)){h.activity=int32_t(kind);h.wait=std::max(h.wait,Clip(mesh,kind)->seconds+3);}}
        }
    }
    HumanPose p;p.point=h.node<0?a.route.At(float(h.routeDistance),&p.tangent):h.resting?h.from:Lerp(h.from,h.to,h.goal>0?h.progress/h.goal:0);
    if(h.node>=0&&!h.resting)h.tangent={h.to.x-h.from.x,h.to.y-h.from.y,h.to.z-h.from.z};
    if(h.node<0&&!h.resting){p.tangent.x*=h.direction;p.tangent.z*=h.direction;h.tangent=p.tangent;}
    p.tangent=h.tangent;p.movement={h.distance,h.speed,h.resting?0.f:Smooth(h.speed/std::max(.1f,a.speed))};
    if(h.resting&&h.activity>=0&&h.elapsed>.8f){
        float t=h.elapsed-.8f;p.activity=h.activity;
        if(h.activity==3){const auto down=Clip(mesh,3),idle=Clip(mesh,4),up=Clip(mesh,5);const float stand=h.wait-up->seconds-.4f;
            if(h.elapsed>=stand){p.activity=5;t=h.elapsed-stand;}
            else if(t>=down->seconds){p.activity=4;t=std::fmod(t-down->seconds,idle->seconds);}
            p.activityTime=t;p.activityWeight=Smooth((h.elapsed-.8f)/.3f)*Smooth((h.wait-h.elapsed)/.3f);
        }else if(const auto c=Clip(mesh,uint32_t(h.activity));c&&t<c->seconds){p.activityTime=t;p.activityWeight=Smooth(t/.25f)*Smooth((c->seconds-t)/.25f);}
        else p.activity=-1;
    }
    return p;
}

FlightPose PhasefrontFlight(const Scene& s,const Actor& a,double time,uint32_t index){
    FlightPose p;if(s.flightLanes.empty())return p;
    const uint32_t seed=Mix(s.behaviorSeed^s.playfield*337u^index*1999u);
    // Fixed windows permit direct evaluation after culling; each window makes
    // fresh decisions. No random numbers or collision searches occur per frame.
    const double t=std::max(0.,time+Unit(seed)*220),window=100.;const uint32_t cycle=uint32_t(t/window);
    const uint32_t key=Mix(seed+cycle*977u);const float local=float(std::fmod(t,window));
    const auto& lane=s.flightLanes[Mix(key+1)%s.flightLanes.size()];const bool reverse=Mix(key+2)&1;
    const float speed=8+Unit(key+3)*8,duration=lane.length/speed,start=3+Unit(key+4)*std::max(1.f,float(window)-duration-8);
    const float elapsed=local-start;if(Unit(key+5)<.30f||elapsed<0||elapsed>=duration)return p;
    float u=elapsed/duration;
    // Small speed variation and smooth lateral corrections resemble steering.
    // Offset norm is bounded by 1.2 m, inside the prepared 3 m safety margin.
    const float progress=u+.025f*std::sin(u*6.2831853f),distance=(reverse?1-progress:progress)*lane.length;
    p.point=lane.At(distance,&p.tangent);if(reverse){p.tangent.x=-p.tangent.x;p.tangent.z=-p.tangent.z;}
    const float length=std::max(.01f,std::hypot(p.tangent.x,p.tangent.z));
    const float wave=std::sin(u*6.2831853f+Unit(key+6)*6.28f)*std::sin(u*3.14159265f);
    p.point.x+=p.tangent.z/length*wave;p.point.z-=p.tangent.x/length*wave;
    p.point.y+=.55f*std::sin(u*6.2831853f)*std::sin(u*3.14159265f);
    p.bank=.06f*std::cos(u*6.2831853f)*std::sin(u*3.14159265f);
    p.alpha=Smooth(elapsed/3)*Smooth((duration-elapsed)/3);p.active=true;return p;
}
}
