#ifdef NDEBUG
#undef NDEBUG
#endif
#include "rvk/underwater_policy.h"
#include <array>
#include <cassert>
#include <limits>
int main(){using namespace rvk::underwater;float y=0;std::array<float,4> a={80,80,std::numeric_limits<float>::quiet_NaN(),95};
assert(Surface(a,2,2,10,20,2,11,21,y)&&y==80&&Below(73,y)&&!Below(80,y));
assert(!Surface(a,2,2,10,20,2,11,23,y));assert(!Surface(a,2,2,10,20,2,9,21,y));
assert(Surface(a,2,2,10,20,2,13,23,y)&&y==95);assert(!Below(94.9f,y));
auto sample=[&](float x,float z,float& height){return Surface(a,2,2,10,20,2,x,z,height);};
assert(BubbleSurface(11,21,y,sample)&&y==80);assert(!BubbleSurface(11,21.9f,y,sample));
assert(BubbleSurface(13,23,y,sample)&&y==95);assert(!BubbleSurface(11.9f,22.1f,y,sample));
assert(Fresh((uint64_t(4873)<<1)|1,1000,1100,4873));assert(!Fresh((uint64_t(4873)<<1)|1,1000,1100,4872));
assert(!Fresh((uint64_t(4873)<<1)|1,1000,3001,4873));assert(!Fresh(uint64_t(4873)<<1,1000,1100,4873));}
