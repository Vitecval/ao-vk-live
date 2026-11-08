#pragma once
#include <cmath>
namespace rvk::daycycle {
// Smooth periodic warp of the local sky clock. Midnight/noon and the full day
// length stay fixed; its middle half occupies about two thirds of real time.
// Positive derivative everywhere: no stopped or backwards sunsets.
inline double LongerDay(double seconds, double day)
{
    if (!std::isfinite(seconds) || !std::isfinite(day) || day <= 0.0) return seconds;
    const double phase = std::fmod(std::fmod(seconds, day) + day, day) / day;
    return (phase + 0.11 * std::sin(6.283185307179586 * phase)) * day;
}
}
