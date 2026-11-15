// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// DV-1 Drive Lab: one drive engine with the voicings of the VIBR drive plugins as modes (see .claude/lab_specs.md):
//   OVERDRIVE   the OD-2 Hybrid Drive: 12 drive types, a LOW / MID / HIGH tone stack and a noise gate (the engine of src/effects/od2)
//   DISTORTION  the DS-1 stompbox distortion: CLASSIC (the DS-1) and MODERN (a tight low cut before the clipper, more gain)
//   TUBE        smooth saturation of the SA-1: SINGLE (lopsided, even harmonics, BIAS) and PUSH-PULL (symmetric, odd harmonics)
//   TAPE        gentle saturation of the SA-1 with WARMTH (head bump, high roll-off): NEW and AGED (darker, a little hiss)
//   FOLD        the HM-2 transistor clipper (TRANSISTOR) and wave folder (FOLDBACK) with FOLD and SYMMETRY
//   CRUSH       the SA-1 bit crusher: HARD (as the SA-1) and SMOOTH (dithered and smoothed)
// Every mode runs through the same 4x oversampler (CRUSH, which is meant to alias, bypasses the nonlinearity but keeps the delay), so the latency is the
// same in every mode and never changes while playing. BOOST (a clean gain before the drive), AUTO LEVEL, MIX and LEVEL are shared.
// The static curve of every mode (`staticCurve`) is what the editor draws. No allocation after prepare().
#include <JuceHeader.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "../od2/OD2DSP.h"
#include "../ds1/DS1DSP.h"
#include "../sa1/SaturatorEngine.h"
#include "../hm2/HarmonicEngine.h"

namespace dv1
{
constexpr int kNumModes = 6, kNumOdTypes = od2::kNumTypes;
enum Mode { Overdrive, Distortion, Tube, Tape, Fold, Crush };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m) { static const char* n[kNumModes] = { "OVERDRIVE", "DISTORTION", "TUBE", "TAPE", "FOLD", "CRUSH" }; return n[clampMode (m)]; }
inline int variantCount (int mode) { return mode == Overdrive ? 0 : 2; }
inline const char* variantName (int mode, int v)
{
    static const char* n[kNumModes][2] = { { "", "" }, { "CLASSIC", "MODERN" }, { "SINGLE", "PUSH-PULL" }, { "NEW", "AGED" }, { "TRANSISTOR", "FOLDBACK" }, { "HARD", "SMOOTH" } };
    return n[clampMode (mode)][std::clamp (v, 0, 1)];
}
inline const char* odTypeName (int t) { return od2::typeDef (t).name; }

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }

struct Params
{
    int mode = Overdrive, variant = 0, odType = 0;
    float drive = 0.4f, tone = 0.5f;                       // 0 .. 1
    bool boostOn = false, autoLevel = true;
    float boostDb = 10.0f;                                 // 0 .. 24
    float mix = 1.0f;                                      // 0 .. 1
    // OVERDRIVE (knobs 0 .. 10 like the OD-2)
    float odParam = 5.0f, odLow = 5.0f, odMid = 5.0f, odHigh = 5.0f, gateThresh = 0.0f, gateDecay = 3.0f;
    float tight = 0.0f;                                    // DISTORTION, MODERN: 0 .. 1
    float tubeBias = 0.4f;                                 // TUBE: 0 .. 1
    float warmth = 0.5f;                                   // TAPE: 0 .. 1
    float fold = 0.5f, symmetry = 1.0f;                    // FOLD: 0 .. 1
    float bits = 8.0f, rate = 12000.0f, crushMix = 1.0f;   // CRUSH: 1 .. 16, Hz, 0 .. 1
};

// the pre-gain of the drive knob in each mode (linear), used by the engine and by the display
inline float driveGain (const Params& p)
{
    switch (p.mode)
    {
        case Overdrive: { const auto& d = od2::typeDef (p.odType); return od2::dbToGain (d.gainMinDb + (d.gainMaxDb - d.gainMinDb) * p.drive); }
        case Distortion: return ds1::distGain (p.drive) * (p.variant == 1 ? 1.8f : 1.0f);
        case Fold: return dbToGain (30.0f * p.drive);
        case Crush: return dbToGain (18.0f * p.drive);
        default: return 1.0f;
    }
}

// the memoryless part of a mode (without the filters), for the editor's transfer curve and sine plot
inline float staticCurve (const Params& p, float x)
{
    const float g = driveGain (p);
    switch (p.mode)
    {
        case Overdrive:
        {
            const auto& d = od2::typeDef (p.odType);
            const float hardness = std::clamp (d.hardness + (p.odParam * 0.1f - 0.5f) * 0.8f, 0.0f, 1.0f);
            float y = x * g;
            for (int s = 0; s < d.stages; ++s) y = od2::shape (y * (s == 0 ? 1.0f : 2.5f), hardness, d.asym, d.fuzz);
            return y;
        }
        case Distortion: return ds1::diodeClip (x * 2.0f * 1.4f * g * 0.5f) / 0.6f;
        case Tube: return sa1::tubeCurve (x, p.drive, p.variant == 0 ? p.tubeBias : 0.0f);
        case Tape: return sa1::tapeCurve (x, p.drive);
        case Fold:
        {
            const int c = p.variant == 0 ? hm2::Transistor : hm2::Fold;
            const float u = x * g * (c == hm2::Fold ? 0.25f + p.fold * 1.5f : 0.25f), bias = (1.0f - p.symmetry) * 0.8f;
            return hm2::saturateBiased (u, c, bias);
        }
        default: return sa1::crushCurve (x * g, p.bits);
    }
}

class Engine
{
public:
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };           // peaks for the editor
    std::atomic<float> dotIn { 0.0f }, dotOut { 0.0f };               // the live point on the transfer curve

    int latency() const { return lat; }

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) kChunk);
        lat = (int) std::lround (os->getLatencyInSamples());
        for (auto& d : dryDelay) d.assign (8192, 0.0f);
        reset();
    }

    void reset()
    {
        if (os) os->reset();
        for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
        wpos = 0;
        for (auto& c : ch) c.reset();
        gS = 1.0f; boostS = 1.0f; autoGain = 1.0f; inPow = outPow = 1.0e-6f; fade = 1.0f; lastMode = lastVariant = lastOdType = -1;
        for (auto& b : bump) b = 0.0f;
        inLevel.store (0.0f); outLevel.store (0.0f);
    }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        for (int pos = 0; pos < n; pos += kChunk) processChunk (l + pos, r + pos, std::min (kChunk, n - pos), p, levelGain);
    }

private:
    static constexpr int kChunk = 512;

    struct Chan
    {
        od2::DriveChannel od; ds1::DistChannel ds; ds1::ToneChannel dsTone; od2::ToneStack stack; od2::Gate gate; od2::Biquad low, high;
        od2::OnePole dcBlock, tightHp, bumpLp, tape1, tape2, crushLp; float hold = 0.0f, tubeDcX = 0.0f, tubeDcY = 0.0f;
        void reset() { od = od2::DriveChannel(); ds = ds1::DistChannel(); dsTone = ds1::ToneChannel(); stack = od2::ToneStack(); gate = od2::Gate(); low = od2::Biquad(); high = od2::Biquad();
                       dcBlock = tightHp = bumpLp = tape1 = tape2 = crushLp = od2::OnePole(); hold = tubeDcX = tubeDcY = 0.0f; }
    };

    void configure (const Params& p)
    {
        const float osRate = fs * 4.0f;
        const bool changed = p.mode != lastMode || p.variant != lastVariant || p.odType != lastOdType;
        if (changed)
        {
            if (lastMode >= 0) { fade = 0.0f; for (auto& c : ch) c.reset(); }
            for (auto& c : ch)
            {
                c.od.prepare (osRate); c.ds.prepare (osRate); c.dsTone.prepare (fs);
                c.dcBlock.setCutoff (20.0f, osRate); c.tightHp.setCutoff (60.0f + 220.0f * 1.0f, osRate);
                c.bumpLp.setCutoff (90.0f, fs); c.crushLp.setCutoff (6000.0f, fs);
            }
            lastMode = p.mode; lastVariant = p.variant; lastOdType = p.odType;
        }
        const float warm = p.warmth, aged = p.mode == Tape && p.variant == 1 ? 1.0f : 0.0f;
        const float fcTape = 16000.0f * std::pow (0.3125f, warm) * (1.0f - 0.45f * aged);
        const float tilt = (p.tone - 0.5f) * 16.0f;
        for (auto& c : ch)
        {
            c.od.configure (p.odType, p.odParam * 0.1f);
            c.stack.set (p.odLow, p.odMid, p.odHigh, fs);
            c.tape1.setCutoff (fcTape, fs); c.tape2.setCutoff (fcTape, fs);
            c.low.lowShelf (350.0f, -tilt * 0.5f, fs); c.high.highShelf (2500.0f, tilt * 0.5f, fs);
            c.tightHp.setCutoff (60.0f + 260.0f * p.tight, osRate);
        }
    }

    void processChunk (float* l, float* r, int n, const Params& p, float levelGain)
    {
        configure (p);
        float* io[2] = { l, r };
        const int mode = clampMode (p.mode);

        float inPeak = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (io[c][i])) io[c][i] = 0.0f;
                inPeak = std::max (inPeak, std::abs (io[c][i]));
                auto& b = dryDelay[c]; b[(size_t) ((wpos + i) & 8191)] = io[c][i]; dry[c][i] = b[(size_t) ((wpos + i - lat) & 8191)];
            }

        // pre: BOOST and, in OVERDRIVE, the noise gate (host rate)
        const float sm = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const float boostT = p.boostOn ? dbToGain (p.boostDb) : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            boostS += (boostT - boostS) * sm;
            for (int c = 0; c < 2; ++c)
            {
                float x = io[c][i] * boostS;
                if (mode == Overdrive) x *= ch[c].gate.process (x, p.gateThresh * 0.1f, p.gateDecay * 0.1f, fs);
                io[c][i] = x;
            }
        }

        // the nonlinear stage at 4x
        const float gT = driveGain(p);
        const float smOs = 1.0f - std::exp (-1.0f / (0.03f * fs * 4.0f));
        if (mode == Distortion) for (auto& c : ch) c.ds.opamp.setBandwidth (gT);
        for (int i = 0; i < (n << 2); ++i) { gS += (gT - gS) * smOs; gOs[i] = gS; }
        juce::dsp::AudioBlock<float> block (io, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        if (mode != Crush)
        {
            const float bias = (1.0f - p.symmetry) * 0.8f;
            for (int c = 0; c < 2; ++c)
            {
                auto* u = up.getChannelPointer ((size_t) c);
                auto& s = ch[c];
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float g = gOs[i];
                    float x = u[i], y;
                    switch (mode)
                    {
                        case Overdrive: y = s.od.process (x, p.odType, g, p.odParam * 0.1f); break;
                        case Distortion: y = s.ds.process (p.variant == 1 ? s.tightHp.hp (x) : x, g); break;
                        case Tube:
                        {
                            y = sa1::tubeCurve (x, p.drive, p.variant == 0 ? p.tubeBias : 0.0f);
                            const float yd = s.dcBlock.hp (y); y = p.variant == 0 ? yd : y;                     // the lopsided curve makes DC
                            break;
                        }
                        case Tape: y = sa1::tapeCurve (x, p.drive); break;
                        default:
                        {
                            const int cc = p.variant == 0 ? hm2::Transistor : hm2::Fold;
                            const float uu = x * g * (cc == hm2::Fold ? 0.25f + p.fold * 1.5f : 0.25f);
                            y = hm2::saturateBiased (uu, cc, bias);
                            y = s.dcBlock.hp (y);
                            break;
                        }
                    }
                    u[i] = std::isfinite (y) ? y : 0.0f;
                }
            }
        }
        os->processSamplesDown (block);

        // post (host rate): tone, tape filters, crush, level trim, MIX, AUTO LEVEL, LEVEL
        const float trim = mode == Overdrive ? dbToGain (od2::typeDef (p.odType).trimDb) : mode == Distortion ? ds1::levelGain (0.5f) : mode == Fold ? 0.9f : 1.0f;
        const float fadeInc = 1.0f / (0.03f * fs), pm = 1.0f - std::exp (-1.0f / (1.0f * fs)), am = 1.0f - std::exp (-1.0f / (0.4f * fs));
        const float rateInc = std::min (1.0f, p.rate / fs), levels = std::exp2 (std::round (p.bits) - 1.0f);
        const bool smooth = p.variant == 1;
        float outPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            fade = std::min (1.0f, fade + fadeInc);
            const bool newHold = (counter += rateInc) >= 1.0f;
            if (newHold) counter -= 1.0f;
            float o[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                float y = io[c][i];
                switch (mode)
                {
                    case Overdrive: y = s.stack.process (y); break;
                    case Distortion: y = s.dsTone.process (y, p.tone); break;
                    case Tape:
                    {
                        const float lp = s.bumpLp.lp (y);
                        y += lp * 0.55f * p.warmth;                                                                  // the head bump
                        y = s.tape2.lp (s.tape1.lp (y));                                                             // the high roll-off
                        y = s.high.process (s.low.process (y));
                        break;
                    }
                    case Crush:
                    {
                        const float xin = y * dbToGain (18.0f * p.drive);
                        if (newHold)
                        {
                            const float lsb = 1.0f / levels, dither = smooth ? (rnd() - rnd()) * lsb * 0.5f : 0.0f;
                            s.hold = std::round ((xin + dither) * levels) / levels;
                        }
                        float q = s.hold; if (smooth) q = s.crushLp.lp (q);
                        y = xin + (q - xin) * p.crushMix;
                        break;
                    }
                    default: y = s.high.process (s.low.process (y)); break;
                }
                o[c] = y * trim;
            }
            for (int c = 0; c < 2; ++c) { const float d = dry[c][i]; o[c] = d + (o[c] - d) * p.mix; }
            const float inE = 0.5f * (dry[0][i] * dry[0][i] + dry[1][i] * dry[1][i]), outE = 0.5f * (o[0] * o[0] + o[1] * o[1]);
            inPow += pm * (inE - inPow); outPow += pm * (outE - outPow);
            const float target = p.autoLevel ? std::clamp (std::sqrt (inPow / std::max (outPow, 1.0e-10f)), 0.03f, 16.0f) : 1.0f;
            autoGain += am * (target - autoGain);
            for (int c = 0; c < 2; ++c)
            {
                float y = o[c] * autoGain * levelGain;
                y = dry[c][i] + (y - dry[c][i]) * fade;
                y = std::isfinite (y) ? std::clamp (y, -16.0f, 16.0f) : 0.0f;
                io[c][i] = y; outPeak = std::max (outPeak, std::abs (y));
            }
        }
        wpos += n;
        const float decay = std::pow (0.88f, (float) n / 512.0f);
        inLevel.store (std::max (inPeak, inLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
        outLevel.store (std::max (outPeak, outLevel.load (std::memory_order_relaxed) * decay), std::memory_order_relaxed);
        dotIn.store (std::clamp (dry[0][n - 1] * boostS, -1.0f, 1.0f), std::memory_order_relaxed);
    }

    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (1.0f / 16777216.0f); }

    float fs = 48000.0f, gS = 1.0f, boostS = 1.0f, autoGain = 1.0f, inPow = 1.0e-6f, outPow = 1.0e-6f, fade = 1.0f, counter = 1.0f, bump[2] {};
    int lat = 0, wpos = 0, lastMode = -1, lastVariant = -1, lastOdType = -1;
    unsigned seed = 12345u;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    Chan ch[2];
    std::vector<float> dryDelay[2];
    float dry[2][kChunk] {};
    float gOs[kChunk * 4] {};
};
} // namespace dv1
