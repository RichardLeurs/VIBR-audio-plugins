// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace spaceecho
{
//==============================================================================
// Mode table: bits 0..2 = playback heads 1..3, reverb flag separate.
struct ModeInfo { int headMask; bool reverb; };

// NOTE: approximation of the RE-201 selector (see README). Index 0 = mode 1.
inline constexpr ModeInfo kModes[12] = {
    { 0b100, false },  //  1: H3
    { 0b010, false },  //  2: H2
    { 0b001, false },  //  3: H1
    { 0b110, false },  //  4: H2+H3
    { 0b101, false },  //  5: H1+H3
    { 0b011, false },  //  6: H1+H2
    { 0b111, false },  //  7: H1+H2+H3
    { 0b100, true  },  //  8: H3 + reverb
    { 0b101, true  },  //  9: H1+H3 + reverb
    { 0b011, true  },  // 10: H1+H2 + reverb
    { 0b111, true  },  // 11: all heads + reverb
    { 0b000, true  },  // 12: reverb only
};

//==============================================================================
// Circular delay line with 4-point Hermite interpolation (fractional delay).
class TapeDelayLine
{
public:
    void prepare (double sampleRate)
    {
        int n = 1;
        while (n < (int) (sampleRate * 1.2)) n <<= 1;
        buffer.assign ((size_t) n, 0.0f);
        mask = n - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void write (float x) { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }

    // delay in samples (>= 2)
    float read (float delay) const
    {
        const float pos = (float) writePos - 1.0f - delay;
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

//==============================================================================
// Schroeder allpass used as a dispersive element in the spring model.
class Allpass
{
public:
    void prepare (int length, float gain)
    {
        buf.assign ((size_t) juce::jmax (1, length), 0.0f);
        idx = 0;
        g = gain;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    float process (float x)
    {
        const float d = buf[(size_t) idx];
        const float v = x + g * d;
        buf[(size_t) idx] = v;
        if (++idx >= (int) buf.size()) idx = 0;
        return d - g * v;
    }

private:
    std::vector<float> buf;
    int idx = 0;
    float g = 0.6f;
};

//==============================================================================
// One spring: dispersive allpass chain inside a damped feedback loop.
class Spring
{
public:
    void prepare (double sampleRate, float loopSeconds, float feedbackGain)
    {
        static constexpr int baseLens[kStages] = { 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
        const float scale = (float) sampleRate / 44100.0f;
        for (int i = 0; i < kStages; ++i)
            stages[i].prepare ((int) std::round ((float) baseLens[i] * scale), 0.62f);

        loopLen = juce::jmax (8, (int) (loopSeconds * (float) sampleRate));
        loop.assign ((size_t) loopLen, 0.0f);
        pos = 0;
        fb = feedbackGain;
        lp = 0.0f;
        dampCoef = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 3200.0f / (float) sampleRate);
    }

    void reset()
    {
        for (auto& s : stages) s.reset();
        std::fill (loop.begin(), loop.end(), 0.0f);
        lp = 0.0f;
    }

    float process (float x)
    {
        const float delayed = loop[(size_t) pos];
        lp += dampCoef * (delayed - lp);                 // damping in the loop
        float y = x + fb * lp;
        for (auto& s : stages) y = s.process (y);        // dispersion -> "boing"
        loop[(size_t) pos] = y;
        if (++pos >= loopLen) pos = 0;
        return lp;
    }

private:
    static constexpr int kStages = 10;
    Allpass stages[kStages];
    std::vector<float> loop;
    int loopLen = 1, pos = 0;
    float fb = 0.8f, lp = 0.0f, dampCoef = 0.3f;
};

//==============================================================================
// Everything that lives per audio channel.
struct ChannelState
{
    TapeDelayLine tape;
    Spring springA, springB;
    juce::dsp::IIR::Filter<float> tapeHP, tapeLP, bass, treble, revHP, revLP;

    void prepare (double sr, bool right)
    {
        tape.prepare (sr);
        springA.prepare (sr, right ? 0.0335f : 0.0310f, 0.84f);
        springB.prepare (sr, right ? 0.0470f : 0.0430f, 0.82f);
        for (auto* f : { &tapeHP, &tapeLP, &bass, &treble, &revHP, &revLP })
            f->reset();
    }

    void reset()
    {
        tape.reset();
        springA.reset();
        springB.reset();
        for (auto* f : { &tapeHP, &tapeLP, &bass, &treble, &revHP, &revLP })
            f->reset();
    }
};

inline float softSat (float x) { return std::tanh (x); }
} // namespace spaceecho
