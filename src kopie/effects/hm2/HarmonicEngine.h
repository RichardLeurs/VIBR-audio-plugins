// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// HM-2 Harmonic Machine: adds and reshapes harmonics.
//   The signal is split at FOCUS into a low and a high band; each band is driven into the saturator (TILT moves the drive between
//   the bands: frequency-dependent distortion).  CHARACTER picks the saturator: TUBE (soft), TAPE (gentle, round), TRANSISTOR
//   (hard, tight) or FOLD (wave folder).  ODD / EVEN biases the saturator: symmetric = odd harmonics (3rd, 5th ...), asymmetric
//   = even harmonics (2nd, 4th ...).  OCTAVE adds an octave up (full-wave rectifier), SUB an octave down (flip-flop divider).
//   TONE rounds off the result.  harmonicSpectrum() computes the harmonics of the current setting for the editor's display.
// Header-only, no JUCE dependency, no allocation.
#include <algorithm>
#include <cmath>

namespace hm2
{
constexpr float kPi = 3.14159265358979f;
constexpr int kNumCharacters = 4;
enum Character { Tube, Tape, Transistor, Fold };

inline const char* characterName (int c)
{
    static const char* n[kNumCharacters] = { "TUBE", "TAPE", "TRANSISTOR", "FOLD" };
    return n[std::clamp (c, 0, kNumCharacters - 1)];
}

inline float focusHz (float v) { return 200.0f * std::pow (20.0f, std::clamp (v, 0.0f, 1.0f)); }       // 200 Hz .. 4 kHz
inline float toneHz (float v) { return 1500.0f * std::pow (13.0f, std::clamp (v, 0.0f, 1.0f)); }       // 1.5 kHz .. 19.5 kHz
inline float driveDb (float v) { return 36.0f * std::clamp (v, 0.0f, 1.0f); }
inline float tiltValue (float v) { return (std::clamp (v, 0.0f, 1.0f) - 0.5f) * 2.0f; }                  // -1 (low band driven) .. +1 (high band)

struct Params
{
    int character = 0;
    float drive = 0.4f, oddEven = 0.5f, octave = 0.0f, sub = 0.0f, focus = 0.4f, tilt = 0.5f, tone = 0.8f;   // 0..1  (oddEven: 0 even .. 1 odd)
};

inline float saturate (float u, int c)
{
    switch (c)
    {
        case Tape:       return (2.0f / kPi) * std::atan (u * kPi * 0.5f);
        case Transistor: { const float a = std::fabs (u), a2 = a * a; return u / std::pow (1.0f + a2 * a2, 0.25f); }
        case Fold:       return std::sin (u);
        default:         return std::tanh (u);
    }
}

// saturator with bias (asymmetry): the output has no DC and the same small-signal gain as the unbiased one
inline float saturateBiased (float u, int c, float bias)
{
    const float d = (saturate (bias + 0.01f, c) - saturate (bias - 0.01f, c)) / 0.02f;
    return (saturate (u + bias, c) - saturate (bias, c)) / std::max (0.25f, d);
}
inline float biasOf (const Params& p) { return (1.0f - p.oddEven) * 0.9f; }

// amplitude of the harmonics 2 .. 9 (dB relative to the fundamental) for the editor
inline void harmonicSpectrum (const Params& p, float outDb[8])
{
    constexpr int N = 256;
    const float amp = 0.3f * std::pow (10.0f, driveDb (p.drive) / 20.0f);
    float y[N];
    for (int i = 0; i < N; ++i) y[i] = saturateBiased (amp * std::sin (2.0f * kPi * (float) i / (float) N), p.character, biasOf (p));
    float mag[10];
    for (int k = 1; k <= 9; ++k)
    {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < N; ++i) { const float a = 2.0f * kPi * (float) (k * i) / (float) N; re += y[i] * std::cos (a); im += y[i] * std::sin (a); }
        mag[k] = std::sqrt (re * re + im * im) * 2.0f / (float) N;
    }
    for (int k = 2; k <= 9; ++k) outDb[k - 2] = 20.0f * std::log10 (std::max (1.0e-6f, mag[k] / std::max (1.0e-6f, mag[1])));
}

class Engine
{
public:
    void prepare (float sampleRate) { fs = sampleRate; reset(); }

    void reset()
    {
        for (auto& c : ch) c = Chan();
        oLp1 = oLp2 = oHp = 0.0f; sLp1 = sLp2 = sEnv = sSm = 0.0f; sState = 1.0f; sPrev = 0.0f; octDc = 0.0f; octLp = 0.0f;
        dryG = 0.0f; wetG = 1.0f;
    }

    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const float bias = biasOf (p);
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0) control (p, mix, levelGain);
            const float x[2] = { l[i], r[i] };
            const float mid = 0.5f * (x[0] + x[1]);
            const float extra = octaveUp (mid) * p.octave * 1.0f + subOctave (mid) * p.sub * 0.5f;
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                Chan& s = ch[c];
                float lo, bp, hp;
                svf (s.s1, s.s2, x[c], gf, 1.4142f, lo, bp, hp);                // low band; the high band is the rest
                const float high = x[c] - lo;
                float v = saturateBiased (lo * gl, p.character, bias) + saturateBiased (high * gh, p.character, bias);
                const float dc = v - s.dcX + 0.995f * s.dcY; s.dcX = v; s.dcY = dc; v = dc;        // DC blocker
                v += extra;
                s.tone += (v - s.tone) * toneA;                                  // tone: one-pole low pass
                y[c] = s.tone;
            }
            l[i] = x[0] * dryG + 1.5f * std::tanh (y[0] / 1.5f) * wetG;
            r[i] = x[1] * dryG + 1.5f * std::tanh (y[1] / 1.5f) * wetG;
        }
    }

private:
    struct Chan { float s1 = 0, s2 = 0, dcX = 0, dcY = 0, tone = 0; };

    static void svf (float& s1, float& s2, float x, float g, float k, float& lo, float& bp, float& hp)
    {
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp;
        bp = v1 + s1; s1 = bp + v1;
        const float v2 = g * bp;
        lo = v2 + s2; s2 = lo + v2;
    }

    void control (const Params& p, float mix, float levelGain)
    {
        const float D = driveDb (p.drive), t = tiltValue (p.tilt);
        gl = std::pow (10.0f, std::clamp (D * (1.0f - 0.8f * t), 0.0f, 45.0f) / 20.0f);
        gh = std::pow (10.0f, std::clamp (D * (1.0f + 0.8f * t), 0.0f, 45.0f) / 20.0f);
        gf = std::tan (kPi * std::min (focusHz (p.focus), 0.45f * fs) / fs);
        toneA = 1.0f - std::exp (-2.0f * kPi * std::min (toneHz (p.tone), 0.45f * fs) / fs);
        dryG += 0.3f * ((1.0f - mix) - dryG);
        wetG += 0.3f * (mix * levelGain - wetG);
    }

    // octave up: the band-limited signal full-wave rectified, DC removed
    float octaveUp (float x)
    {
        const float g1 = 1.0f - std::exp (-2.0f * kPi * 1500.0f / fs);
        oLp1 += (x - oLp1) * g1; oLp2 += (oLp1 - oLp2) * g1;
        oHp += (oLp2 - oHp) * (1.0f - std::exp (-2.0f * kPi * 80.0f / fs));
        const float band = oLp2 - oHp;
        const float rect = std::fabs (band) * 2.0f;
        octDc += (rect - octDc) * (1.0f - std::exp (-2.0f * kPi * 40.0f / fs));
        octLp += (rect - octDc - octLp) * (1.0f - std::exp (-2.0f * kPi * 4000.0f / fs));
        return octLp;
    }

    // octave down: the low part of the signal toggles a flip-flop at every rising zero crossing; the square wave follows the level
    float subOctave (float x)
    {
        const float g1 = 1.0f - std::exp (-2.0f * kPi * 350.0f / fs);
        sLp1 += (x - sLp1) * g1; sLp2 += (sLp1 - sLp2) * g1;
        const float hyst = 0.002f;
        if (sPrev < -hyst && sLp2 >= hyst) sState = -sState;                // rising zero crossing (with a little hysteresis)
        if (sLp2 < -hyst || sLp2 > hyst) sPrev = sLp2 < 0.0f ? -1.0f : 1.0f;
        const float a = std::fabs (sLp2);
        sEnv = a > sEnv ? sEnv + (a - sEnv) * 0.05f : sEnv + (a - sEnv) * (1.0f / (0.03f * fs));
        const float target = sState * std::min (1.0f, sEnv * 6.0f);
        sSm += (target - sSm) * (1.0f - std::exp (-2.0f * kPi * 250.0f / fs));
        return sSm;
    }

    float fs = 48000.0f;
    Chan ch[2];
    float gl = 1.0f, gh = 1.0f, gf = 0.1f, toneA = 0.5f, dryG = 0.0f, wetG = 1.0f;
    float oLp1 = 0, oLp2 = 0, oHp = 0, octDc = 0, octLp = 0, sLp1 = 0, sLp2 = 0, sEnv = 0, sSm = 0, sState = 1.0f, sPrev = 0.0f;
};
} // namespace hm2
