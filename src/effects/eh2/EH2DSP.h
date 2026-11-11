// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Enhancer engine (EH-2 style): the treble above FREQ is picked off the input, brightened with a little soft clipping and
// added to (or, with the MIX knob left of centre, subtracted from) the dry signal. SENS sets the input level at which the
// effect opens up: the more sensitive, the quieter the playing that still gets enhanced.
//   MIX   -10 .. +10: INV (effect in opposite phase, a dip) .. 0 (off) .. NOR (a boost)
//   FREQ  corner of the treble band (700 Hz .. 7 kHz)
//   SENS  detector threshold (-8 dB .. -53 dB); the SENS lamp lights while the effect is open
#include <JuceHeader.h>
#include <cmath>

namespace eh2
{
inline float freqHz (float f01) { return 700.0f * std::pow (10.0f, f01); }                 // 700 Hz .. 7 kHz
inline float thresholdDb (float s01) { return -8.0f - 45.0f * s01; }

// second-order high-pass (RBJ, Q 0.707), transposed direct form II
class HighPass
{
public:
    void set (double sr, float freq)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr, cw = std::cos (w), alpha = std::sin (w) / (2.0 * 0.7071);
        const double a0 = 1.0 + alpha;
        c0 = (float) ((1.0 + cw) * 0.5 / a0); c1 = (float) (-(1.0 + cw) / a0); c2 = c0;
        d1 = (float) (-2.0 * cw / a0); d2 = (float) ((1.0 - alpha) / a0);
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
} // namespace eh2
