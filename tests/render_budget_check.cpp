#include "render_budget.h"
#include "area_profiles.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace rvk::budget;
    assert(SmokeCells(128,0,8)==128);
    assert(SmokeCells(128,1.5f,8)==48);
    assert(SmokeCells(128,1.5f,50)==32);
    assert(SmokeCells(32,1.5f,8)==32);
    assert(SmokeScale(5,50,1)>5);
    assert(SmokeScale(8,0,1)>=.1f);
    assert(!MaterialCold(400,300,0));
    assert(!MaterialCold(400,1,390));
    assert(MaterialCold(400,1,100));
    const auto a=rvk::areas::AmbientLook(4310),b=rvk::areas::AmbientLook(4311);
    assert(a.colour[2]!=b.colour[2] && a.speed!=b.speed);
    std::puts("Render budgets: bounded smoke grid, uncapped calibration, active-map protection and regional ambience passed");
}
