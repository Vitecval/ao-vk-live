#include "shadow_camera.h"
#include <cstdio>
int main(){
    rvk::d3d::Matrix a{},b{};a.m[0][0]=a.m[1][1]=1.7f;a.m[2][2]=1.0001f;a.m[2][3]=1;a.m[3][2]=-.1f;
    b=a;b.m[2][2]=1.01f;b.m[3][2]=-1;b.m[2][0]=.0002f;
    if(!rvk::shadow::SameLens(a,b))return 1;
    b.m[0][0]=2;if(rvk::shadow::SameLens(a,b))return 2;
    a={};for(int i=0;i<4;++i)a.m[i][i]=1;b=a;
    if(!rvk::shadow::SameView(a,b))return 3;
    b.m[3][0]=100;if(rvk::shadow::SameView(a,b))return 4;
    std::puts("PASS: world depth/jitter variants share a lens; different lenses and view cameras stay separate");return 0;
}
