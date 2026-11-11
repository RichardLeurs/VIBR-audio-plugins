// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace sdd3000
{
// Delay line (up to ~2.3 s) with 4-point Hermite interpolation for the modulated read.
class DelayLine
{
public:
    void prepare (double sampleRate)
    {
        int n = 1;
        while (n < (int) (sampleRate * 2.4)) n <<= 1;
        buffer.assign ((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
        maxDelay = (float) (n - 8);
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    void write (float x) { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    float getMaxDelay() const { return maxDelay; }

    float read (float delaySamples) const   // 2 <= delaySamples <= maxDelay
    {
        const float pos = (float) writePos - 1.0f - delaySamples;
        const float fl = std::floor (pos);
        const float f = pos - fl;
        const int i1 = (int) fl;
        const float y0 = buffer[(size_t) ((i1 - 1) & mask)];
        const float y1 = buffer[(size_t) (i1 & mask)];
        const float y2 = buffer[(size_t) ((i1 + 1) & mask)];
        const float y3 = buffer[(size_t) ((i1 + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    float maxDelay = 0.0f;
};

struct Channel
{
    DelayLine delay;
    float lp1 = 0.0f, lp2 = 0.0f;   // regeneration high-cut (two one-poles)
    float hpLow = 0.0f;             // regeneration low-cut (one-pole high-pass = input minus its low-pass)
    void reset() { delay.reset(); lp1 = lp2 = hpLow = 0.0f; }
};

// Modulation waveforms (phase in radians): 0 sine, 1 triangle, 2 ramp up, 3 ramp down, 4 square (softened)
inline float lfoShape (float phase, int shape)
{
    const float t = phase / juce::MathConstants<float>::twoPi;   // 0..1
    switch (shape)
    {
        case 1:  return 4.0f * std::abs (t - 0.5f) - 1.0f;
        case 2:  return 2.0f * t - 1.0f;
        case 3:  return 1.0f - 2.0f * t;
        case 4:  return std::tanh (6.0f * std::sin (phase));
        default: return std::sin (phase);
    }
}
} // namespace sdd3000
