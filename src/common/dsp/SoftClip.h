// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once
#include <cmath>

namespace vibr::dsp
{
// Safety limiter for a wet signal: linear up to 0.9, then a tanh knee that approaches 1.0 and never passes it (SF-1, TF-1, IC-1).
inline float softClip (float x) noexcept
{
    const float a = std::fabs (x);
    return a <= 0.9f ? x : std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), x);
}
}
