// Player-selected atmosphere; independent of the game's seasonal weather packets.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace rvk {
enum class WeatherMode { Automatic, Clear, Overcast, Rain, Thunderstorm };
enum class FogMode { Automatic, Halloween, Mist, Custom };

struct SelectedWeather { bool forced = false; float clouds = 0, rain = 0, storm = 0; };
inline SelectedWeather SelectWeather(float mode, float amount)
{
    if (!std::isfinite(mode) || mode < 0.5f) return {};
    amount = std::isfinite(amount) ? std::clamp(amount, 0.05f, 1.0f) : 0.65f;
    switch (WeatherMode(int(std::clamp(std::round(mode), 1.0f, 4.0f)))) {
    case WeatherMode::Clear: return {true, 0, 0, 0};
    case WeatherMode::Overcast: return {true, 1, 0, 0};
    case WeatherMode::Rain: return {true, 0.85f, amount, 0};
    default: return {true, 1, amount, amount};
    }
}

inline const char* WeatherModeName(int mode)
{
    static constexpr const char* names[] = {"Automatic", "Clear", "Overcast", "Rain", "Thunderstorm"};
    return mode >= 0 && mode < 5 ? names[mode] : "Automatic";
}
inline const char* FogModeName(int mode)
{
    static constexpr const char* names[] = {"Automatic", "Halloween", "Mist", "Custom"};
    return mode >= 0 && mode < 4 ? names[mode] : "Automatic";
}

struct UserFogProfile {
    std::array<float, 3> colour{};
    float range = 0, clear = 0, shade = 0, drift = 0;
};
inline UserFogProfile MakeUserFog(float mode, float range, float clear, float shade, float drift,
                                 std::array<float, 3> colour, bool indoors, bool allowIndoors)
{
    UserFogProfile fog;
    if (!std::isfinite(mode) || mode < 0.5f || (indoors && !allowIndoors)) return fog;
    const int style = int(std::clamp(std::round(mode), 1.0f, 3.0f));
    fog.colour = style == int(FogMode::Halloween) ? std::array<float, 3>{0.006f, 0.004f, 0.01f} :
                 style == int(FogMode::Mist) ? std::array<float, 3>{0.42f, 0.47f, 0.52f} : colour;
    for (float& c : fog.colour) c = std::isfinite(c) ? std::clamp(c, 0.0f, 1.0f) : 0.0f;
    fog.range = std::isfinite(range) ? std::clamp(range, 5.0f, 500.0f) : 30.0f;
    fog.clear = std::isfinite(clear) ? std::clamp(clear, 0.0f, fog.range * 0.8f) : 3.0f;
    fog.shade = style == int(FogMode::Mist) || !std::isfinite(shade) ? 0.0f : std::clamp(shade, 0.0f, 1.0f);
    fog.drift = std::isfinite(drift) ? std::clamp(drift, 0.0f, 1.0f) : 0.0f;
    return fog;
}
} // namespace rvk
