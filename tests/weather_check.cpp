#include "weather_options.h"
#include <cstdio>
#include <limits>

int main()
{
    int failures = 0, checks = 0;
    auto check = [&](bool ok, const char* message) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
    };
    using namespace rvk;
    check(!SelectWeather(0, 1).forced, "Automatic leaves the game's weather in control");
    const auto clear = SelectWeather(1, 1), overcast = SelectWeather(2, 1);
    const auto rain = SelectWeather(3, 0.4f), storm = SelectWeather(4, 0.6f);
    check(clear.forced && clear.rain == 0 && clear.storm == 0 && clear.clouds == 0, "Clear suppresses rain and storms");
    check(overcast.rain == 0 && overcast.storm == 0 && overcast.clouds == 1, "Overcast is cloudy without rain");
    check(rain.rain == 0.4f && rain.storm == 0 && rain.clouds > 0.8f, "Rain has clouds without forced lightning");
    check(storm.rain == 0.6f && storm.storm == 0.6f, "Thunderstorm uses selected strength");
    check(SelectWeather(4, 9).rain == 1, "Storm strength bounded");
    auto fog = [](float mode, bool indoors = false, bool allow = false) {
        return MakeUserFog(mode, 30, 3, 0.35f, 0.4f, {0.2f, 0.3f, 0.4f}, indoors, allow);
    };
    check(fog(0).range == 0, "Automatic adds no fog");
    const auto halloween = fog(1);
    check(halloween.range == 30 && halloween.clear == 3, "Halloween reference visibility and near clarity");
    check(halloween.colour[0] < 0.02f && halloween.colour[2] < 0.02f, "Halloween fog stays near black");
    check(fog(1, true).range == 0 && fog(1, true, true).range == 30, "Indoor opt-in");
    check(fog(2).shade == 0 && fog(2).colour[0] > 0.3f, "Mist is pale and leaves near lighting alone");
    check(fog(3).colour[1] == 0.3f, "Custom RGB respected");
    const auto bounded = MakeUserFog(3, 5, 20, 5, -1, {-1, 2, 0}, false, false);
    check(bounded.clear < bounded.range && bounded.colour[0] == 0 && bounded.colour[1] == 1, "Safe colour and distance bounds");
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    check(!SelectWeather(nan, 1).forced && MakeUserFog(nan, 30, 3, 0, 0, {}, false, false).range == 0, "Invalid modes disabled");
    std::printf("weather check: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
