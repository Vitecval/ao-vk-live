#include "day_cycle.h"
#include <cstdio>
#include <cmath>
#include <initializer_list>
int main() {
    constexpr double day=27.0*3600.0;
    double previous=-1;
    unsigned daylight=0;
    for(int i=0;i<100000;++i) {
        const double t=day*i/100000.0, shown=rvk::daycycle::LongerDay(t,day);
        if(!std::isfinite(shown) || shown<0 || shown>=day || shown<previous) return 1;
        previous=shown;
        daylight+=shown>=day*.25 && shown<day*.75;
    }
    if(daylight<65000 || daylight>71000) return 2;
    for(double t:{0.0,day*.5,day})
        if(std::abs(rvk::daycycle::LongerDay(t,day)-std::fmod(t,day))>1e-6) return 3;
    if(std::abs(rvk::daycycle::LongerDay(day*2.25,day)-rvk::daycycle::LongerDay(day*.25,day))>1e-6) return 4;
    std::printf("27-hour local cycle: monotonic, periodic, noon/midnight fixed; middle-half share %.2f%%\n",daylight/1000.0);
}
