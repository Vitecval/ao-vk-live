#pragma once
#include "d3d7.h"
#include <cmath>

namespace rvk::shadow {
// AO can change near/far clipping for nearby geometry without changing the
// world camera. Depth coefficients and subpixel projection jitter do not
// identify a separate UI camera.
inline bool SameLens(const d3d::Matrix& a,const d3d::Matrix& b) {
    for(int row=0;row<4;++row)for(int col=0;col<4;++col) {
        if((row==2&&(col==0||col==1||col==2))||(row==3&&col==2))continue;
        if(!std::isfinite(a.m[row][col])||!std::isfinite(b.m[row][col])||
            std::fabs(a.m[row][col]-b.m[row][col])>1e-5f)return false;
    }
    return true;
}
inline bool SameView(const d3d::Matrix& a,const d3d::Matrix& b) {
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)
        if(!std::isfinite(a.m[row][col])||!std::isfinite(b.m[row][col])||
            std::fabs(a.m[row][col]-b.m[row][col])>(row==3?.001f:.00001f))return false;
    return true;
}
}
