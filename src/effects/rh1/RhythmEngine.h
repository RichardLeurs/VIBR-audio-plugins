// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// RH-1 Rhythm Lab: one control engine and three destinations (see .claude/lab_specs.md). A control signal moves one parameter in time with the music:
//   SOURCE       LFO (sine - triangle - square - random), ENVELOPE (the playing moves it: auto-wah, ducking) or STEPS (a 16-step lane with glide and swing)
//   DESTINATION  FILTER (the MF-1: LOW PASS, BAND PASS, HIGH PASS, NOTCH, VOWEL: the cutoff moves up to 5 octaves)
//                TREMOLO (the TR-2: SMOOTH; HARMONIC: lows and highs in opposite phase; PANNER: left and right in opposite phase)
//                SLICER  (the SL-2: the step lane gates the sound; SINGLE, DUAL, TREMOLO, SFX, HARMONIC)
// The clock is the position `pos` in note lengths (1/16 by default): with SYNC it is locked to the host position while the host plays and follows the tempo
// otherwise; without SYNC the RATE is in Hz (cycles of the LFO, steps per second of the lane). The SLICER always follows the tempo. SWING delays the odd steps.
// Header-only, no JUCE dependency, no allocation after prepare(). No latency.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace rh1
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;
constexpr int kNumModes = 3, kSteps = 16, kNumDivisions = 9, kNumPatterns = 11, kHistory = 256;
enum Mode { Filter, Tremolo, Slicer };
enum Source { Lfo, Envelope, Steps };
enum FilterType { LowPass, BandPass, HighPass, Notch, Vowel };
enum TremType { Smooth, Harmonic, Panner };
enum SlicerMode { Single, Dual, SlTremolo, Sfx, SlHarmonic };

inline int clampMode (int m) { return std::clamp (m, 0, kNumModes - 1); }
inline const char* modeName (int m) { static const char* n[kNumModes] = { "FILTER", "TREMOLO", "SLICER" }; return n[clampMode (m)]; }
inline const char* sourceName (int s) { static const char* n[3] = { "LFO", "ENVELOPE", "STEPS" }; return n[std::clamp (s, 0, 2)]; }
inline int variantCount (int mode) { return mode == Filter ? 5 : mode == Tremolo ? 3 : 5; }
inline const char* variantName (int mode, int v)
{
    static const char* f[5] = { "LOW PASS", "BAND PASS", "HIGH PASS", "NOTCH", "VOWEL" }, *t[3] = { "SMOOTH", "HARMONIC", "PANNER" }, *s[5] = { "SINGLE", "DUAL", "TREMOLO", "SFX", "HARMONIC" };
    v = std::clamp (v, 0, variantCount (mode) - 1);
    return mode == Filter ? f[v] : mode == Tremolo ? t[v] : s[v];
}
inline const char* divisionName (int i)
{
    static const char* n[kNumDivisions] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/8 DOT" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline float divisionBeats (int i)                                    // a beat = a quarter note
{
    static const float b[kNumDivisions] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f, 0.75f };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
// the 11 patterns of the SL-2 (16 steps of 16th notes)
inline const char* patternString (int i)
{
    static const char* const p[kNumPatterns] = { "x...x...x...x...", "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx", "x..x..x.x..x..x.", "x.xx.xx.x.xx.xx.", "xx..xx..xx..xx..",
                                                 "x.x..x.x.x..x.x.", "xxx.xxx.xxx.xxx.", "x..xx..xx..xx..x", ".x.x.x.x.x.x.x.x", "xx.xx.x.xx.xx.x." };
    return p[std::clamp (i, 0, kNumPatterns - 1)];
}

// the curves of the MF-1, TR-2 and SL-2
inline float cutoffHz (float v) { return 60.0f * std::pow (200.0f, std::clamp (v, 0.0f, 1.0f)); }
inline float decayMs (float v) { return 20.0f * std::pow (40.0f, std::clamp (v, 0.0f, 1.0f)); }
inline float sensGain (float v) { return std::pow (30.0f, std::clamp (v, 0.0f, 1.0f)); }
inline float tremLfo (float phase, float shape01)                    // -1 .. 1, sine (0) .. square (1), as the TR-2
{
    const float s = std::sin (kTwoPi * phase), k = 1.0f + 30.0f * shape01 * shape01;
    return std::tanh (k * s) / std::tanh (k);
}
inline float slAttackMs (float a01) { return 0.5f * std::pow (100.0f, a01); }
inline float slDuty (float d01) { return 0.1f + 0.9f * d01; }
inline float coefMs (float ms, float fs) { return 1.0f - std::exp (-1.0f / (0.001f * std::max (0.05f, ms) * fs)); }

struct Params
{
    int mode = Filter, variant = 0, source = Lfo;
    bool sync = false, hostPlaying = false;
    int division = 4;
    double hostBeats = 0.0;                                           // the host position (quarter notes) at the start of the block
    float bpm = 120.0f;
    float rate = 3.0f, shape = 0.0f, depth = 0.6f, sens = 0.35f, decay = 0.4f, swing = 0.0f, glide = 0.0f;       // Hz; the rest 0 .. 1 (swing 0 .. 0.75)
    float steps[kSteps] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
    float cutoff = 0.4f, res = 0.55f;                                 // FILTER
    float tremStereo = 0.0f, tremBias = 0.0f;                         // TREMOLO
    float slAttack = 0.3f, slDuty = 1.0f, slBalance = 0.7f;           // SLICER (0 .. 1)
    float mix = 1.0f;
};

class Engine
{
public:
    std::atomic<float> histCtl[kHistory], histDest[kHistory];         // one entry per 20 ms: the control signal (0 .. 1) and the destination (cutoff / gain / gate, 0 .. 1)
    std::atomic<int> histPos { 0 }, currentStep { 0 };

    void prepare (float sampleRate) { fs = sampleRate; histWindow = std::max (1, (int) (0.02f * fs)); reset(); }

    void reset()
    {
        for (auto& c : ch) c = Chan();
        pos = 0.0; env = 0.0f; stepVal = 0.0f; shRand = shSm = 0.0f; shPrev = 0; mod = 0.0f; uni = 0.0f; stepIdx = 0; stepFrac = 0.0f;
        rng = 0x2468ace1u; envA = envB = 0.0f; envTa = envTb = 0.15f; balS = -1.0f; lastMode = -1; fade = 1.0f; dryG = 0.0f; wetG = 1.0f;
        winCount = 0; winCtl = winDest = 0.0f;
        for (int i = 0; i < kHistory; ++i) { histCtl[i].store (0.0f); histDest[i].store (0.0f); }
        histPos.store (0); currentStep.store (0);
    }

    float lastModulation() const { return mod; }

    void process (float* l, float* r, int n, const Params& p, float levelGain)
    {
        if (p.mode != lastMode) { if (lastMode >= 0) { for (auto& c : ch) c = Chan(); envA = envB = 0.0f; envTa = envTb = 0.15f; fade = 0.0f; } lastMode = p.mode; }
        const bool slicer = p.mode == Slicer, clockTempo = p.sync || slicer;
        const float divBeats = divisionBeats (p.division);
        double inc = clockTempo ? (double) p.bpm / 60.0 / (double) divBeats / (double) fs : (double) p.rate / (double) fs;
        if (clockTempo && p.hostPlaying) pos = p.hostBeats / (double) divBeats;
        const float stepSec = clockTempo ? 60.0f / p.bpm * divBeats : 1.0f / std::max (0.05f, p.rate);
        const float fadeInc = 1.0f / (0.03f * fs);
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            const float x[2] = { std::isfinite (l[i]) ? l[i] : 0.0f, std::isfinite (r[i]) ? r[i] : 0.0f };
            advance (p, 0.5f * (x[0] + x[1]), stepSec, inc);
            float y[2], destVal = 0.0f;
            switch (p.mode)
            {
                case Filter:
                {
                    if ((i & 15) == 0) computeFilter (p);
                    for (int c = 0; c < 2; ++c) y[c] = 1.5f * std::tanh (filter (ch[c], x[c], p) / 1.5f);
                    destVal = std::clamp (std::log (std::clamp (cutHz / 60.0f, 1.0f, 200.0f)) / std::log (200.0f), 0.0f, 1.0f);
                    break;
                }
                case Tremolo:
                {
                    const float depth = std::clamp (p.depth, 0.0f, 1.0f), floorG = std::clamp (p.tremBias, 0.0f, 1.0f);
                    float u = uni;                                                                  // 0 .. 1
                    if (p.source == Lfo) u = 0.5f + 0.5f * tremLfo (phase, p.shape);
                    const float g = floorG + (1.0f - floorG) * (1.0f - depth * (1.0f - u));
                    destVal = g;
                    if (p.variant == Smooth)
                    {
                        // STEREO: 0 = the same on both channels, 1 = the right channel in opposite phase
                        const float uR = u + (1.0f - 2.0f * u) * std::clamp (p.tremStereo, 0.0f, 1.0f);
                        const float gR = floorG + (1.0f - floorG) * (1.0f - depth * (1.0f - uR));
                        y[0] = x[0] * g; y[1] = x[1] * gR;
                    }
                    else if (p.variant == Harmonic)
                    {
                        const float gHi = floorG + (1.0f - floorG) * (1.0f - depth * u);              // the highs in opposite phase
                        for (int c = 0; c < 2; ++c) { ch[c].lo += lowCoef () * (x[c] - ch[c].lo); y[c] = ch[c].lo * g + (x[c] - ch[c].lo) * gHi; }
                    }
                    else                                                                            // PANNER: left and right in opposite phase (both from 1 down)
                    {
                        const float gL = floorG + (1.0f - floorG) * (1.0f - depth * (1.0f - u)), gR = floorG + (1.0f - floorG) * (1.0f - depth * u);
                        y[0] = x[0] * gL; y[1] = x[1] * gR;
                    }
                    break;
                }
                default:                                                                            // SLICER (the SL-2)
                {
                    const bool on = p.steps[stepIdx & 15] >= 0.5f;
                    const float duty = slDuty (p.slDuty), u = stepFrac;
                    const float atkC = coefMs (slAttackMs (p.slAttack), fs), relC = coefMs (4.0f, fs), swellC = coefMs (0.5f * stepSec * 1000.0f, fs);
                    const float gateA = (on && u < duty) ? 1.0f : 0.0f, gateB = (! on && u < duty) ? 1.0f : 0.0f;
                    envA += (gateA - envA) * (gateA > envA ? atkC : relC);
                    envB += (gateB - envB) * (gateB > envB ? atkC : relC);
                    envTa += ((on ? 1.0f : 0.15f) - envTa) * swellC;
                    envTb += ((on ? 0.15f : 1.0f) - envTb) * swellC;
                    if (balS < 0.0f) balS = p.slBalance;
                    balS += (p.slBalance - balS) * 0.001f;
                    const float dryGain = std::min (1.0f, 2.0f * (1.0f - balS)), wetGain = std::min (1.0f, 2.0f * balS);
                    for (int c = 0; c < 2; ++c)
                    {
                        float wet = 0.0f;
                        switch (p.variant)
                        {
                            case Single: wet = x[c] * envA; break;
                            case Dual: wet = x[c] * (c == 0 ? envA : envB); break;
                            case SlTremolo: wet = x[c] * envTa; break;
                            case Sfx: wet = svf (ch[c], x[c], 250.0f + 5500.0f * envA) * (0.15f + 0.85f * envA) * 1.6f; break;
                            default: { ch[c].lo += lowCoef () * (x[c] - ch[c].lo); wet = ch[c].lo * envTa + (x[c] - ch[c].lo) * envTb; break; }
                        }
                        y[c] = x[c] * dryGain + wet * wetGain;
                    }
                    destVal = p.variant == Dual ? 0.5f * (envA + envB) : p.variant == SlTremolo || p.variant == SlHarmonic ? envTa : envA;
                    break;
                }
            }
            fade = std::min (1.0f, fade + fadeInc);
            for (int c = 0; c < 2; ++c)
            {
                float o = (x[c] + (y[c] - x[c]) * p.mix) * levelGain;
                o = x[c] * (1.0f - fade) * levelGain + o * fade;                                  // a mode change fades in from the dry sound
                io[c][i] = std::isfinite (o) ? o : 0.0f;
            }
            winCtl = uni; winDest = destVal;
            if (++winCount >= histWindow)
            {
                const int hp = histPos.load (std::memory_order_relaxed);
                histCtl[hp].store (winCtl, std::memory_order_relaxed); histDest[hp].store (winDest, std::memory_order_relaxed);
                histPos.store ((hp + 1) % kHistory, std::memory_order_release);
                winCount = 0;
            }
        }
        if (! (clockTempo && p.hostPlaying)) pos += inc * 0.0;                                       // (the clock advances in advance())
    }

private:
    struct Chan { float s1 = 0, s2 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0, lo = 0, svLow = 0, svBand = 0; };

    uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float rand11() { return (float) (rnd() & 0xffffff) / 8388608.0f - 1.0f; }
    float lowCoef() const { return 1.0f - std::exp (-kTwoPi * 700.0f / fs); }
    float svf (Chan& c, float x, float cutoff) const
    {
        const float f = 2.0f * std::sin (kPi * std::min (cutoff, 0.2f * fs) / fs);
        c.svLow += f * c.svBand;
        const float high = x - c.svLow - 0.45f * c.svBand;
        c.svBand += f * high;
        return c.svLow;
    }

    // the control signal: `mod` as in the MF-1 (the LFO and the steps -1 .. 1, the envelope 0 .. 1), `uni` 0 .. 1, the step index and its fraction
    void advance (const Params& p, float xin, float stepSec, double inc)
    {
        if (p.source == Envelope)
        {
            const float a = std::fabs (xin) * sensGain (p.sens);
            const float rel = 1.0f / (decayMs (p.decay) * 0.001f * fs);
            env = a > env ? env + (a - env) * (1.0f / (0.002f * fs)) : env + (a - env) * rel;
            mod = std::clamp (std::tanh (env * 3.0f), 0.0f, 1.0f);
            uni = mod;
            pos += inc;                                                                            // (the clock still runs: the slicer and the display need it)
        }
        else
        {
            pos += inc;
            const double cyc = p.sync || p.mode == Slicer ? 1.0 : 1.0;                              // pos counts cycles (LFO) or steps (lane)
            (void) cyc;
            phase = (float) (pos - std::floor (pos));
            if (p.source == Lfo)
            {
                const float sh = std::clamp (p.shape, 0.0f, 1.0f) * 3.0f;
                const int i = std::min (2, (int) sh);
                const float f = sh - (float) i;
                const float sine = std::sin (kTwoPi * phase), tri = 1.0f - 4.0f * std::fabs (phase - 0.5f), sq = std::tanh (6.0f * sine);
                const int q = (int) (phase * 4.0f);
                if (q != shPrev) { shRand = rand11(); shPrev = q; }
                shSm += (shRand - shSm) * (1.0f / (0.004f * fs));
                const float v[4] = { sine, tri, sq, shSm };
                mod = v[i] + (v[i + 1] - v[i]) * f;
                uni = 0.5f + 0.5f * mod;
            }
        }
        // the step clock (also drives the slicer): pos in steps, odd steps delayed by SWING
        const double q = pos * 0.5, k = std::floor (q), t = q - k;
        const double b = 0.5 + 0.5 * (double) std::clamp (p.swing, 0.0f, 0.75f);
        int s; float f;
        if (t < b) { s = (int) k * 2; f = (float) (t / b); } else { s = (int) k * 2 + 1; f = (float) ((t - b) / (1.0 - b)); }
        stepIdx = ((s % kSteps) + kSteps) % kSteps; stepFrac = std::clamp (f, 0.0f, 0.9999f);
        if (stepIdx != currentStep.load (std::memory_order_relaxed)) currentStep.store (stepIdx, std::memory_order_relaxed);
        if (p.source == Steps)
        {
            const float target = p.steps[stepIdx] * 2.0f - 1.0f;
            const float glideSec = std::max (0.0005f, p.glide * p.glide * stepSec * 1.2f);
            stepVal += (target - stepVal) * (1.0f / (glideSec * fs));
            mod = stepVal; uni = 0.5f + 0.5f * mod;
        }
    }

    // the filter of the MF-1
    void computeFilter (const Params& p)
    {
        const float oct = mod * p.depth * 5.0f;
        float fc = cutoffHz (std::clamp (p.cutoff, 0.0f, 1.0f)) * std::pow (2.0f, oct);
        fc = std::clamp (fc, 30.0f, 0.45f * fs);
        cutHz = fc;
        g = std::tan (kPi * fc / fs);
        k = 2.0f - 1.96f * std::clamp (p.res, 0.0f, 1.0f);
        if (p.variant == Vowel)
        {
            const float vp = std::clamp (std::log (fc / 60.0f) / std::log (200.0f), 0.0f, 1.0f) * 4.0f;
            static const float f1[5] = { 800, 400, 350, 450, 325 }, f2[5] = { 1150, 1600, 2300, 800, 700 };
            const int i0 = std::min (3, (int) vp);
            const float fr = vp - (float) i0;
            gv1 = std::tan (kPi * (f1[i0] + fr * (f1[i0 + 1] - f1[i0])) / fs);
            gv2 = std::tan (kPi * (f2[i0] + fr * (f2[i0 + 1] - f2[i0])) / fs);
            gv3 = std::tan (kPi * 2800.0f / fs);
            kv = 1.0f / (3.0f + 11.0f * std::clamp (p.res, 0.0f, 1.0f));
        }
    }
    static void svfTpt (float& s1, float& s2, float x, float g, float k, float& lo, float& bp, float& hp)
    {
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        const float v1 = g * hp; bp = v1 + s1; s1 = bp + v1;
        const float v2 = g * bp; lo = v2 + s2; s2 = lo + v2;
    }
    float filter (Chan& c, float x, const Params& p)
    {
        float lo, bp, hp;
        switch (p.variant)
        {
            case LowPass:  svfTpt (c.s1, c.s2, x, g, k, lo, bp, hp); return lo;
            case BandPass: svfTpt (c.s1, c.s2, x, g, k, lo, bp, hp); return bp * k * (1.0f + 0.5f * (2.0f - k));
            case HighPass: svfTpt (c.s1, c.s2, x, g, k, lo, bp, hp); return hp;
            case Notch:    svfTpt (c.s1, c.s2, x, g, k, lo, bp, hp); return lo + hp;
            default:
            {
                float o1, o2, o3, h;
                svfTpt (c.s1, c.s2, x, gv1, kv, lo, o1, h);
                svfTpt (c.a1, c.a2, x, gv2, kv, lo, o2, h);
                svfTpt (c.b1, c.b2, x, gv3, kv, lo, o3, h);
                return 3.0f * kv * (1.4f * o1 + o2 + 0.4f * o3) * 2.0f;
            }
        }
    }

    float fs = 48000.0f, phase = 0.0f, env = 0.0f, stepVal = 0.0f, shRand = 0.0f, shSm = 0.0f, mod = 0.0f, uni = 0.0f, stepFrac = 0.0f, fade = 1.0f, dryG = 0.0f, wetG = 1.0f;
    float g = 0.1f, k = 1.0f, gv1 = 0.1f, gv2 = 0.1f, gv3 = 0.1f, kv = 0.2f, cutHz = 1000.0f, envA = 0.0f, envB = 0.0f, envTa = 0.15f, envTb = 0.15f, balS = -1.0f, winCtl = 0.0f, winDest = 0.0f;
    double pos = 0.0;
    int stepIdx = 0, shPrev = 0, lastMode = -1, histWindow = 960, winCount = 0;
    uint32_t rng = 0x2468ace1u;
    Chan ch[2];
};
} // namespace rh1
