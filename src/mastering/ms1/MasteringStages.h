// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MS-1 stages of version 0.4 (see .claude/mastering_specs.md), on top of MasteringEngine.h:
//   Crossover        Linkwitz-Riley 24 dB per octave band splitter (2 or 3 splits); the bands add up to an all-pass (flat magnitude)
//   MultibandComp    4 bands, per band threshold / ratio / attack / release (or auto) / make-up / solo, stereo-linked detection
//   Imager           3 bands, per band width (0 .. 200 %), mono-below, mid / side balance
//   Colour           tape / tube / transformer saturation in 4x oversampling, tone tilt, mid / side or stereo, mix, auto-gain
// Header-only, no JUCE dependency, no allocation after prepare().
#include <complex>
#include "MasteringEngine.h"

namespace ms1
{
//==============================================================================
// 2nd-order Butterworth biquad, low or high pass; two of them in series are a Linkwitz-Riley 24 dB per octave filter
struct Butter2
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (float hz, float fs, bool high)
    {
        const double K = std::tan (3.14159265358979 * (double) std::clamp (hz, 10.0f, 0.45f * fs) / (double) fs), q = 0.70710678118, a0 = 1.0 + K / q + K * K;
        const double n0 = high ? 1.0 : K * K, n1 = high ? -2.0 : 2.0 * K * K, n2 = n0;
        b0 = (float) (n0 / a0); b1 = (float) (n1 / a0); b2 = (float) (n2 / a0);
        a1 = (float) (2.0 * (K * K - 1.0) / a0); a2 = (float) ((1.0 - K / q + K * K) / a0);
    }
    float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void clear() { z1 = z2 = 0.0f; }
};
struct Lr4
{
    Butter2 s[2];
    void set (float hz, float fs, bool high) { s[0].set (hz, fs, high); s[1].set (hz, fs, high); }
    float tick (float x) { return s[1].tick (s[0].tick (x)); }
    void clear() { s[0].clear(); s[1].clear(); }
};

// N splits -> N + 1 bands. Band i (i < N) is the low part of split i of what the lower splits left over; the bands below the top ones are passed through the
// all-pass (low + high) of every higher split, so the sum of the bands has a flat magnitude.
template <int N>
class Crossover
{
public:
    static constexpr int kBands = N + 1;
    void set (const float* freqs, float fs)
    {
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < N; ++j)
            {
                lp[c][j].set (freqs[j], fs, false); hp[c][j].set (freqs[j], fs, true);
                for (int i = 0; i <= j; ++i) { cLp[c][i][j].set (freqs[j], fs, false); cHp[c][i][j].set (freqs[j], fs, true); }
            }
    }
    void clear()
    {
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < N; ++j)
            {
                lp[c][j].clear(); hp[c][j].clear();
                for (int i = 0; i < N; ++i) { cLp[c][i][j].clear(); cHp[c][i][j].clear(); }
            }
    }
    void split (int c, float x, float* out)           // out[kBands]
    {
        float rest = x;
        for (int j = 0; j < N; ++j) { out[j] = lp[c][j].tick (rest); rest = hp[c][j].tick (rest); }
        out[N] = rest;
        for (int i = 0; i < N; ++i)
            for (int j = i + 1; j < N; ++j) out[i] = cLp[c][i][j].tick (out[i]) + cHp[c][i][j].tick (out[i]);
    }
private:
    Lr4 lp[2][N], hp[2][N], cLp[2][N][N], cHp[2][N][N];
};

//==============================================================================
// Multiband compressor
struct MbBand { bool on = true, solo = false, autoRelease = false; float thresholdDb = -18.0f, ratio = 2.0f, attackMs = 10.0f, releaseMs = 200.0f, makeupDb = 0.0f; };
struct MbParams
{
    bool on = false;
    float mix = 1.0f;
    float freq[3] = { 120.0f, 1200.0f, 6000.0f };
    float kneeDb = 6.0f, rmsBlend = 0.0f, sidechainHz = 20.0f;           // knee 0 .. 12 dB; detector 0 = peak .. 1 = RMS; sidechain high-pass (<= 20 Hz = off)
    MbBand band[4];
};

class MultibandComp
{
public:
    std::atomic<float> gr[4];                          // the gain reduction of each band now (dB, <= 0)

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset() { xo.clear(); for (auto& b : st) b = Env(); for (auto& h : scHp) for (auto& f : h) f = Butter2(); scHz = -1.0f; act = 0.0f; for (auto& g : gr) g.store (0.0f); }

    // the minimum-phase crossovers (Linkwitz-Riley 24 dB per octave, no latency)
    void process (float* l, float* r, int n, const MbParams& p)
    {
        xo.set (p.freq, fs);
        run (l, r, n, p, [this] (int i, const float* il, const float* ir, float* bl, float* br) { xo.split (0, il[i], bl); xo.split (1, ir[i], br); });
    }

    // the linear-phase crossovers: bl[k] / br[k] hold the n samples of band k (made by the host code with the FIRs of designLinearBands); the result is the sum of the
    // compressed bands, mixed with the sum of the untouched bands (which is the input delayed by the FIR delay)
    void processBands (float* l, float* r, int n, const MbParams& p, const float* const* bl, const float* const* br)
    {
        run (l, r, n, p, [bl, br] (int i, const float*, const float*, float* ol, float* orr) { for (int k = 0; k < 4; ++k) { ol[k] = bl[k][i]; orr[k] = br[k][i]; } }, true);
    }

private:
    template <class Split>
    void run (float* l, float* r, int n, const MbParams& p, Split split, bool external = false)
    {
        if (! p.on && act <= 0.0f)                                                      // off: untouched (linear phase: the sum of the bands, i.e. the delayed input)
        {
            for (auto& e : st) e = Env();
            for (auto& g : gr) g.store (0.0f, std::memory_order_relaxed);
            if (external) for (int i = 0; i < n; ++i) { float bl[4], br[4]; split (i, l, r, bl, br); l[i] = bl[0] + bl[1] + bl[2] + bl[3]; r[i] = br[0] + br[1] + br[2] + br[3]; }
            return;
        }
        bool anySolo = false;
        for (const auto& b : p.band) anySolo = anySolo || b.solo;
        const float actInc = 1.0f / (0.02f * fs), knee = std::max (0.0f, p.kneeDb), rmsC = std::exp (-1.0f / (0.01f * fs)), rmsB = std::clamp (p.rmsBlend, 0.0f, 1.0f);
        const bool scOn = p.sidechainHz > 20.5f;
        if (p.sidechainHz != scHz) { scHz = p.sidechainHz; if (scOn) for (auto& h : scHp) for (auto& f : h) f.set (scHz, fs, true); }
        struct Coef { float slope, atk, rel, relSlow, makeup; } cf[4];
        for (int k = 0; k < 4; ++k)
        {
            const auto& b = p.band[k];
            cf[k].slope = 1.0f / std::max (1.0f, b.ratio) - 1.0f;
            cf[k].atk = std::exp (-1.0f / (std::max (0.05f, b.attackMs) * 0.001f * fs));
            cf[k].rel = std::exp (-1.0f / ((b.autoRelease ? 150.0f : std::max (5.0f, b.releaseMs)) * 0.001f * fs));
            cf[k].relSlow = std::exp (-1.0f / (1.5f * fs));
            cf[k].makeup = dbToGain (b.makeupDb);
        }
        float minGr[4] = { 0, 0, 0, 0 };
        const float* il = l; const float* ir = r;
        for (int i = 0; i < n; ++i)
        {
            act = p.on ? std::min (1.0f, act + actInc) : std::max (0.0f, act - actInc);
            float bl[4], br[4];
            split (i, il, ir, bl, br);
            float sumL = 0.0f, sumR = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const auto& b = p.band[k];
                float g = 1.0f;
                if (b.on)
                {
                    float dl = bl[k], dr = br[k];
                    if (scOn) { dl = scHp[k][0].tick (dl); dr = scHp[k][1].tick (dr); }
                    Env& e = st[(size_t) k];
                    const float peak = std::max (std::abs (dl), std::abs (dr));
                    e.ms = rmsC * e.ms + (1.0f - rmsC) * std::max (dl * dl, dr * dr);
                    const float level = gainToDb ((1.0f - rmsB) * peak + rmsB * std::sqrt (e.ms) * 1.4142f), over = level - b.thresholdDb;
                    float red = 0.0f;
                    if (knee < 0.01f) { if (over > 0.0f) red = -cf[k].slope * over; }
                    else if (2.0f * over > knee) red = -cf[k].slope * over;
                    else if (2.0f * over > -knee) { const float x = over + knee * 0.5f; red = -cf[k].slope * x * x / (2.0f * knee); }
                    e.fast = red > e.fast ? cf[k].atk * e.fast + (1.0f - cf[k].atk) * red : cf[k].rel * e.fast + (1.0f - cf[k].rel) * red;
                    float gr2 = e.fast;
                    if (b.autoRelease)
                    {
                        e.slow = red > e.slow ? cf[k].atk * e.slow + (1.0f - cf[k].atk) * red : cf[k].relSlow * e.slow + (1.0f - cf[k].relSlow) * red;
                        gr2 = 0.5f * (e.fast + e.slow);
                    }
                    g = dbToGain (-gr2) * cf[k].makeup;
                    minGr[k] = std::min (minGr[k], -gr2);
                }
                if (anySolo && ! b.solo) continue;                                         // not soloed: left out of the sum
                sumL += bl[k] * g; sumR += br[k] * g;
            }
            const float m = p.mix * act;
            const float dryL = external ? bl[0] + bl[1] + bl[2] + bl[3] : l[i], dryR = external ? br[0] + br[1] + br[2] + br[3] : r[i];       // linear phase: the dry sound is the sum of the raw bands
            l[i] = dryL + (sumL - dryL) * m;
            r[i] = dryR + (sumR - dryR) * m;
        }
        for (int k = 0; k < 4; ++k) gr[k].store (minGr[k], std::memory_order_relaxed);
    }

    struct Env { float fast = 0.0f, slow = 0.0f, ms = 0.0f; };
    float fs = 48000.0f, act = 0.0f, scHz = -1.0f;
    Crossover<3> xo;
    Env st[4];
    Butter2 scHp[4][2];
};

//==============================================================================
// Stereo imager: 3 bands, width per band (the side signal x width; the mid signal is never touched by the width), mono-below (the side signal below
// the frequency is removed with a 24 dB per octave high-pass: the lows are mono), balance (mid +x / side -x, in dB)
struct ImagerParams
{
    bool on = false;
    float freq[2] = { 200.0f, 4000.0f }, width[3] = { 1.0f, 1.0f, 1.0f }, monoBelowHz = 0.0f, balanceDb = 0.0f;       // monoBelowHz 0 = off
    bool safe = false;                                 // auto safe width: the side level of a band backs off when its phase correlation goes below safeThreshold
    float safeThreshold = -0.2f;
};

class Imager
{
public:
    std::atomic<float> correlation { 1.0f };           // the phase correlation of the output, -1 .. +1 (slow)
    std::atomic<float> safeScale[3];
    std::atomic<float> sideMidDb[3];                   // the measured side level against the mid level of each band after the width (dB; 0 = a pure side signal and a mono one balance, < -20 almost mono)                   // the side level auto safe width leaves each band now (1 = untouched)
    static constexpr int kScope = 512;                 // the last output samples (every 4th), for the goniometer
    float scopeL[kScope] {}, scopeR[kScope] {};
    std::atomic<int> scopePos { 0 };

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset() { xo.clear(); sideHp.clear(); act = 0.0f; ll = rr = lr = 0.0f; hpHz = -1.0f; correlation.store (1.0f); for (auto& m : mm) m = 0.0f; for (auto& m : ss) m = 0.0f; for (auto& k : scale) k = 1.0f; for (auto& k : safeScale) k.store (1.0f); for (auto& k : sideMidDb) k.store (-60.0f); }

    void process (float* l, float* r, int n, const ImagerParams& p)
    {
        if (! p.on && act <= 0.0f) return;                                                 // off: untouched
        xo.set (p.freq, fs);
        if (p.monoBelowHz != hpHz) { hpHz = p.monoBelowHz; if (hpHz > 0.0f) sideHp.set (hpHz, fs, true); }
        const float actInc = 1.0f / (0.02f * fs), gm = dbToGain (0.5f * p.balanceDb), gs = dbToGain (-0.5f * p.balanceDb), cc = std::exp (-1.0f / (0.3f * fs)), sa = 1.0f - std::exp (-1.0f / (0.02f * fs)), sr = 1.0f - std::exp (-1.0f / (0.25f * fs));
        for (int i = 0; i < n; ++i)
        {
            act = p.on ? std::min (1.0f, act + actInc) : std::max (0.0f, act - actInc);
            float bl[3], br[3];
            xo.split (0, l[i], bl); xo.split (1, r[i], br);
            float mid = 0.0f, side = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float m = 0.5f * (bl[k] + br[k]), sd = 0.5f * (bl[k] - br[k]) * p.width[k];
                mm[k] = cc * mm[k] + (1.0f - cc) * m * m; ss[k] = cc * ss[k] + (1.0f - cc) * sd * sd;
                float target = 1.0f;                                                     // auto safe width: L = M + S, R = M - S, so the correlation is (M2 - S2) / (M2 + S2)
                if (p.safe) { const float corr = (mm[k] - ss[k]) / (mm[k] + ss[k] + 1.0e-12f); if (corr < p.safeThreshold) target = std::clamp (1.0f - (p.safeThreshold - corr) * 1.25f, 0.0f, 1.0f); }
                scale[k] += (target - scale[k]) * (target < scale[k] ? sa : sr);
                mid += m; side += sd * scale[k];
            }
            if (hpHz > 0.0f) side = sideHp.tick (side);
            mid *= gm; side *= gs;
            const float ol = mid + side, orr = mid - side;
            l[i] = l[i] + (ol - l[i]) * act;
            r[i] = r[i] + (orr - r[i]) * act;
            if ((i & 3) == 0) { const int w = scopePos.load (std::memory_order_relaxed); scopeL[w] = l[i]; scopeR[w] = r[i]; scopePos.store ((w + 1) % kScope, std::memory_order_relaxed); }
            ll = cc * ll + (1.0f - cc) * l[i] * l[i]; rr = cc * rr + (1.0f - cc) * r[i] * r[i]; lr = cc * lr + (1.0f - cc) * l[i] * r[i];
        }
        for (int k = 0; k < 3; ++k) { safeScale[k].store (scale[k], std::memory_order_relaxed); sideMidDb[k].store (std::clamp (10.0f * std::log10 ((ss[k] + 1.0e-12f) / (mm[k] + 1.0e-12f)), -60.0f, 30.0f), std::memory_order_relaxed); }
        correlation.store (ll * rr > 1.0e-12f ? std::clamp (lr / std::sqrt (ll * rr), -1.0f, 1.0f) : 1.0f, std::memory_order_relaxed);
    }

private:
    float fs = 48000.0f, act = 0.0f, ll = 0.0f, rr = 0.0f, lr = 0.0f, hpHz = -1.0f, mm[3] {}, ss[3] {}, scale[3] { 1.0f, 1.0f, 1.0f };
    Crossover<2> xo;
    Lr4 sideHp;
};

//==============================================================================
// Colour: saturation with three characters, always in 4x oversampling (a 97-tap Kaiser-windowed sinc, about 80 dB stop-band). The stage always delays
// the signal by kLatency samples (the oversampling filters), also when it is off, so the latency never changes while playing.
enum ColourChar { Tape, Tube, Transformer };
enum ColourMode { ColourStereo, ColourMid, ColourSide };
struct ColourParams
{
    bool on = false;
    int character = Tape, mode = ColourStereo;
    float drive = 0.0f, tiltDb = 0.0f, mix = 1.0f, outDb = 0.0f;       // drive, mix 0 .. 1
};

class Colour
{
public:
    static constexpr int kLatency = 24, kUp = 4, kTaps = 97, kRing = 128;

    void prepare (float sampleRate) { fs = sampleRate; reset(); }
    void reset()
    {
        for (auto& c : chan) c = Chan();
        for (auto& d : dry) std::fill (std::begin (d), std::end (d), 0.0f);
        dpos = 0; act = 0.0f; tiltLp[0] = tiltLp[1] = 0.0f;
    }

    void process (float* l, float* r, int n, const ColourParams& p)
    {
        float* io[2] = { l, r };
        const bool idle = ! p.on && act <= 0.0f;
        const float pre = 1.0f + 9.0f * p.drive * p.drive;
        const float fs4 = fs * (float) kUp;
        const float lp70 = 1.0f - std::exp (-kTwoPi * 70.0f / fs4), lp100 = 1.0f - std::exp (-kTwoPi * 100.0f / fs4), lpTop = 1.0f - std::exp (-kTwoPi * 14000.0f / fs4);
        const float dc = 1.0f - std::exp (-kTwoPi * 10.0f / fs4), tiltA = 1.0f - std::exp (-kTwoPi * 1000.0f / fs), actInc = 1.0f / (0.02f * fs);
        const float norm = 0.3f / std::max (1.0e-4f, shape (p.character, 0.3f * pre));          // auto-gain: the gain at a typical level stays 1
        const float outGain = dbToGain (p.outDb) * norm, tl = dbToGain (-0.5f * p.tiltDb), th = dbToGain (0.5f * p.tiltDb);
        const bool wetNeeded = p.drive > 0.0005f || std::abs (p.tiltDb) > 0.05f || std::abs (p.outDb) > 0.05f;
        for (int i = 0; i < n; ++i)
        {
            const float in[2] = { io[0][i], io[1][i] };
            for (int c = 0; c < 2; ++c) { dry[c][(size_t) (dpos & 63)] = in[c]; }
            const float dl = dry[0][(size_t) ((dpos - kLatency) & 63)], dr = dry[1][(size_t) ((dpos - kLatency) & 63)];
            ++dpos;
            act = (p.on && wetNeeded) ? std::min (1.0f, act + actInc) : std::max (0.0f, act - actInc);
            if (idle && act <= 0.0f) { io[0][i] = dl; io[1][i] = dr; continue; }

            // the part that is saturated: both channels, the mid or the side
            float x0, x1;
            if (p.mode == ColourStereo) { x0 = in[0]; x1 = in[1]; }
            else { const float m = 0.5f * (in[0] + in[1]), s = 0.5f * (in[0] - in[1]); x0 = p.mode == ColourMid ? m : s; x1 = 0.0f; }
            float y0 = saturate (chan[0], x0, pre, p.character, lp70, lp100, lpTop, dc);
            float y1 = p.mode == ColourStereo ? saturate (chan[1], x1, pre, p.character, lp70, lp100, lpTop, dc) : 0.0f;
            y0 *= outGain; y1 *= outGain;
            float wl, wr;
            if (p.mode == ColourStereo) { wl = y0; wr = y1; }
            else
            {
                const float dm = 0.5f * (dl + dr), ds = 0.5f * (dl - dr), c = p.mode == ColourMid ? dm : ds, d = y0 - c;
                if (p.mode == ColourMid) { wl = dl + d; wr = dr + d; } else { wl = dl + d; wr = dr - d; }
            }
            // tone tilt around 1 kHz
            tiltLp[0] += tiltA * (wl - tiltLp[0]); tiltLp[1] += tiltA * (wr - tiltLp[1]);
            wl = tiltLp[0] * tl + (wl - tiltLp[0]) * th; wr = tiltLp[1] * tl + (wr - tiltLp[1]) * th;
            const float m = p.mix * act;
            io[0][i] = dl + (wl - dl) * m;
            io[1][i] = dr + (wr - dr) * m;
        }
    }

    // the static curve of a character (for the display): input -> output at the given drive
    static float shape (int character, float x)
    {
        switch (character)
        {
            case Tube:        return std::tanh (x + 0.0f);
            case Transformer: return std::abs (x) < 1.0f ? x * (1.5f - 0.5f * x * x) : (x > 0.0f ? 1.0f : -1.0f);
            default:          return std::tanh (x);
        }
    }
    static float curve (int character, float x, float drive)
    {
        const float pre = 1.0f + 9.0f * drive * drive, norm = 0.3f / std::max (1.0e-4f, shape (character, 0.3f * pre));
        if (character == Tube) { const float b = 0.25f * drive * drive; return (std::tanh (pre * x + b) - std::tanh (b)) * norm; }
        return shape (character, pre * x) * norm;
    }

private:
    struct Chan
    {
        float hist[32] {};                  // the input history of the up-sampling filter
        float ring[kRing] {};               // the 4x stream before the down-sampling filter
        int hpos = 0, rpos = 0;
        float lowBump = 0.0f, lowXf = 0.0f, topLp = 0.0f, dcState = 0.0f;
    };

    static const std::array<float, kTaps>& taps()           // low-pass at the original Nyquist frequency, seen from the 4x rate; the sum is 4 (each polyphase branch has unity DC gain)
    {
        static const std::array<float, kTaps> t = []
        {
            std::array<float, kTaps> h {};
            const double mid = (kTaps - 1) * 0.5, beta = 9.0;
            const auto bessel0 = [] (double x) { double s = 1.0, tt = 1.0; for (int k = 1; k < 40; ++k) { tt *= (x / (2.0 * k)) * (x / (2.0 * k)); s += tt; } return s; };
            double sum = 0.0;
            for (int n = 0; n < kTaps; ++n)
            {
                const double x = ((double) n - mid) / (double) kUp, sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (3.14159265358979 * x) / (3.14159265358979 * x);
                const double u = ((double) n - mid) / mid, w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - u * u))) / bessel0 (beta);
                h[(size_t) n] = (float) (sinc * w); sum += h[(size_t) n];
            }
            for (auto& v : h) v = (float) ((double) v * (double) kUp / sum);
            return h;
        }();
        return t;
    }

    float saturate (Chan& c, float x, float pre, int character, float lp70, float lp100, float lpTop, float dcA)
    {
        const auto& h = taps();
        c.hist[c.hpos & 31] = x; c.hpos = (c.hpos + 1) & 31;
        float y = 0.0f;
        for (int ph = 0; ph < kUp; ++ph)
        {
            float u = 0.0f;                                                                  // the up-sampled value of this phase
            int k = 0;
            for (int idx = ph; idx < kTaps; idx += kUp, ++k) u += h[(size_t) idx] * c.hist[(c.hpos - 1 - k) & 31];
            const float v = nonlinear (c, u, pre, character, lp70, lp100, lpTop, dcA);
            c.ring[c.rpos & (kRing - 1)] = v; c.rpos = (c.rpos + 1) & (kRing - 1);
            if (ph == 0)                                                                     // the down-sampling filter, centred on the phase 0 sample of this frame
            {
                const float inv = 1.0f / (float) kUp;
                for (int n = 0; n < kTaps; ++n) y += h[(size_t) n] * inv * c.ring[(c.rpos - 1 - n) & (kRing - 1)];
            }
        }
        return y;
    }

    static float nonlinear (Chan& c, float u, float pre, int character, float lp70, float lp100, float lpTop, float dcA)
    {
        if (character == Tube)
        {
            const float b = 0.25f * std::clamp ((pre - 1.0f) / 9.0f, 0.0f, 1.0f) * 1.0f, y = std::tanh (pre * u + b) - std::tanh (b);
            c.dcState += dcA * (y - c.dcState);
            return y - c.dcState;
        }
        if (character == Transformer)
        {
            c.lowXf += lp100 * (u - c.lowXf);
            return shape (Transformer, pre * (u + 0.35f * c.lowXf));                         // odd harmonics, the lows driven harder
        }
        c.lowBump += lp70 * (u - c.lowBump);                                                 // tape: head bump, soft saturation, gentle top roll-off
        const float y = std::tanh (pre * (u + 0.25f * c.lowBump * std::clamp ((pre - 1.0f) / 9.0f, 0.0f, 1.0f)));
        c.topLp += lpTop * (y - c.topLp);
        return c.topLp;
    }

    float fs = 48000.0f, act = 0.0f, tiltLp[2] {};
    float dry[2][64] {};
    int dpos = 0;
    Chan chan[2];
};

//==============================================================================
// Linear-phase EQ: the summed magnitude response of the bands as a symmetric FIR (zero phase, shifted by N / 2 samples, Hann window), N = 4096 / 8192 /
// 16384 by sample rate. The plugin convolves with it (juce::dsp::Convolution) and reports N / 2 samples of latency. Two channels: the left / right pair (Stereo
// bands on both, Left / Right bands on one) or, when any band works on Mid or Side, the mid / side pair (Stereo bands on both, Mid and Side on one each;
// Left and Right bands then count as Stereo).
inline int linearIrLength (float fs) { return fs <= 50000.0f ? 4096 : fs <= 100000.0f ? 8192 : 16384; }

struct LinearIr { std::vector<float> a, b; bool midSide = false; };

inline void fftInPlace (std::vector<std::complex<double>>& x, bool inverse)
{
    const size_t n = x.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (x[i], x[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * 3.14159265358979323846 / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = x[i + k], v = x[i + k + len / 2] * w;
                x[i + k] = u + v; x[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) for (auto& v : x) v /= (double) n;
}

inline LinearIr designLinearIr (const std::array<EqBand, kEqBands>& bands, float fs, int n)
{
    LinearIr out;
    for (const auto& b : bands) if (b.on && std::abs (b.gainDb) > 0.005f && (b.mode == EqMid || b.mode == EqSide)) out.midSide = true;
    std::vector<std::complex<double>> xa ((size_t) n), xb ((size_t) n);
    for (int k = 0; k <= n / 2; ++k)
    {
        const float f = std::max (1.0f, (float) k * fs / (float) n);
        double da = 0.0, db = 0.0;
        for (int i = 0; i < kEqBands; ++i)
        {
            const auto& b = bands[(size_t) i];
            if (! b.on || std::abs (b.gainDb) <= 0.005f) continue;
            const double r = (double) eqResponseDb (i, b, f, fs);
            const bool stereoLike = b.mode == EqStereo || (out.midSide && (b.mode == EqLeft || b.mode == EqRight));         // in the mid / side domain Left and Right count as Stereo
            const bool a = stereoLike || (out.midSide ? b.mode == EqMid : b.mode == EqLeft);
            const bool bb = stereoLike || (out.midSide ? b.mode == EqSide : b.mode == EqRight);
            if (a) da += r;
            if (bb) db += r;
        }
        const double ma = std::pow (10.0, da / 20.0), mb = std::pow (10.0, db / 20.0);
        xa[(size_t) k] = ma; xb[(size_t) k] = mb;
        if (k > 0 && k < n / 2) { xa[(size_t) (n - k)] = ma; xb[(size_t) (n - k)] = mb; }
    }
    fftInPlace (xa, true); fftInPlace (xb, true);
    out.a.assign ((size_t) n, 0.0f); out.b.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * ((double) ((i + n / 2) % n) + 0.0) / (double) n);      // Hann, centred on N / 2
        const size_t dst = (size_t) ((i + n / 2) % n);
        out.a[dst] = (float) (xa[(size_t) i].real() * w);
        out.b[dst] = (float) (xb[(size_t) i].real() * w);
    }
    return out;
}

//==============================================================================
// Match EQ: a gentle (at most +-4 dB, smooth) fit of the 8 EQ bands to the difference between the long-term spectrum of a reference and of the input
// (both in the 128 bands of the spectrum display, dB). The overall level difference is removed first (the loudness is the limiter's job).
inline std::array<EqBand, kEqBands> matchEqFit (const float* refDb, const float* inDb)
{
    std::array<float, kBands> d {};
    std::array<bool, kBands> valid {};
    double sum = 0.0; int cnt = 0;
    const auto bandOf = [] (float hz) { return std::clamp ((int) std::lround (std::log (hz / 20.0f) / std::log (1000.0f) * (float) kBands - 0.5f), 0, kBands - 1); };
    for (int b = 0; b < kBands; ++b)
    {
        valid[(size_t) b] = refDb[b] > -100.0f && inDb[b] > -100.0f;
        d[(size_t) b] = valid[(size_t) b] ? refDb[b] - inDb[b] : 0.0f;
        if (valid[(size_t) b] && b >= bandOf (100.0f) && b <= bandOf (8000.0f)) { sum += d[(size_t) b]; ++cnt; }
    }
    const float offset = cnt > 0 ? (float) (sum / cnt) : 0.0f;
    std::array<float, kBands> sm {};
    for (int b = 0; b < kBands; ++b)                                                          // one octave of smoothing (about 13 bands)
    {
        double acc = 0.0; int n = 0;
        for (int k = std::max (0, b - 6); k <= std::min (kBands - 1, b + 6); ++k) if (valid[(size_t) k]) { acc += d[(size_t) k] - offset; ++n; }
        sm[(size_t) b] = n > 0 ? (float) (acc / n) : 0.0f;
    }
    static const float freq[kEqBands] = { 70.0f, 160.0f, 360.0f, 800.0f, 1800.0f, 3800.0f, 7000.0f, 11000.0f };
    std::array<EqBand, kEqBands> out;
    for (int i = 0; i < kEqBands; ++i)
    {
        const bool shelf = i == 0 || i == kEqBands - 1;
        int lo = bandOf (freq[i] / 1.41f), hi = bandOf (freq[i] * 1.41f);
        if (i == 0) lo = 0;
        if (i == kEqBands - 1) hi = kBands - 1;
        double acc = 0.0; int n = 0;
        for (int k = lo; k <= hi; ++k) { acc += sm[(size_t) k]; ++n; }
        const float avg = n > 0 ? (float) (acc / n) : 0.0f, g = std::round (4.0f * std::tanh (avg / 4.0f) * 10.0f) / 10.0f;
        out[(size_t) i].freq = freq[i]; out[(size_t) i].q = shelf ? 0.71f : 1.1f; out[(size_t) i].gainDb = std::abs (g) < 0.3f ? 0.0f : g;
        out[(size_t) i].on = true; out[(size_t) i].mode = EqStereo;
    }
    return out;
}

// Linear-phase crossover: the four bands of the multiband as zero-phase FIRs (Hann windowed, N taps, delay N / 2). The magnitudes are those of the Linkwitz-Riley
// 24 dB per octave filters (low-pass L = 1 / (1 + (f / fc)^4), high-pass 1 - L), band 1 = L1, band 2 = (1 - L1) L2, band 3 = (1 - L1)(1 - L2) L3, band 4 =
// (1 - L1)(1 - L2)(1 - L3), so the four FIRs add up to a pure delay of N / 2 samples.
inline std::array<std::vector<float>, 4> designLinearBands (const float* freq, float fs, int n)
{
    std::array<std::vector<std::complex<double>>, 4> x;
    for (auto& v : x) v.assign ((size_t) n, 0.0);
    for (int k = 0; k <= n / 2; ++k)
    {
        const double f = std::max (1.0, (double) k * (double) fs / (double) n);
        double lp[3];
        for (int j = 0; j < 3; ++j) { const double u = f / (double) freq[j], u2 = u * u; lp[j] = 1.0 / (1.0 + u2 * u2); }
        const double m[4] = { lp[0], (1.0 - lp[0]) * lp[1], (1.0 - lp[0]) * (1.0 - lp[1]) * lp[2], (1.0 - lp[0]) * (1.0 - lp[1]) * (1.0 - lp[2]) };
        for (int b = 0; b < 4; ++b) { x[(size_t) b][(size_t) k] = m[b]; if (k > 0 && k < n / 2) x[(size_t) b][(size_t) (n - k)] = m[b]; }
    }
    std::array<std::vector<float>, 4> out;
    for (int b = 0; b < 4; ++b)
    {
        fftInPlace (x[(size_t) b], true);
        out[(size_t) b].assign ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const size_t dst = (size_t) ((i + n / 2) % n);
            const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) dst / (double) n);
            out[(size_t) b][dst] = (float) (x[(size_t) b][(size_t) i].real() * w);
        }
    }
    return out;
}
} // namespace ms1
