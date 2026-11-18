// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// SP-1 Space Engine: an experimental spatial processor on one engine with five modes.
//   Core: input diffusion (4 all-passes) -> 8-line feedback delay network (Householder mix, modulated lines, damping) -> stereo taps.
//   SHIMMER    octave and octave+fifth pitch-shifted signal fed back into the network (rising, glassy tails)
//   INFINITE   a swelling input and a decay that ends in an endless tail
//   SPECTRAL   a bank of six tuned resonators (harmonic series) in front of the network: metallic, bell-like reflections
//   MODULATED  deep, slow modulation of the network lines: a lush, moving space
//   ROOMS      early reflections and uneven line lengths: small, odd rooms and caverns
//   FREEZE     (any mode) closes the input and holds the tail
// Header-only, no JUCE dependency, no allocation after prepare().
#include "ReverbEngine.h"      // bigsky::Delay, Allpass, PitchShifter

namespace sp1
{
constexpr int kNumModes = 5;
enum Mode { Shimmer, Infinite, Spectral, Modulated, Rooms };

inline const char* modeName (int m)
{
    static const char* n[kNumModes] = { "SHIMMER", "INFINITE", "SPECTRAL", "MODULATED", "ROOMS" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}
inline const char* textureName (int m)       // what the TEXTURE knob does in each mode
{
    static const char* n[kNumModes] = { "SHIMMER", "SWELL", "TUNING", "RATE", "SHAPE" };
    return n[std::clamp (m, 0, kNumModes - 1)];
}

struct Params
{
    int mode = 0;
    bool freeze = false;
    float size = 0.5f, decay = 0.5f, diffusion = 0.6f, tone = 0.5f, motion = 0.4f, texture = 0.5f;   // 0..1
};

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        using namespace bigsky;
        static const float apMs[4] = { 4.3f, 7.1f, 10.9f, 15.7f };
        for (int i = 0; i < 4; ++i) ap[i].init ((int) (apMs[i] * 0.001f * fs));
        for (auto& l : fdn) l.init ((int) (0.34f * fs));
        for (auto& c : comb) c.init ((int) (0.03f * fs) + 8);
        early.init ((int) (0.3f * fs) + 4);
        for (auto& s : shifter) s.init (fs);
        for (int k = 0; k < N; ++k) inc[k] = 2.0f * kPi * (0.19f + 0.073f * (float) k) / fs;
        reset();
    }

    void reset()
    {
        for (auto& l : fdn) l.clear();
        for (auto& a : ap) a.d.clear();
        for (auto& c : comb) c.clear();
        early.clear();
        for (auto& s : shifter) s.clear();
        for (int k = 0; k < N; ++k) { lp[k] = 0.0f; ph[k] = (float) k * 0.7f; len[k] = 1000.0f; }
        for (auto& c : combLen) c = 100.0f;
        wetPrev = 0.0f; shLp[0] = shLp[1] = 0.0f; env = 0.0f; gain = 0.0f; silent = 0; frz = 0.0f; duck = 1.0f;
        dcX = dcY = 0.0f; snap = true; mode = 0;
    }

    // Processes l / r in place (dry / wet included).  mix 0..1, levelGain linear for the wet signal.
    void process (float* l, float* r, int n, const Params& p, float mix, float levelGain)
    {
        const float dryT = std::cos (mix * 0.5f * bigsky::kPi), wetT = std::sin (mix * 0.5f * bigsky::kPi) * levelGain;
        for (int off = 0; off < n; off += kBlock)
        {
            const int m = std::min (kBlock, n - off);
            control (p);
            for (int i = off; i < off + m; ++i)
            {
                const bool want = p.mode == mode;               // mode change: fade out, reset the network, fade in
                duck = std::clamp (duck + (want ? 1.0f : -1.0f) / (0.03f * fs), 0.0f, 1.0f);
                if (! want && duck <= 0.0f) { const float keepDry = dryG, keepWet = wetG; reset(); dryG = keepDry; wetG = keepWet; mode = p.mode; control (p); }
                dryG += 0.01f * (dryT - dryG);
                wetG += 0.01f * (wetT - wetG);
                float yl, yr;
                tick (0.5f * (l[i] + r[i]), p, yl, yr);
                const float w = duck * wetG;
                l[i] = l[i] * dryG + yl * w;
                r[i] = r[i] * dryG + yr * w;
            }
        }
    }

private:
    static constexpr int N = 8, kBlock = 32, kCombs = 6;
    using Delay = bigsky::Delay;

    static float rtSeconds (int m, float d)             // decay knob -> RT60
    {
        if (m == Infinite) return d >= 0.98f ? 1.0e9f : 2.0f * std::pow (60.0f, d / 0.98f);
        return 0.3f * std::pow (100.0f, d);
    }

    void control (const Params& p)
    {
        using namespace bigsky;
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        static const float spread[N] = { 0.0f, 0.55f, -0.3f, 0.8f, -0.5f, 0.3f, 1.0f, -0.15f };
        const float scale = p.mode == Rooms ? 0.15f + 1.45f * p.size : 0.5f + 2.5f * p.size;
        const float shape = p.mode == Rooms ? p.texture * 0.8f : 0.0f;
        const float maxLen = 0.33f * fs;
        for (int k = 0; k < N; ++k)
        {
            const float t = std::clamp (baseMs[k] * 0.001f * fs * scale * (1.0f + shape * spread[k]), 12.0f, maxLen);
            len[k] = snap ? t : len[k] + 0.1f * (t - len[k]);
        }
        const float rt = rtSeconds (p.mode, p.decay);
        for (int k = 0; k < N; ++k)
            gT[k] = rt > 1.0e8f ? 1.0f : std::pow (10.0f, -3.0f * len[k] / (fs * rt));
        const float fc = 1200.0f * std::pow (13.3f, p.tone);                          // damping 1.2 k .. 16 k
        dampT = coef (fc, fs);
        for (auto& a : ap) a.g = 0.25f + 0.55f * p.diffusion;
        const float modMs = p.mode == Modulated ? 9.0f : 2.5f;
        modDepth = std::min (p.motion * modMs * 0.001f * fs, 0.4f * len[0]);
        rateMul = p.mode == Modulated ? 0.3f + 4.0f * p.texture : 1.0f;
        if (p.mode == Shimmer) { octAmt = std::min (1.0f, p.texture * 2.0f) * 0.45f; fifAmt = std::max (0.0f, p.texture * 2.0f - 1.0f) * 0.4f; }
        if (p.mode == Spectral)
        {
            const float f0 = 55.0f * std::pow (8.0f, p.texture);                      // 55 .. 440 Hz
            for (int k = 0; k < kCombs; ++k) combT[k] = std::clamp (fs / (f0 * (float) (k + 1)), 4.0f, 0.029f * fs);
            if (snap) for (int k = 0; k < kCombs; ++k) combLen[k] = combT[k];
        }
        if (p.mode == Rooms)
        {
            static const float r[12] = { 0.03f, 0.07f, 0.11f, 0.17f, 0.23f, 0.31f, 0.4f, 0.5f, 0.6f, 0.72f, 0.85f, 1.0f };
            const float span = (0.02f + 0.1f * p.size) * fs;
            for (int k = 0; k < 12; ++k)
            {
                tapL[k] = std::max (1.0f, r[k] * span);
                tapR[k] = std::max (1.0f, (r[k] * 0.87f + 0.04f) * span);
                tapA[k] = std::pow (10.0f, -1.2f * r[k]) * ((k & 1) ? -1.0f : 1.0f);
            }
        }
        snap = false;
    }

    void tick (float xin, const Params& p, float& outL, float& outR)
    {
        using namespace bigsky;
        frz += ((p.freeze ? 1.0f : 0.0f) - frz) * (1.0f / (0.03f * fs));
        const float dcy = xin - dcX + 0.9975f * dcY;                 // DC blocker
        dcX = xin; dcY = dcy;
        float x = dcy * (1.0f - frz);
        env = std::max (std::fabs (dcy), env * 0.9997f);

        float xf = x;
        switch (p.mode)
        {
            case Shimmer:
            {
                const float k = 1.0f - 0.7f * frz;
                const float a = shifter[0].process (wetPrev, 2.0f), b = shifter[1].process (wetPrev, 3.0f);
                shLp[0] = a + coef (5000.0f, fs) * (shLp[0] - a);
                shLp[1] = b + coef (4500.0f, fs) * (shLp[1] - b);
                xf = x + std::tanh ((shLp[0] * octAmt + shLp[1] * fifAmt) * 2.0f * k);
                break;
            }
            case Infinite:
            {
                const float rise = 0.02f + p.texture * 3.0f;
                if (env > 0.01f) { silent = 0; gain = std::min (1.0f, gain + 1.0f / (rise * fs)); }
                else if (++silent > (int) (0.3f * fs)) gain = std::max (0.0f, gain - 1.0f / (0.05f * fs));
                xf = x * gain;
                break;
            }
            case Spectral:
            {
                float sum = 0.0f;
                for (int k = 0; k < kCombs; ++k)
                {
                    combLen[k] += 0.002f * (combT[k] - combLen[k]);
                    const float y = x + 0.9f * comb[k].read (std::max (1.0f, combLen[k] - 1.0f));
                    comb[k].push (std::tanh (y));
                    sum += y;
                }
                xf = 0.6f * x + 0.1f * sum;
                break;
            }
            default: break;
        }

        float yl = 0.0f, yr = 0.0f;
        for (auto& a : ap) xf = a.process (xf);
        static const float sL[N] = { 1, -1, 1, -1, 1, -1, 1, -1 }, sR[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
        static const float inj[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        const float damp = dampT + (coef (16000.0f, fs) - dampT) * frz;
        float z[N], sum = 0.0f;
        for (int k = 0; k < N; ++k)
        {
            const float m = modDepth * std::sin (ph[k]);
            ph[k] += inc[k] * rateMul; if (ph[k] > 2.0f * kPi) ph[k] -= 2.0f * kPi;
            const float y = fdn[k].read (std::max (1.0f, len[k] - 1.0f + m));
            lp[k] = y + damp * (lp[k] - y);
            z[k] = lp[k];
            sum += z[k];
            yl += sL[k] * z[k];
            yr += sR[k] * z[k];
        }
        const float s2 = sum * (2.0f / (float) N);
        for (int k = 0; k < N; ++k)
        {
            const float g = gT[k] + (1.0f - gT[k]) * frz;
            fdn[k].push (0.35f * inj[k] * xf + g * (z[k] - s2));
        }
        yl *= 0.35f; yr *= 0.35f;
        wetPrev = 0.5f * (yl + yr);

        if (p.mode == Rooms)
        {
            early.push (x);
            float el = 0.0f, er = 0.0f;
            for (int k = 0; k < 12; ++k)
            {
                el += tapA[k] * early.read (tapL[k]);
                er += tapA[k] * ((k & 2) ? -1.0f : 1.0f) * early.read (tapR[k]);
            }
            yl += 0.3f * el; yr += 0.3f * er;
        }
        outL = 1.5f * std::tanh (yl / 1.5f);             // safety: the tail can never run away
        outR = 1.5f * std::tanh (yr / 1.5f);
    }

    float fs = 48000.0f;
    int mode = 0;
    bool snap = true;
    float duck = 1.0f, dryG = 0.7f, wetG = 0.7f, frz = 0.0f;

    Delay early, fdn[N], comb[kCombs];
    bigsky::Allpass ap[4];
    bigsky::PitchShifter shifter[2];
    float len[N] {}, gT[N] {}, lp[N] {}, ph[N] {}, inc[N] {};
    float combLen[kCombs] {}, combT[kCombs] {};
    float tapL[12] {}, tapR[12] {}, tapA[12] {};
    float dampT = 0.5f, modDepth = 0.0f, rateMul = 1.0f, octAmt = 0.0f, fifAmt = 0.0f;
    float wetPrev = 0.0f, shLp[2] {}, env = 0.0f, gain = 0.0f, dcX = 0.0f, dcY = 0.0f;
    int silent = 0;
};
} // namespace sp1
