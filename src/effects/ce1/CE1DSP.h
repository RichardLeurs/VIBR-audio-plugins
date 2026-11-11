// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// Delay line with 4-point Hermite interpolation, used by the chorus/vibrato (a BBD in the real CE-1).
namespace ce1
{
class ModDelay
{
public:
    void prepare (double sampleRate)
    {
        int n = 1;
        while (n < (int) (sampleRate * 0.1)) n <<= 1;   // 100 ms is plenty for chorus/vibrato
        buffer.assign ((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void write (float x) { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }

    float read (float delaySamples) const   // delaySamples >= 2
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
};

struct Channel
{
    ModDelay delay;
    float lp1 = 0.0f, lp2 = 0.0f;   // BBD-style reconstruction filter (2 poles)
    void reset() { delay.reset(); lp1 = lp2 = 0.0f; }
};
} // namespace ce1
