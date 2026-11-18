// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

namespace vibr::dsp
{
// One-pole DC blocker: y[n] = x[n] - x[n-1] + r * y[n-1]. r close to 1 = lower corner (0.995 is about 38 Hz at 48 kHz, 0.9995 about 4 Hz).
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f;
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float in, float r) noexcept { const float out = in - x1 + r * y1; x1 = in; y1 = out; return out; }
};
}
