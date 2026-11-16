#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace rvk::settings {
// Choice lists have an authored display order, which may run in either
// direction (render scale is 100, 85, 75, 67, 50).
inline float StepChoice(const std::vector<float>& choices,float value,int direction) {
    if(choices.empty())return value;
    const auto at=std::min_element(choices.begin(),choices.end(),[value](float a,float b){return std::fabs(a-value)<std::fabs(b-value);});
    const int next=std::clamp(int(at-choices.begin())+direction,0,int(choices.size())-1);
    return choices[size_t(next)];
}
}
