// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace memoryman
{
// Delay line (up to ~0.9 s) with 4-point Hermite interpolation for the modulated read.
class DelayLine
{
public:
    void prepare (double sampleRate)
    {
        int n = 1;
        while (n < (int) (sampleRate * 0.9)) n <<= 1;
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
    float lp1 = 0.0f, lp2 = 0.0f, lp3 = 0.0f;   // BBD reconstruction filter (3 poles; darker for longer delays)
    void reset() { delay.reset(); lp1 = lp2 = lp3 = 0.0f; }
};
} // namespace memoryman
