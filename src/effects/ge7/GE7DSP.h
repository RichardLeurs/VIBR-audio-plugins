// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Graphic equalizer engine (GE-7 style): seven peaking bands (100, 200, 400, 800 Hz, 1.6, 3.2, 6.4 kHz), +-15 dB each,
// plus an output LEVEL slider (+-15 dB).
#include <JuceHeader.h>
#include <cmath>

namespace ge7
{
constexpr int kBands = 7;
constexpr float kFreq[kBands] = { 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f };
constexpr float kQ = 1.4f;                                    // one-octave spacing: neighbouring bands overlap a little

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// RBJ peaking biquad (transposed direct form II)
class Peak
{
public:
    void set (double sr, float freq, float gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * kQ);
        const double a0 = 1.0 + alpha / A;
        c0 = (float) ((1.0 + alpha * A) / a0); c1 = (float) (-2.0 * cw / a0); c2 = (float) ((1.0 - alpha * A) / a0);
        d1 = (float) (-2.0 * cw / a0); d2 = (float) ((1.0 - alpha / A) / a0);
    }
    void reset() { z1 = z2 = 0.0f; }
    float process (float x)
    {
        const float y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        return y;
    }
private:
    float c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
};
} // namespace ge7
