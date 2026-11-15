// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Flanger engine (BF-3 style): a short modulated delay with feedback, mixed with the dry signal.
//   MANUAL  centre of the sweep (0.25 .. 5 ms)
//   DEPTH   how far the delay sweeps round the centre (in octaves)
//   RATE    triangle LFO speed (0.05 .. 8 Hz); with SYNC on, one LFO cycle per note division of the host tempo (same range)
//   RES     feedback (resonance)
//   MODE    0 GATE/PAN: the flange is panned between the two outputs (A / B)
//           1 STANDARD: normal flanger, left / right LFOs 90 degrees apart
//           2 ULTRA:    deeper sweep and stronger feedback
#include <JuceHeader.h>
#include <cmath>

namespace bf3
{
constexpr int kModeGatePan = 0, kModeStandard = 1, kModeUltra = 2;

inline float centreMs (float manual01) { return 0.25f * std::pow (20.0f, manual01); }           // 0.25 .. 5 ms
inline float rateHz (float rate01)     { return 0.05f * std::pow (160.0f, rate01); }            // 0.05 .. 8 Hz
constexpr float kRateMinHz = 0.05f, kRateMaxHz = 8.0f;
// inverse of rateHz (tempo sync): the knob position 0..1 that gives `hz`, clamped to the LFO range. The processor smooths this
// value, so a tempo change glides along the same exponential curve as turning the RATE knob.
inline float rate01ForHz (float hz)
{
    return std::log (juce::jlimit (kRateMinHz, kRateMaxHz, hz) / kRateMinHz) / std::log (kRateMaxHz / kRateMinHz);
}
inline float sweepOctaves (float depth01, int mode) { return depth01 * (mode == kModeUltra ? 3.0f : 2.0f); }
inline float feedbackAmount (float res01, int mode) { return res01 * (mode == kModeUltra ? 0.97f : 0.85f); }

// triangle LFO, phase 0..1 -> -1..1
inline float triangle (float phase)
{
    phase -= std::floor (phase);
    return 4.0f * std::fabs (phase - 0.5f) - 1.0f;
}

class FlangerChannel
{
public:
    void prepare (double sampleRate)
    {
        int n = 64;
        const int need = (int) (0.030 * sampleRate) + 8;          // 30 ms of room
        while (n < need) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        reset();
    }

    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    // delaySamples >= 2. Returns the delayed (wet) sample and feeds it back.
    float process (float x, float delaySamples, float feedback)
    {
        const float pos = (float) w - delaySamples;
        const float fl = std::floor (pos);
        const float f = pos - fl;
        const int i1 = (int) fl;
        const float y0 = buf[(size_t) ((i1 - 1) & mask)], y1 = buf[(size_t) (i1 & mask)],
                    y2 = buf[(size_t) ((i1 + 1) & mask)], y3 = buf[(size_t) ((i1 + 2) & mask)];
        // 4-point Hermite interpolation
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const float wet = ((c3 * f + c2) * f + c1) * f + c0;
        buf[(size_t) (w & mask)] = x + std::tanh (feedback * wet);      // soft-clipped feedback keeps it stable
        w = (w + 1) & mask;
        return wet;
    }

private:
    std::vector<float> buf;
    int mask = 63, w = 0;
};
} // namespace bf3
