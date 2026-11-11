// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Limiter engine (LM-2 style): a stereo-linked peak limiter with a high ratio and a fast attack.
//   THRESHOLD  lowers the ceiling (-4 .. -40 dB); the make-up gain follows part of it
//   RELEASE    recovery time (40 ms .. 1.2 s)
//   TONE       tilt: low shelf and high shelf in opposite directions round 1 kHz
//   LEVEL      output level
#include <JuceHeader.h>
#include <cmath>

namespace lm2
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

inline float releaseMs (float r01)    { return 40.0f * std::pow (30.0f, r01); }          // 40 .. 1200 ms
inline float thresholdDb (float t01)  { return -4.0f - 36.0f * t01; }                    // -4 .. -40 dB
constexpr float kRatio = 20.0f, kKneeDb = 4.0f;

// gain reduction in dB (<= 0) for an input level in dB, soft knee
inline float gainReductionDb (float levelDb, float thr)
{
    const float over = levelDb - thr, slope = 1.0f / kRatio - 1.0f;
    if (over <= -0.5f * kKneeDb) return 0.0f;
    if (over >= 0.5f * kKneeDb) return slope * over;
    const float t = over + 0.5f * kKneeDb;
    return slope * t * t / (2.0f * kKneeDb);
}

inline float makeupDb (float thr) { return -0.5f * thr - 2.0f; }                           // lowering the threshold brings the level back up

// RBJ shelving biquad
class Shelf
{
public:
    void set (double sr, float freq, float gainDb, bool high)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * 0.7071);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        double b0, b1, b2, a0, a1, a2;
        if (high)
        {
            b0 = A * ((A + 1) + (A - 1) * cw + sq);  b1 = -2 * A * ((A - 1) + (A + 1) * cw);  b2 = A * ((A + 1) + (A - 1) * cw - sq);
            a0 = (A + 1) - (A - 1) * cw + sq;        a1 = 2 * ((A - 1) - (A + 1) * cw);       a2 = (A + 1) - (A - 1) * cw - sq;
        }
        else
        {
            b0 = A * ((A + 1) - (A - 1) * cw + sq);  b1 = 2 * A * ((A - 1) - (A + 1) * cw);   b2 = A * ((A + 1) - (A - 1) * cw - sq);
            a0 = (A + 1) + (A - 1) * cw + sq;        a1 = -2 * ((A - 1) + (A + 1) * cw);      a2 = (A + 1) + (A - 1) * cw - sq;
        }
        c0 = (float) (b0 / a0); c1 = (float) (b1 / a0); c2 = (float) (b2 / a0); d1 = (float) (a1 / a0); d2 = (float) (a2 / a0);
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
} // namespace lm2
