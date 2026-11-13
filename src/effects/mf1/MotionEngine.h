// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MF-1 Motion Filter: a resonant filter that is moved by the playing or by a pattern.
//   Filter types: LOW PASS, BAND PASS, HIGH PASS, NOTCH and VOWEL (formant filters: the cutoff position sweeps A - E - I - O - U)
//   Movement sources: LFO (sine - triangle - square - random), ENVELOPE (auto-wah: the playing opens the filter) and
//   STEPS (an 8-step sequencer with glide).  DEPTH sets how far (up to 5 octaves) the source moves the cutoff.
//   SYNC locks the LFO cycle / step length to the host tempo.
// Header-only, no JUCE dependency, no allocation.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mf1
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumTypes = 5, kNumSources = 3, kNumSteps = 8, kNumDivisions = 7;
enum Type { LowPass, BandPass, HighPass, Notch, Vowel };
enum Source { Lfo, Envelope, Steps };

inline const char* typeName (int t)
{
    static const char* n[kNumTypes] = { "LOW PASS", "BAND PASS", "HIGH PASS", "NOTCH", "VOWEL" };
    return n[std::clamp (t, 0, kNumTypes - 1)];
}
inline const char* sourceName (int s)
{
    static const char* n[kNumSources] = { "LFO", "ENVELOPE", "STEPS" };
    return n[std::clamp (s, 0, kNumSources - 1)];
}
inline const char* motionName (int source, int which)           // the two MOTION knobs are renamed per source
{
    static const char* a[kNumSources] = { "RATE", "SENS", "RATE" };
    static const char* b[kNumSources] = { "SHAPE", "DECAY", "GLIDE" };
    return (which == 0 ? a : b)[std::clamp (source, 0, kNumSources - 1)];
}
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline int divisionIndex (float v) { return std::clamp ((int) (v * (float) kNumDivisions), 0, kNumDivisions - 1); }
inline float divisionBeats (int i)      // length of one step in beats (a beat = a quarter note)
{
    static const float b[kNumDivisions] = { 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
inline const char* shapeName (float v)
{
    const int i = std::clamp ((int) (v * 4.0f), 0, 3);
    static const char* n[4] = { "SINE", "TRIANGLE", "SQUARE", "RANDOM" };
    return n[i];
}

inline float cutoffHz (float v) { return 60.0f * std::pow (200.0f, std::clamp (v, 0.0f, 1.0f)); }       // 60 Hz .. 12 kHz
inline float rateHz (float v) { return 0.1f * std::pow (100.0f, std::clamp (v, 0.0f, 1.0f)); }            // cycle: 0.1 .. 10 Hz
inline float decayMs (float v) { return 20.0f * std::pow (40.0f, std::clamp (v, 0.0f, 1.0f)); }         // 20 ms .. 800 ms
inline float sensGain (float v) { return std::pow (30.0f, std::clamp (v, 0.0f, 1.0f)); }                 // 1x .. 30x

struct Params
{
    int type = 1, source = 0;
    bool sync = false;
    float cutoff = 0.4f, res = 0.55f, depth = 0.6f, m1 = 0.35f, m2 = 0.0f;   // 0..1  (m1: rate / sens, m2: shape / decay / glide)
    float steps[kNumSteps] = { 0.2f, 0.8f, 0.4f, 1.0f, 0.3f, 0.7f, 0.5f, 0.9f };
    float bpm = 120.0f;
};

class Engine
{
public:
    void prepare (float sampleRate) { fs = sampleRate; reset(); }

    void reset()
    {
        for (auto& c : ch) c = Chan();
        phase = 0.0f; step = 0; env = 0.0f; stepVal = 0.5f; shRand = 0.0f; shPrev = 0; mod = 0.0f;
        dryG = 0.0f; wetG = 1.0f; rng = 0x2468ace1u; current = 0;
    }

    int currentStep() const { return current; }
    float lastModulation() const { return mod; }                     // -1 .. 1, for the editor

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0) { computeFilter (p); dryG += 0.3f * ((1.0f - mix) - dryG); wetG += 0.3f * (mix * levelGain - wetG); }
            const float x[2] = { l[i], r[i] };
            advanceSource (p, 0.5f * (x[0] + x[1]));
            float y[2];
            for (int c = 0; c < 2; ++c) y[c] = filter (ch[c], x[c], p);
            l[i] = x[0] * dryG + 1.5f * std::tanh (y[0] / 1.5f) * wetG;
            r[i] = x[1] * dryG + 1.5f * std::tanh (y[1] / 1.5f) * wetG;
        }
    }

private:
    struct Chan { float s1 = 0, s2 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0, c1 = 0, c2 = 0; };      // main SVF + two more for the vowel

    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float rand11() { return (float) (rnd() & 0xffffff) / 8388608.0f - 1.0f; }

    //---------------------------------------------------------------- movement
    void advanceSource (const Params& p, float xin)
    {
        const float cyc = p.sync ? 1.0f / (8.0f * divisionBeats (divisionIndex (p.m1)) * 60.0f / p.bpm) : rateHz (p.m1);   // cycles per second (8 steps)
        if (p.source == Envelope)
        {
            const float a = std::fabs (xin) * sensGain (p.m1);
            const float rel = 1.0f / (decayMs (p.m2) * 0.001f * fs);
            env = a > env ? env + (a - env) * (1.0f / (0.002f * fs)) : env + (a - env) * rel;
            mod = std::clamp (std::tanh (env * 3.0f), 0.0f, 1.0f);                           // 0 .. 1: closed when quiet, open when loud
            return;
        }
        phase += cyc / fs;
        if (phase >= 1.0f) phase -= 1.0f;
        if (p.source == Lfo)
        {
            const float sh = std::clamp (p.m2, 0.0f, 1.0f) * 3.0f;
            const int i = std::min (2, (int) sh);
            const float f = sh - (float) i;
            const float sine = std::sin (2.0f * kPi * phase);
            const float tri = 1.0f - 4.0f * std::fabs (phase - 0.5f);
            const float sq = std::tanh (6.0f * sine);
            const int q = (int) (phase * 4.0f);
            if (q != shPrev) { shRand = rand11(); shPrev = q; }
            shSm += (shRand - shSm) * (1.0f / (0.004f * fs));
            const float v[4] = { sine, tri, sq, shSm };
            mod = v[i] + (v[i + 1] - v[i]) * f;
            return;
        }
        // steps: 8 steps per cycle, glide = the share of a step used to slide to the next value
        const float pos = phase * (float) kNumSteps;
        const int s = std::clamp ((int) pos, 0, kNumSteps - 1);
        if (s != step) { step = s; current = s; }
        const float target = p.steps[step] * 2.0f - 1.0f;
        const float stepSec = 1.0f / (cyc * (float) kNumSteps);
        const float glideSec = std::max (0.0005f, p.m2 * p.m2 * stepSec * 1.2f);
        stepVal += (target - stepVal) * (1.0f / (glideSec * fs));
        mod = stepVal;
    }

    //---------------------------------------------------------------- filter
    void computeFilter (const Params& p)
    {
        const float oct = mod * p.depth * 5.0f;      // octaves of movement
        const float pos = std::clamp (p.cutoff, 0.0f, 1.0f);
        float fc = cutoffHz (pos) * std::pow (2.0f, oct);
        fc = std::clamp (fc, 30.0f, 0.45f * fs);
        g = std::tan (kPi * fc / fs);
        k = 2.0f - 1.96f * std::clamp (p.res, 0.0f, 1.0f);
        if (p.type == Vowel)
        {
            // position A .. U from the (moved) cutoff, on a log scale over the knob range
            const float vp = std::clamp (std::log (fc / 60.0f) / std::log (200.0f), 0.0f, 1.0f) * 4.0f;
            static const float f1[5] = { 800, 400, 350, 450, 325 }, f2[5] = { 1150, 1600, 2300, 800, 700 };
            const int i0 = std::min (3, (int) vp);
            const float fr = vp - (float) i0;
            const float a = f1[i0] + fr * (f1[i0 + 1] - f1[i0]), b = f2[i0] + fr * (f2[i0 + 1] - f2[i0]);
            gv1 = std::tan (kPi * a / fs);
            gv2 = std::tan (kPi * b / fs);
            gv3 = std::tan (kPi * 2800.0f / fs);
            kv = 1.0f / (3.0f + 11.0f * std::clamp (p.res, 0.0f, 1.0f));
        }
    }

    // TPT state-variable filter: returns the low, band and high outputs
    static void svf (float& s1, float& s2, float x, float g, float k, float& lo, float& bp, float& hp)
    {
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp;
        bp = v1 + s1; s1 = bp + v1;
        const float v2 = g * bp;
        lo = v2 + s2; s2 = lo + v2;
    }

    float filter (Chan& c, float x, const Params& p)
    {
        float lo, bp, hp;
        switch (p.type)
        {
            case LowPass:  svf (c.s1, c.s2, x, g, k, lo, bp, hp); return lo;
            case BandPass: svf (c.s1, c.s2, x, g, k, lo, bp, hp); return bp * k * (1.0f + 0.5f * (2.0f - k));
            case HighPass: svf (c.s1, c.s2, x, g, k, lo, bp, hp); return hp;
            case Notch:    svf (c.s1, c.s2, x, g, k, lo, bp, hp); return lo + hp;
            default:
            {
                float o1, o2, o3, h;
                svf (c.s1, c.s2, x, gv1, kv, lo, o1, h);
                svf (c.a1, c.a2, x, gv2, kv, lo, o2, h);
                svf (c.b1, c.b2, x, gv3, kv, lo, o3, h);
                return 3.0f * kv * (1.4f * o1 + o2 + 0.4f * o3) * 2.0f;
            }
        }
    }

    float fs = 48000.0f;
    Chan ch[2];
    float g = 0.1f, k = 1.0f, gv1 = 0.1f, gv2 = 0.1f, gv3 = 0.1f, kv = 0.2f;
    float phase = 0.0f, env = 0.0f, stepVal = 0.5f, shRand = 0.0f, shSm = 0.0f, mod = 0.0f, dryG = 0.0f, wetG = 1.0f;
    int step = 0, shPrev = 0, current = 0;
    uint32_t rng = 0x2468ace1u;
};
} // namespace mf1
