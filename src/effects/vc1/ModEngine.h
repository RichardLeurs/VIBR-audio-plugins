// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// VC-1 Voltage Controller: a modulation matrix on a small signal path (vibrato, filter, amp, pan).
//   Sources: ENV (an envelope follower on the playing), LFO 1, LFO 2, RANDOM (stepped random with a little slew) and MIDI (mod wheel /
//   expression / pressure).  Destinations: FILTER (cutoff, up to 5 octaves), AMP (level), PITCH (vibrato depth, up to 6 ms of delay)
//   and PAN.  Every crossing in the 5 x 4 matrix is a depth from -100 % to +100 %.
//   Cross-modulation: X-MOD lets LFO 2 move the rate of LFO 1 (FM of the modulators), and the envelope also nudges both LFO rates.
// Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vc1
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumSources = 5, kNumDests = 4, kNumTypes = 3, kNumShapes = 4;
enum Source { Env, Lfo1, Lfo2, Random, Midi };
enum Dest { Filter, Amp, Pitch, Pan };
enum Type { LowPass, BandPass, HighPass };

inline const char* sourceName (int s) { static const char* n[kNumSources] = { "ENVELOPE", "LFO 1", "LFO 2", "RANDOM", "MIDI" }; return n[std::clamp (s, 0, kNumSources - 1)]; }
inline const char* destName (int d) { static const char* n[kNumDests] = { "FILTER", "AMP", "PITCH", "PAN" }; return n[std::clamp (d, 0, kNumDests - 1)]; }
inline const char* typeName (int t) { static const char* n[kNumTypes] = { "LOW PASS", "BAND PASS", "HIGH PASS" }; return n[std::clamp (t, 0, kNumTypes - 1)]; }
inline const char* shapeName (int s) { static const char* n[kNumShapes] = { "SINE", "TRIANGLE", "SQUARE", "RAMP" }; return n[std::clamp (s, 0, kNumShapes - 1)]; }

inline float cutoffHz (float v) { return 60.0f * std::pow (200.0f, std::clamp (v, 0.0f, 1.0f)); }        // 60 Hz .. 12 kHz
inline float lfoHz (float v) { return 0.05f * std::pow (400.0f, std::clamp (v, 0.0f, 1.0f)); }            // 0.05 .. 20 Hz
inline float randomHz (float v) { return 0.5f * std::pow (40.0f, std::clamp (v, 0.0f, 1.0f)); }           // 0.5 .. 20 steps per second

// tempo sync: with SYNC on, the three rate knobs (LFO 1, LFO 2, RANDOM) pick one of these cycle lengths of the host tempo
constexpr int kNumDivisions = 10;
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionHz (int i, float bpm)         // cycles per second: one cycle is that many beats (a beat = a quarter note)
{
    static const float beats[kNumDivisions] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
    return std::max (20.0f, bpm) / 60.0f / beats[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float releaseMs (float v) { return 20.0f * std::pow (50.0f, std::clamp (v, 0.0f, 1.0f)); }        // 20 ms .. 1 s
inline float sensGain (float v) { return std::pow (60.0f, std::clamp (v, 0.0f, 1.0f)); }                 // 1x .. 60x

struct Params
{
    int type = 0, shape1 = 0, shape2 = 1;
    bool sync = false;                               // the rate knobs follow the host tempo
    float bpm = 120.0f;
    float cutoff = 0.5f, res = 0.4f, mix = 1.0f, envSens = 0.5f, envRel = 0.4f, rate1 = 0.45f, rate2 = 0.3f, randRate = 0.4f, xmod = 0.0f;
    float matrix[kNumSources][kNumDests] = {};       // -1 .. 1
    float midi = 0.0f;                               // 0 .. 1 (mod wheel, expression, pressure)
};

class Engine
{
public:
    std::atomic<float> sourceVis[kNumSources];       // for the editor: the sources now (-1 .. 1)

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        size = 1; while ((float) size < 0.05f * fs) size <<= 1;
        mask = size - 1;
        for (auto& c : ch) c.buf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& c : ch) { std::fill (c.buf.begin(), c.buf.end(), 0.0f); c.w = 0; c.s1 = c.s2 = 0.0f; }
        p1 = p2 = 0.0f; env = 0.0f; rTarget = rValue = 0.0f; rClock = 0.0f; midiSm = 0.0f; delaySm = 0.008f * fs;
        dryG = 0.0f; wetG = 1.0f; rng = 0x13579bdfu; vis = 0;
        for (auto& v : sourceVis) v.store (0.0f);
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        for (int i = 0; i < n; ++i)
        {
            const float x[2] = { l[i], r[i] };
            float src[kNumSources];
            sources (p, 0.5f * (x[0] + x[1]), src);
            float sum[kNumDests] = { 0, 0, 0, 0 };
            for (int s = 0; s < kNumSources; ++s)
                for (int d = 0; d < kNumDests; ++d) sum[d] += p.matrix[s][d] * src[s];

            if ((i & 7) == 0) setFilter (p, sum[Filter]);
            const float targetDelay = std::max (0.0005f * fs, (0.008f + 0.006f * std::clamp (sum[Pitch], -1.2f, 1.2f)) * fs);
            delaySm += (targetDelay - delaySm) * 0.2f;
            const float gain = std::clamp (1.0f + sum[Amp], 0.0f, 3.0f);
            const float pan = std::clamp (sum[Pan], -1.0f, 1.0f);
            const float gl = std::cos ((pan + 1.0f) * kPi * 0.25f) * 1.4142f, gr = std::sin ((pan + 1.0f) * kPi * 0.25f) * 1.4142f;

            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                Chan& s = ch[c];
                s.buf[(size_t) (s.w & mask)] = x[c];
                const float d = std::clamp (delaySm, 2.0f, (float) (size - 4));
                const int di = (int) d;
                const float f = d - (float) di;
                const float a = s.buf[(size_t) ((s.w - di) & mask)], b = s.buf[(size_t) ((s.w - di - 1) & mask)];
                ++s.w;
                float v = a + (b - a) * f;                                           // vibrato delay
                float lo, bp, hp;
                svf (s.s1, s.s2, v, g, k, lo, bp, hp);
                v = p.type == LowPass ? lo : p.type == BandPass ? bp * k * 1.3f : hp;
                y[c] = v * gain * (c == 0 ? gl : gr);
            }
            dryG += 0.001f * ((1.0f - p.mix) - dryG);
            l[i] = x[0] * dryG + 1.5f * std::tanh (y[0] / 1.5f) * p.mix * levelGain;
            r[i] = x[1] * dryG + 1.5f * std::tanh (y[1] / 1.5f) * p.mix * levelGain;
            if ((++vis & 1023) == 0) for (int s = 0; s < kNumSources; ++s) sourceVis[s].store (src[s], std::memory_order_relaxed);
        }
    }

private:
    struct Chan { std::vector<float> buf; int64_t w = 0; float s1 = 0, s2 = 0; };

    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float rand11() { return (float) (rnd() & 0xffffff) / 8388608.0f - 1.0f; }

    static float wave (int shape, float ph)
    {
        switch (shape)
        {
            case 1:  return 1.0f - 4.0f * std::fabs (ph - 0.5f);
            case 2:  return std::tanh (8.0f * std::sin (2.0f * kPi * ph));
            case 3:  return 2.0f * ph - 1.0f;
            default: return std::sin (2.0f * kPi * ph);
        }
    }

    void sources (const Params& p, float xin, float* src)
    {
        // envelope follower
        const float a = std::fabs (xin) * sensGain (p.envSens);
        env += a > env ? (a - env) * (1.0f / (0.003f * fs)) : (a - env) * (1.0f / (releaseMs (p.envRel) * 0.001f * fs));
        const float e = std::clamp (std::tanh (env * 3.0f), 0.0f, 1.0f);
        // LFO 2 (free), LFO 1 (its rate moved by LFO 2 and the envelope)
        p2 += (p.sync ? divisionHz (divisionIndex (p.rate2), p.bpm) : lfoHz (p.rate2)) / fs; if (p2 >= 1.0f) p2 -= 1.0f;
        const float l2 = wave (p.shape2, p2);
        const float rate1 = (p.sync ? divisionHz (divisionIndex (p.rate1), p.bpm) : lfoHz (p.rate1)) * std::pow (2.0f, p.xmod * l2 * 3.0f + e * p.xmod * 1.5f);
        p1 += std::clamp (rate1, 0.01f, 40.0f) / fs; if (p1 >= 1.0f) p1 -= 1.0f;
        const float l1 = wave (p.shape1, p1);
        // random: a new value every step, glided most of the way
        const float randHz = p.sync ? divisionHz (divisionIndex (p.randRate), p.bpm) : randomHz (p.randRate);
        rClock += randHz / fs;
        if (rClock >= 1.0f) { rClock -= 1.0f; rTarget = rand11(); }
        rValue += (rTarget - rValue) * (1.0f / (0.25f / randHz * fs));
        midiSm += (p.midi - midiSm) * (1.0f / (0.02f * fs));
        src[Env] = e; src[Lfo1] = l1; src[Lfo2] = l2; src[Random] = rValue; src[Midi] = midiSm;
    }

    static void svf (float& s1, float& s2, float x, float g, float k, float& lo, float& bp, float& hp)
    {
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp;
        bp = v1 + s1; s1 = bp + v1;
        const float v2 = g * bp;
        lo = v2 + s2; s2 = lo + v2;
    }

    void setFilter (const Params& p, float modSum)
    {
        const float fc = std::clamp (cutoffHz (p.cutoff) * std::pow (2.0f, std::clamp (modSum, -1.5f, 1.5f) * 5.0f), 30.0f, 0.45f * fs);
        g = std::tan (kPi * fc / fs);
        k = 2.0f - 1.96f * std::clamp (p.res, 0.0f, 1.0f);
    }

    float fs = 48000.0f, p1 = 0, p2 = 0, env = 0, rTarget = 0, rValue = 0, rClock = 0, midiSm = 0, delaySm = 384.0f, g = 0.1f, k = 1.0f, dryG = 0.0f, wetG = 1.0f;
    int size = 1, mask = 0, vis = 0;
    uint32_t rng = 0x13579bdfu;
    Chan ch[2];
};
} // namespace vc1
