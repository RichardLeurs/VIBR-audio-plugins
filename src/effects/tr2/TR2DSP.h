// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Tremolo engine (TR-2 style): an LFO modulates the level.
//   RATE   0.5 .. 20 Hz
//   DEPTH  how far the level dips (0 = no effect, max = down to silence)
//   WAVE   sine (min) .. square (max): the sine is driven harder and harder into a limiter
#include <JuceHeader.h>
#include <cmath>

namespace tr2
{
inline float rateHz (float rate01) { return 0.5f * std::pow (40.0f, rate01); }              // 0.5 .. 20 Hz

// LFO value -1 .. 1 for phase 0 .. 1 and shape 0 (sine) .. 1 (square)
inline float lfo (float phase, float shape01)
{
    const float s = std::sin (juce::MathConstants<float>::twoPi * phase);
    const float k = 1.0f + 30.0f * shape01 * shape01;                                         // 1 = nearly a sine, 31 = nearly a square
    return std::tanh (k * s) / std::tanh (k);
}

// gain 1 - depth * (0.5 - 0.5 * lfo): between 1 and 1 - depth
inline float gain (float lfoValue, float depth01) { return 1.0f - depth01 * (0.5f - 0.5f * lfoValue); }
} // namespace tr2
