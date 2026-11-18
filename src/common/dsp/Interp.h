// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

namespace vibr::dsp
{
// Interpolation between delay-line samples. f = 0 .. 1 is the fraction between y0 and y1.

// 4-point Hermite (the modulated reads of DL-1 and ML-1): ym1, y0, y1, y2 are the samples around the read position.
inline float hermite4 (float ym1, float y0, float y1, float y2, float f) noexcept
{
    const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

// linear (TF-1 and IC-1 ring reads, the rotary of ML-1)
inline float lerp (float a, float b, float f) noexcept { return a + (b - a) * f; }
}
