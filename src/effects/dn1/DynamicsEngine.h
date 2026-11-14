// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DN-1 Dynamics Lab: one dynamics engine with the voicings of the VIBR dynamics plugins as modes (see .claude/lab_specs.md):
//   SHAPER      the DY-1 Dynamic Shaper: gate -> expander -> transient shaper, each with its own switch (the engine of src/effects/dy1)
//   COMPRESSOR  types SUSTAIN (the CS-3 Compression Sustainer: one SUSTAIN knob sets threshold, ratio and release, ATTACK, TONE), VCA (a clean feed-forward
//               compressor with a soft knee) and OPTO (slower, with a program-dependent release: short reductions recover quickly, long ones slowly)
//   LIMITER     types PEAK (the LM-2 limiter: fast, no lookahead, soft ceiling) and TRUE PEAK (the MS-1 limiter: 2 ms lookahead, inter-sample safe)
// The latency is the same in every mode and type (the lookahead of the true-peak limiter), so it never changes while playing: what does not pass through the
// lookahead is delayed by the same number of samples, and so is the dry sound of MIX. Header-only, no JUCE dependency, no allocation after prepare().
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "../dy1/DynamicEngine.h"
#include "../../mastering/ms1/MasteringEngine.h"

namespace dn1
{
constexpr int kNumModes = 3, kHistory = dy1::kHistory;
enum Mode { Shaper, Compressor, Limiter };
enum CompType { Sustain, Vca, Opto };
enum LimType { Peak, TruePeak };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m) { static const char* n[kNumModes] = { "SHAPER", "COMPRESSOR", "LIMITER" }; return n[clampMode (m)]; }
inline int variantCount (int mode) { return mode == Compressor ? 3 : mode == Limiter ? 2 : 0; }
inline const char* variantName (int mode, int v)
{
    static const char* c[3] = { "SUSTAIN", "VCA", "OPTO" }, *l[2] = { "PEAK", "TRUE PEAK" };
    return mode == Compressor ? c[std::clamp (v, 0, 2)] : l[std::clamp (v, 0, 1)];
}

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 8.68588964f * std::log (std::max (g, 1.0e-9f)); }

// the compressor and limiter curves of the CS-3 and LM-2, as in their engines
inline float cs3AttackMs (float a01)  { return 0.5f * std::pow (60.0f, a01); }
inline float cs3ReleaseMs (float s01) { return 450.0f - 300.0f * s01; }
inline float cs3ThresholdDb (float s01) { return -8.0f - 42.0f * s01; }
inline float cs3Ratio (float s01) { return 3.0f + 17.0f * s01 * s01; }
inline float softKnee (float over, float ratio, float knee)         // the gain reduction in dB (<= 0) of a downward compressor with a soft knee
{
    const float slope = 1.0f / ratio - 1.0f;
    if (over <= -0.5f * knee) return 0.0f;
    if (over >= 0.5f * knee) return slope * over;
    const float t = over + 0.5f * knee;
    return slope * t * t / (2.0f * knee);
}
inline float cs3MakeupDb (float thr, float ratio) { return -softKnee (-20.0f - thr, ratio, 10.0f) * 0.9f; }
inline float lm2MakeupDb (float thr) { return -0.5f * thr - 2.0f; }

struct Params
{
    int mode = Shaper, variant = 0;
    // SHAPER (the DY-1)
    dy1::Params shaper;
    // COMPRESSOR: SUSTAIN uses sustain / attack01 / tone, VCA and OPTO threshold, ratio, attackMs, releaseMs, makeup, mix and tone (all 0 .. 1 are knob positions)
    float sustain = 0.5f, attack01 = 0.3f;
    float compThresh = -20.0f, compRatio = 4.0f, compAttackMs = 10.0f, compReleaseMs = 200.0f, compMakeup = 0.0f, compMix = 1.0f;
    float tone = 0.5f;                                 // 0 .. 1, 0.5 = flat (the tilt of the CS-3 and LM-2)
    // LIMITER
    float limThresh = -6.0f, limRelease = 100.0f, limCeiling = -1.0f;
    // shared
    float scHpHz = 0.0f, link = 1.0f, mix = 1.0f;
    bool autoLevel = false;
};

class Engine
{
public:
    std::atomic<float> histIn[kHistory], histGain[kHistory];            // dB, one entry per 20 ms: the input peak and the gain of the chain
    std::atomic<int> histPos { 0 };
    std::atomic<float> grNow { 0.0f };                                  // the gain reduction now (dB <= 0), for the value card
    int latency() const { return lat; }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        shaper.prepare (fs);
        limiter.prepare (fs);
        limiter.setLookahead (2.0f);
        lat = limiter.latency();
        for (auto& d : delay) d.assign (8192, 0.0f);
        for (auto& d : dryDelay) d.assign (8192, 0.0f);
        histWindow = std::max (1, (int) (0.02f * fs));
        reset();
    }

    void reset()
    {
        shaper.reset(); limiter.reset();
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
        wpos = 0;
        for (auto& c : ch) c.reset();
        autoGain = 1.0f; inPow = outPow = 1.0e-6f; lastMode = -1; fade = 1.0f;
        winCount = 0; winIn = winOut = 0.0f;
        for (int i = 0; i < kHistory; ++i) { histIn[i].store (-90.0f); histGain[i].store (0.0f); }
        histPos.store (0); grNow.store (0.0f);
    }

    // the output gain `levelGain` is linear
    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        for (int pos = 0; pos < n; pos += kChunk)
            processChunk (l + pos, r + pos, std::min (kChunk, n - pos), p, levelGain);
    }

private:
    static constexpr int kChunk = 512;

    struct Shelf
    {
        float c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
        void set (double sr, float freq, float gainDb, bool high)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 6.283185307179586 * freq / sr, cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * 0.7071), sq = 2.0 * std::sqrt (A) * alpha;
            double b0, b1, b2, a0, a1, a2;
            if (high) { b0 = A * ((A + 1) + (A - 1) * cw + sq); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sq); a0 = (A + 1) - (A - 1) * cw + sq; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sq; }
            else      { b0 = A * ((A + 1) - (A - 1) * cw + sq); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sq); a0 = (A + 1) + (A - 1) * cw + sq; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sq; }
            c0 = (float) (b0 / a0); c1 = (float) (b1 / a0); c2 = (float) (b2 / a0); d1 = (float) (a1 / a0); d2 = (float) (a2 / a0);
        }
        float process (float x) { const float y = c0 * x + z1; z1 = c1 * x - d1 * y + z2; z2 = c2 * x - d2 * y; return y; }
    };
    struct Hp
    {
        float b0 = 1, b1 = -2, b2 = 1, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (float hz, float fs)
        {
            const double K = std::tan (3.14159265358979 * (double) hz / (double) fs), q = 0.70710678, a0 = 1.0 + K / q + K * K;
            b0 = (float) (1.0 / a0); b1 = -2.0f * b0; b2 = b0; a1 = (float) (2.0 * (K * K - 1.0) / a0); a2 = (float) ((1.0 - K / q + K * K) / a0);
        }
        float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    struct Chan
    {
        Shelf low, high; Hp hp; float envDb = -120.0f, gr = 0.0f, slowGr = 0.0f;
        void reset() { low = Shelf(); high = Shelf(); hp = Hp(); envDb = -120.0f; gr = slowGr = 0.0f; }
    };

    void setTone (float tone01)
    {
        const float tilt = (tone01 * 10.0f - 5.0f) * 1.6f;
        if (tilt == toneNow && fs == toneFs) return;
        for (auto& c : ch) { const auto lz1 = c.low.z1, lz2 = c.low.z2, hz1 = c.high.z1, hz2 = c.high.z2; c.low.set (fs, 350.0f, -tilt, false); c.high.set (fs, 2500.0f, tilt, true); c.low.z1 = lz1; c.low.z2 = lz2; c.high.z1 = hz1; c.high.z2 = hz2; }
        toneNow = tilt; toneFs = fs;
    }

    void delayBy (float* x, int n, int c, int d)                          // a fixed delay of `d` samples (the ring is 8192 long)
    {
        auto& b = delay[c];
        for (int i = 0; i < n; ++i) { b[(size_t) ((wpos + i) & 8191)] = x[i]; x[i] = b[(size_t) ((wpos + i - d) & 8191)]; }
    }

    void processChunk (float* l, float* r, int n, const Params& p, float levelGain)
    {
        const int mode = clampMode (p.mode);
        if (mode != lastMode) { if (lastMode >= 0) { fade = 0.0f; shaper.reset(); limiter.reset(); for (auto& c : ch) c.reset(); } lastMode = mode; }
        float* io[2] = { l, r };

        // the dry sound for MIX, delayed like the wet path, and the input level
        float inPeak = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (io[c][i])) io[c][i] = 0.0f;
                inPeak = std::max (inPeak, std::abs (io[c][i]));
                auto& b = dryDelay[c]; b[(size_t) ((wpos + i) & 8191)] = io[c][i]; dry[c][i] = b[(size_t) ((wpos + i - lat) & 8191)];
            }

        bool delayed = false;                                              // the wet path already has the latency (the true-peak limiter)
        if (mode == Shaper)
        {
            dy1::Params q = p.shaper; q.mix = 1.0f;
            shaper.process (l, r, n, q, 1.0f);
        }
        else if (mode == Compressor)
        {
            setTone (p.tone);
            const bool sus = p.variant == Sustain, opto = p.variant == Opto;
            const float thrSus = cs3ThresholdDb (p.sustain), ratSus = cs3Ratio (p.sustain);
            const float thr = sus ? thrSus : p.compThresh, ratio = sus ? ratSus : std::max (1.0f, p.compRatio);
            const float atkMs = sus ? cs3AttackMs (p.attack01) : std::max (0.05f, p.compAttackMs) * (opto ? 1.5f : 1.0f);
            const float relMs = sus ? cs3ReleaseMs (p.sustain) : std::max (10.0f, p.compReleaseMs);
            const float aCoef = std::exp (-1.0f / (0.001f * atkMs * fs));
            const float rCoef = std::exp (-1.0f / (0.001f * relMs * fs)), slowC = std::exp (-1.0f / (2.0f * fs)), slowA = std::exp (-1.0f / (0.5f * fs));       // OPTO: how long the compressor has been working (rises in 0.5 s, forgets in 2 s)
            const float makeup = sus ? cs3MakeupDb (thr, ratio) : p.compMakeup, knee = sus ? 10.0f : opto ? 10.0f : 6.0f;
            const float mixW = sus ? 1.0f : std::clamp (p.compMix, 0.0f, 1.0f);
            if (p.scHpHz > 20.0f) { if (p.scHpHz != hpNow) { hpNow = p.scHpHz; for (auto& c : ch) c.hp.set (hpNow, fs); } } else hpNow = 0.0f;
            float minGr = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float det[2] = { l[i], r[i] };
                if (hpNow > 0.0f) { det[0] = ch[0].hp.tick (det[0]); det[1] = ch[1].hp.tick (det[1]); }
                det[0] = std::abs (det[0]); det[1] = std::abs (det[1]);
                const float dMax = std::max (det[0], det[1]);
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[c];
                    const float d = p.link * dMax + (1.0f - p.link) * det[c];
                    const float inDb = 20.0f * std::log10 (d + 1.0e-6f);
                    float gr;
                    if (sus)                                              // the CS-3: the envelope follows the level in dB
                    {
                        s.envDb = (inDb > s.envDb ? aCoef : rCoef) * s.envDb + (1.0f - (inDb > s.envDb ? aCoef : rCoef)) * inDb;
                        gr = softKnee (s.envDb - thr, ratio, knee);
                    }
                    else                                                  // VCA / OPTO: the ballistics work on the gain reduction
                    {
                        const float red = -softKnee (inDb - thr, ratio, knee);                     // >= 0
                        float rc = rCoef;
                        if (opto) { s.slowGr = red > s.slowGr ? slowA * s.slowGr + (1.0f - slowA) * red : slowC * s.slowGr + (1.0f - slowC) * red;
                                    rc = std::exp (-1.0f / (0.001f * relMs * (0.5f + 2.5f * std::clamp (s.slowGr / 10.0f, 0.0f, 1.0f)) * fs)); }
                        s.gr = red > s.gr ? aCoef * s.gr + (1.0f - aCoef) * red : rc * s.gr + (1.0f - rc) * red;
                        gr = -s.gr;
                    }
                    minGr = std::min (minGr, gr);
                    const float g = dbToGain (gr + makeup);
                    float y = s.high.process (s.low.process (io[c][i] * g));
                    if (sus) y = std::tanh (y * 0.9f) / 0.9f;
                    io[c][i] = mixW >= 1.0f ? y : io[c][i] + (y - io[c][i]) * mixW;
                }
            }
            grNow.store (minGr, std::memory_order_relaxed);
        }
        else if (p.variant == Peak)                                        // the LM-2
        {
            setTone (p.tone);
            const float aCoef = std::exp (-1.0f / (0.0002f * fs)), rCoef = std::exp (-1.0f / (0.001f * std::max (5.0f, p.limRelease) * fs));
            const float thr = p.limThresh, ceil = dbToGain (p.limCeiling);
            float minGr = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float dMax = std::max (std::abs (l[i]), std::abs (r[i]));
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[c];
                    const float d = p.link * dMax + (1.0f - p.link) * std::abs (io[c][i]);
                    const float inDb = 20.0f * std::log10 (d + 1.0e-6f), coef = inDb > s.envDb ? aCoef : rCoef;
                    s.envDb = coef * s.envDb + (1.0f - coef) * inDb;
                    const float gr = softKnee (s.envDb - thr, 20.0f, 4.0f);
                    minGr = std::min (minGr, gr);
                    float y = s.high.process (s.low.process (io[c][i] * dbToGain (gr + lm2MakeupDb (thr))));
                    io[c][i] = std::tanh (y * 0.9f) / 0.9f * ceil;
                }
            }
            grNow.store (minGr, std::memory_order_relaxed);
        }
        else                                                               // TRUE PEAK: the MS-1 limiter, the threshold drives the input up to the ceiling
        {
            setTone (p.tone);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = ch[c].high.process (ch[c].low.process (io[c][i]));
            ms1::LimiterParams lp;
            lp.gainDb = p.limCeiling - p.limThresh; lp.ceilingDb = p.limCeiling; lp.releaseMs = p.limRelease; lp.lookaheadMs = 2.0f; lp.link = p.link;
            limiter.process (l, r, n, lp);
            grNow.store (limiter.grDb.load (std::memory_order_relaxed), std::memory_order_relaxed);
            delayed = true;
        }
        if (! delayed) for (int c = 0; c < 2; ++c) delayBy (io[c], n, c, lat);

        // MIX, auto level, LEVEL
        float outPeak = 0.0f, inE = 0.0f, outE = 0.0f;
        const float fadeInc = 1.0f / (0.03f * fs), sm = 1.0f - std::exp (-1.0f / (0.4f * fs)), pm = 1.0f - std::exp (-1.0f / (1.0f * fs));
        for (int i = 0; i < n; ++i)
        {
            fade = std::min (1.0f, fade + fadeInc);
            const float m = std::clamp (p.mix, 0.0f, 1.0f);
            float o[2];
            for (int c = 0; c < 2; ++c) { const float w = io[c][i]; o[c] = dry[c][i] + (w - dry[c][i]) * m; }
            inE = 0.5f * (dry[0][i] * dry[0][i] + dry[1][i] * dry[1][i]); outE = 0.5f * (o[0] * o[0] + o[1] * o[1]);
            inPow += pm * (inE - inPow); outPow += pm * (outE - outPow);
            const float target = p.autoLevel ? std::clamp (std::sqrt (inPow / std::max (outPow, 1.0e-10f)), 0.125f, 8.0f) : 1.0f;
            autoGain += sm * (target - autoGain);
            for (int c = 0; c < 2; ++c)
            {
                float y = o[c] * autoGain * levelGain;
                y = dry[c][i] + (y - dry[c][i]) * fade;                      // a mode change fades in from the dry sound
                if (! std::isfinite (y)) y = 0.0f;
                io[c][i] = y;
                outPeak = std::max (outPeak, std::abs (io[c][i]));
            }
        }
        wpos += n;

        // the display history: the input peak and the gain of the chain (the output peak over the input peak)
        winIn = std::max (winIn, inPeak); winOut = std::max (winOut, outPeak / std::max (1.0e-6f, std::abs (levelGain)));
        winCount += n;
        while (winCount >= histWindow)
        {
            const int pos = histPos.load (std::memory_order_relaxed);
            histIn[pos].store (gainToDb (winIn), std::memory_order_relaxed);
            histGain[pos].store (winIn > 1.0e-4f ? std::clamp (gainToDb (winOut) - gainToDb (winIn), -40.0f, 24.0f) : 0.0f, std::memory_order_relaxed);
            histPos.store ((pos + 1) % kHistory, std::memory_order_release);
            winCount -= histWindow; winIn = winOut = 0.0f;
        }
    }

    float fs = 48000.0f, toneNow = 1000.0f, toneFs = 0.0f, hpNow = 0.0f, autoGain = 1.0f, inPow = 1.0e-6f, outPow = 1.0e-6f, fade = 1.0f, winIn = 0.0f, winOut = 0.0f;
    int lat = 0, lastMode = -1, histWindow = 960, winCount = 0, wpos = 0;
    dy1::Engine shaper;
    ms1::Limiter limiter;
    Chan ch[2];
    std::vector<float> delay[2], dryDelay[2];
    float dry[2][kChunk] {};
};
} // namespace dn1
