#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace rvk::attachment {
using Point=std::array<float,3>;
inline Point Sub(Point a,Point b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
inline float Dot(Point a,Point b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Point Add(Point a,Point d,float t){return {a[0]+d[0]*t,a[1]+d[1]*t,a[2]+d[2]*t};}
inline Point Closest(Point p,Point a,Point b,Point c) {
    const Point ab=Sub(b,a),ac=Sub(c,a),ap=Sub(p,a);
    const float d1=Dot(ab,ap),d2=Dot(ac,ap);
    if(d1<=0 && d2<=0)return a;
    const Point bp=Sub(p,b);const float d3=Dot(ab,bp),d4=Dot(ac,bp);
    if(d3>=0 && d4<=d3)return b;
    const float vc=d1*d4-d3*d2;
    if(vc<=0 && d1>=0 && d3<=0)return Add(a,ab,d1/std::max(d1-d3,1e-20f));
    const Point cp=Sub(p,c);const float d5=Dot(ab,cp),d6=Dot(ac,cp);
    if(d6>=0 && d5<=d6)return c;
    const float vb=d5*d2-d1*d6;
    if(vb<=0 && d2>=0 && d6<=0)return Add(a,ac,d2/std::max(d2-d6,1e-20f));
    const float va=d3*d6-d5*d4;
    if(va<=0 && d4-d3>=0 && d5-d6>=0)return Add(b,Sub(c,b),(d4-d3)/std::max((d4-d3)+(d5-d6),1e-20f));
    const float sum=va+vb+vc;
    if(!std::isfinite(sum) || std::fabs(sum)<1e-20f)return a;
    return Add(Add(a,ab,vb/sum),ac,vc/sum);
}
}
