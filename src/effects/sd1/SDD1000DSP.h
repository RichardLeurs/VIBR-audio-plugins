// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace sdd1000
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
    float cut1 = 0.0f, cut2 = 0.0f;   // Hi-Cut: two one-pole low-passes in the regeneration path
    void reset() { delay.reset(); cut1 = cut2 = 0.0f; }
};

// TIME knob (0..10) -> milliseconds. Anchors follow the printed marks: 16 / 256 / 1024 / 2048 ms,
// with the REC/HOLD zone at the far end held at 2048 ms (geometric interpolation in between).
inline float timeKnobToMs (float k)
{
    struct A { float k, ms; };
    static constexpr A a[] = { { 0.0f, 16.0f }, { 3.0f, 256.0f }, { 6.0f, 1024.0f }, { 8.0f, 2048.0f }, { 10.0f, 2048.0f } };
    k = juce::jlimit (0.0f, 10.0f, k);
    for (int i = 0; i < 4; ++i)
        if (k <= a[i + 1].k)
        {
            const float t = (k - a[i].k) / (a[i + 1].k - a[i].k);
            return a[i].ms * std::pow (a[i + 1].ms / a[i].ms, t);
        }
    return 2048.0f;
}
} // namespace sdd1000
