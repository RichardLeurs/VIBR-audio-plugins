// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// OD-2 hybrid drive: 12 drive types, one engine.
//   input -> noise gate -> high-pass -> mid emphasis (band-pass added) -> DRIVE gain (+ PRE boost)
//         -> 1-3 clipping stages -> low-pass -> clean blend          (this part runs oversampled)
//         -> LOW / MIDDLE / HIGH tone stack -> LEVEL (+ POST boost)  (host rate)
// PARAM is the "character" control: it morphs the clipping between soft (0) and hard (10) and moves the mid emphasis.
// Header-only, no JUCE dependency, no allocation in the process functions.
#include <algorithm>
#include <cmath>

namespace od2
{
constexpr float kPi = 3.14159265358979f;
enum Type { Overdrive, Blues, Scream, CntrOd, XDrive, Dist, XDist, StackDrive, FatDist, Brown, XMetal, Fuzz, kNumTypes };

struct TypeDef
{
    const char* name;        // printed round the selector
    const char* shortName;   // 3 characters for the display
    float hpHz;              // low cut before the gain
    float humpHz, humpDb;    // mid emphasis before clipping (band-pass added to the signal)
    float gainMinDb, gainMaxDb;   // DRIVE 0 .. 10
    int stages;              // clipping stages in series
    float hardness;          // 0 soft (tanh) .. 1 hard clip, at PARAM 5
    float asym;              // asymmetry (even harmonics)
    float postLpHz;          // low-pass after clipping
    float clean;             // clean (dry) blend after the clipping
    float trimDb;            // output trim so the types sit at similar loudness
    bool fuzz;               // asymmetric fuzz shaper
};

inline const TypeDef& typeDef (int t)
{
    static const TypeDef defs[kNumTypes] = {
        //  name           short   hp   humpHz dB   gMin gMax stg hard asym  postLp clean trim  fuzz
        { "OVERDRIVE",    "OD ",   90, 700,  5.0f,  0, 30, 1, 0.10f, 0.05f, 6500, 0.00f,  -3.1f, false },
        { "BLUES",        "BLU",   70, 500,  2.0f,  0, 24, 1, 0.00f, 0.12f, 6000, 0.15f,  0.5f, false },
        { "SCREAM",       "SCR",  150, 720, 10.0f,  4, 34, 1, 0.05f, 0.00f, 4800, 0.00f,  -6.5f, false },
        { "CNTR OD",      "CNT",   80, 900,  3.0f,  2, 28, 1, 0.15f, 0.08f, 7000, 0.35f,  -0.6f, false },
        { "X-DRIVE",      "XDR",  100, 800,  6.0f,  4, 36, 2, 0.20f, 0.06f, 5500, 0.00f, -9.8f, false },
        { "DIST",         "DST",  120, 1000, 3.0f,  8, 42, 1, 0.65f, 0.00f, 5200, 0.00f, -9.5f, false },
        { "X-DIST",       "XDS",  140, 1100, 4.0f, 10, 46, 2, 0.75f, 0.00f, 4800, 0.00f, -11.0f, false },
        { "STACK DRIVE",  "STK",  110, 750,  5.0f,  6, 40, 2, 0.35f, 0.04f, 5200, 0.00f, -10.4f, false },
        { "FAT DIST",     "FAT",   60, 400,  4.0f,  8, 42, 1, 0.70f, 0.00f, 4200, 0.00f, -10.2f, false },
        { "BROWN",        "BRN",  100, 650,  6.0f, 14, 50, 3, 0.45f, 0.10f, 4000, 0.00f, -11.2f, false },
        { "X-METAL",      "XMT",  240, 1500, 4.0f, 16, 54, 3, 0.85f, 0.00f, 4500, 0.00f, -10.8f, false },
        { "FUZZ",         "FUZ",   80, 600,  2.0f, 12, 46, 1, 0.80f, 0.35f, 5000, 0.00f, -8.1f, true  },
    };
    return defs[std::clamp (t, 0, (int) kNumTypes - 1)];
}

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct OnePole
{
    float z = 0.0f, a = 0.0f;
    void setCutoff (float hz, float sr) { a = std::exp (-2.0f * kPi * std::min (hz, 0.45f * sr) / sr); }
    float lp (float x) noexcept { z = x + a * (z - x); return z; }
    float hp (float x) noexcept { return x - lp (x); }
    void reset() noexcept { z = 0.0f; }
};

// state-variable filter, band-pass output
struct Svf
{
    float low = 0.0f, band = 0.0f, f = 0.1f, q = 0.5f;
    void set (float hz, float sr, float damping) { f = 2.0f * std::sin (kPi * std::min (hz, 0.2f * sr) / sr); q = damping; }
    float bandpass (float x) noexcept
    {
        low += f * band;
        const float high = x - low - q * band;
        band += f * high;
        return band;
    }
    void reset() noexcept { low = band = 0.0f; }
};

// one clipping stage: x is the pre-gained signal
inline float shape (float x, float hardness, float asym, bool fuzz) noexcept
{
    if (fuzz)
    {
        const float y = x > 0.0f ? 1.0f - std::exp (-x * 1.4f) : -0.85f * (1.0f - std::exp (x * 0.7f));
        return y * (0.7f + 0.3f * hardness) ;
    }
    const float xo = x + asym;
    const float soft = std::tanh (xo) - std::tanh (asym);
    const float hard = std::clamp (xo, -1.0f, 1.0f) - std::clamp (asym, -1.0f, 1.0f);
    return soft + (hard - soft) * hardness;
}

// the nonlinear part, one instance per channel, runs at the oversampled rate
struct DriveChannel
{
    OnePole hp, dc[3], lp, stageHp[3];
    Svf hump;
    int lastType = -1;
    float lastParam = -1.0f;
    float rate = 44100.0f;

    void prepare (float osRate) { rate = osRate; hp.reset(); lp.reset(); hump.reset(); for (auto& d : dc) d.reset(); for (auto& s : stageHp) s.reset(); lastType = -1; lastParam = -1.0f; }

    // call at block rate: set the filters for the type and PARAM (0..1)
    void configure (int type, float param01) noexcept
    {
        if (type == lastType && std::abs (param01 - lastParam) < 0.002f) return;
        lastType = type; lastParam = param01;
        const auto& d = typeDef (type);
        hp.setCutoff (d.hpHz, rate);
        hump.set (d.humpHz * std::pow (2.0f, (param01 - 0.5f) * 1.2f), rate, 0.5f);      // PARAM moves the emphasis up to +-0.6 octave
        lp.setCutoff (d.postLpHz, rate);
        for (auto& s : stageHp) s.setCutoff (60.0f, rate);
    }

    // gain = linear pre-gain (drive + boost), param01 = PARAM
    float process (float x, int type, float gain, float param01) noexcept
    {
        const auto& d = typeDef (type);
        float y = hp.hp (x);
        y += hump.bandpass (y) * (dbToGain (d.humpDb) - 1.0f);
        y *= gain;
        const float hardness = std::clamp (d.hardness + (param01 - 0.5f) * 0.8f, 0.0f, 1.0f);
        for (int s = 0; s < d.stages; ++s)
        {
            y = shape (y * (s == 0 ? 1.0f : 2.5f), hardness, d.asym, d.fuzz);
            y = stageHp[s].hp (y);                       // keeps asymmetric stages from piling up DC
        }
        y = lp.lp (y);
        return y * (1.0f - d.clean) + x * d.clean * 2.0f;      // clean blend
    }
};

// RBJ biquad, plain floats, set at block rate
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() noexcept { z1 = z2 = 0.0f; }
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2) { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }

    void lowShelf (float hz, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * hz / sr, c = std::cos (w), s = std::sin (w), al = s / 2.0f * std::sqrt (2.0f), tsa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + tsa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - tsa),
             (A + 1) + (A - 1) * c + tsa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - tsa);
    }
    void highShelf (float hz, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * hz / sr, c = std::cos (w), s = std::sin (w), al = s / 2.0f * std::sqrt (2.0f), tsa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + tsa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - tsa),
             (A + 1) - (A - 1) * c + tsa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - tsa);
    }
    void peak (float hz, float db, float q, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * hz / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

// LOW / MIDDLE / HIGH, each +-12 dB around 5
struct ToneStack
{
    Biquad low, mid, high;
    void set (float lowK, float midK, float highK, float sr)      // knobs 0..10
    {
        low.lowShelf (110.0f, (lowK - 5.0f) * 2.4f, sr);
        mid.peak (700.0f, (midK - 5.0f) * 2.4f, 0.8f, sr);
        high.highShelf (3200.0f, (highK - 5.0f) * 2.4f, sr);
    }
    float process (float x) noexcept { return high.process (mid.process (low.process (x))); }
    void reset() { low.reset(); mid.reset(); high.reset(); }
};

// noise gate: threshold 0..10 (0 = off), decay 0..10
struct Gate
{
    float env = 0.0f, gain = 1.0f;
    void reset() { env = 0.0f; gain = 1.0f; }
    // returns the gain to apply to x
    float process (float x, float thresh01, float decay01, float sr) noexcept
    {
        if (thresh01 <= 0.001f) { gain += (1.0f - gain) * 0.01f; return gain; }
        const float level = std::abs (x);
        env = level > env ? level : env + (level - env) * (1.0f - std::exp (-1.0f / (0.05f * sr)));     // fast attack, 50 ms release
        const float thr = dbToGain (-70.0f + 40.0f * thresh01);                                        // -70 .. -30 dB
        const float target = env > thr ? 1.0f : 0.0f;
        const float ms = target > gain ? 1.0f : 20.0f + 980.0f * decay01;                               // open fast, close in 20 ms .. 1 s
        gain += (target - gain) * (1.0f - std::exp (-1.0f / (0.001f * ms * sr)));
        return gain;
    }
};
} // namespace od2
