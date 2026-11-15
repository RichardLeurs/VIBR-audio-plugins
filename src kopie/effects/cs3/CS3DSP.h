// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Compression sustainer engine (CS-3 style): a stereo-linked feed-forward compressor with a high ratio and automatic make-up gain.
//   SUSTAIN  lowers the threshold and raises the ratio (more compression, longer sustain), make-up gain follows
//   ATTACK   attack time (fast = squashed pick, slow = the pick attack comes through)
//   TONE     tilt: low shelf and high shelf in opposite directions round 1 kHz
//   LEVEL    output level
#include <JuceHeader.h>
#include <cmath>

namespace cs3
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

inline float attackMs (float a01)  { return 0.5f * std::pow (60.0f, a01); }            // 0.5 .. 30 ms
inline float releaseMs (float s01) { return 450.0f - 300.0f * s01; }                   // more sustain = faster release (150 .. 450 ms)
inline float thresholdDb (float s01) { return -8.0f - 42.0f * s01; }                    // -8 .. -50 dB
inline float ratioFor (float s01)    { return 3.0f + 17.0f * s01 * s01; }               // 3:1 .. 20:1
constexpr float kKneeDb = 10.0f;

// gain reduction in dB (<= 0) for an input level in dB, soft knee
inline float gainReductionDb (float levelDb, float thr, float ratio)
{
    const float over = levelDb - thr, slope = 1.0f / ratio - 1.0f;          // slope < 0
    if (over <= -0.5f * kKneeDb) return 0.0f;
    if (over >= 0.5f * kKneeDb) return slope * over;
    const float t = over + 0.5f * kKneeDb;
    return slope * t * t / (2.0f * kKneeDb);
}

// make-up so that a level of -20 dB goes through at roughly the same level, whatever the sustain setting
inline float makeupDb (float thr, float ratio) { return -gainReductionDb (-20.0f, thr, ratio) * 0.9f; }

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
} // namespace cs3
