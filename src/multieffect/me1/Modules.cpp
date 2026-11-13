// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#include "Modules.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace me1
{
namespace
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float gainToDb (float g) { return 8.68588964f * std::log (std::max (g, 1.0e-9f)); }
inline float pct (float v) { return v * 0.01f; }
inline float timeCoef (float seconds, float sr) { return std::exp (-1.0f / std::max (1.0f, seconds * sr)); }   // one-pole coefficient
inline float softClip (float x) { return std::tanh (x); }

// one-pole smoothing of a control value (per sample)
struct Smooth
{
    float v = 0.0f, c = 0.01f;
    bool primed = false;
    void prepare (double sr, float ms) { c = 1.0f - timeCoef (ms * 0.001f, (float) sr); primed = false; }
    float next (float target) { if (! primed) { v = target; primed = true; } v += (target - v) * c; return v; }
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setFc (float fc, double sr) { a = std::exp (-kTwoPi * fc / (float) sr); }
    float lp (float x) { z = x * (1.0f - a) + z * a; return z; }
    void reset() { z = 0.0f; }
};

// RBJ biquad (direct form II transposed)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0.0f; }
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        const float i = 1.0f / na0;
        b0 = nb0 * i; b1 = nb1 * i; b2 = nb2 * i; a1 = na1 * i; a2 = na2 * i;
    }
    void lowpass (float fc, float q, double sr)
    {
        const float w = kTwoPi * std::min (fc, 0.45f * (float) sr) / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void highpass (float fc, float q, double sr)
    {
        const float w = kTwoPi * std::min (fc, 0.45f * (float) sr) / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void peak (float fc, float q, float gainDb, double sr)
    {
        const float A = std::exp (gainDb * 0.0575646f), w = kTwoPi * fc / (float) sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float fc, float gainDb, double sr)
    {
        const float A = std::exp (gainDb * 0.0575646f), w = kTwoPi * fc / (float) sr, c = std::cos (w), sq = 2.0f * std::sqrt (A) * (std::sin (w) * 0.70710678f);
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float fc, float gainDb, double sr)
    {
        const float A = std::exp (gainDb * 0.0575646f), w = kTwoPi * fc / (float) sr, c = std::cos (w), sq = 2.0f * std::sqrt (A) * (std::sin (w) * 0.70710678f);
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// circular delay line with linear interpolation
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (float maxSamples)
    {
        int n = 4;
        while ((float) n < maxSamples + 4.0f) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // the sample `d` samples before the one that is about to be pushed (call before push)
    float tap (float d) const
    {
        d = std::clamp (d - 1.0f, 0.0f, (float) mask - 3.0f);
        const float fi = std::floor (d), fr = d - fi;
        const int i0 = (w - 1 - (int) fi) & mask, i1 = (i0 - 1) & mask;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
};

struct Phase
{
    float ph = 0.0f;
    float tick (float hz, float sr) { ph += hz / sr; if (ph >= 1.0f) ph -= 1.0f; return ph; }
};

inline float sine (float ph) { return std::sin (kTwoPi * ph); }

//==============================================================================
// DELAY: clean digital delay
class DelayModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (auto& d : line) d.prepare (2.1f * sr);
        time.prepare (s, 60.0f);
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float tt = p[0] * 0.001f * sr, fb = std::min (pct (p[1]), 0.95f), mix = pct (p[2]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float ts = time.next (tt);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], y = line[c].tap (ts);
                line[c].push (x + y * fb);
                io[c][i] = x + y * mix;
            }
        }
    }
private:
    float sr = 44100.0f;
    DelayLine line[2];
    Smooth time;
};

// ECHO: tape-style echo, repeats get darker, the tape wobbles
class EchoModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c)
        {
            line[c].prepare (2.1f * sr);
            lp[c].lowpass (3200.0f, 0.6f, s); hp[c].highpass (140.0f, 0.7f, s);
            lp[c].reset(); hp[c].reset();
        }
        time.prepare (s, 140.0f);
        p1 = p2 = 0.0f;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float tt = p[0] * 0.001f * sr, fb = pct (p[1]) * 0.92f, level = pct (p[2]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            p1 += 0.55f / sr; if (p1 >= 1.0f) p1 -= 1.0f;
            p2 += 6.2f / sr;  if (p2 >= 1.0f) p2 -= 1.0f;
            const float wobble = (0.25f * sine (p1) + 0.07f * sine (p2)) * 0.001f * sr;
            const float ts = time.next (tt) + wobble;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float y = hp[c].tick (lp[c].tick (line[c].tap (ts)));
                line[c].push (x + softClip (y * 1.4f) * (fb / 1.4f) * 1.4f);
                io[c][i] = x + y * level * 1.2f;
            }
        }
    }
private:
    float sr = 44100.0f, p1 = 0.0f, p2 = 0.0f;
    DelayLine line[2];
    Biquad lp[2], hp[2];
    Smooth time;
};

// CHORUS: two modulated short delays, opposite phase left / right
class ChorusModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& d : line) d.prepare (0.05f * sr); ph.ph = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], depth = pct (p[1]) * 0.0055f * sr, base = 0.0125f * sr, mix = pct (p[2]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float t = ph.tick (rate, sr);
            const float mod[2] = { sine (t), sine (t + 0.25f) };
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], y = line[c].tap (base + mod[c] * depth);
                line[c].push (x);
                io[c][i] = x + (y - x) * mix;
            }
        }
    }
private:
    float sr = 44100.0f;
    DelayLine line[2];
    Phase ph;
};

// VIBRATO: a modulated delay, 100 % wet
class VibratoModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& d : line) d.prepare (0.03f * sr); ph.ph = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], depth = pct (p[1]) * 0.0045f * sr, base = 0.0055f * sr;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float m = sine (ph.tick (rate, sr));
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                line[c].push (x);
                io[c][i] = line[c].tap (base + m * depth + 1.0f);
            }
        }
    }
private:
    float sr = 44100.0f;
    DelayLine line[2];
    Phase ph;
};

// FLANGER: a very short swept delay with feedback
class FlangerModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& d : line) d.prepare (0.01f * sr); ph.ph = 0.0f; fbState[0] = fbState[1] = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], depth = pct (p[1]), fb = pct (p[2]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float t = ph.tick (rate, sr);
            const float lfo[2] = { 0.5f + 0.5f * sine (t), 0.5f + 0.5f * sine (t + 0.1f) };
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float d = (0.0005f + lfo[c] * depth * 0.0035f) * sr;
                const float y = line[c].tap (d);
                line[c].push (x + softClip (y * fb));
                io[c][i] = 0.7071f * (x + y);
            }
        }
    }
private:
    float sr = 44100.0f, fbState[2] {};
    DelayLine line[2];
    Phase ph;
};

// PHASER: four first-order all-pass stages swept by an LFO, with feedback
class PhaserModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; for (auto& c : st) for (auto& z : c) z = 0.0f; fbz[0] = fbz[1] = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], depth = pct (p[1]), fb = pct (p[2]) * 0.95f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float lfo = 0.5f + 0.5f * sine (ph.tick (rate, sr));
            const float fc = 250.0f * std::pow (14.0f, lfo * depth + (1.0f - depth) * 0.3f);
            const float tn = std::tan (kPi * std::min (fc, 0.45f * sr) / sr), a = (tn - 1.0f) / (tn + 1.0f);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                float v = x + fbz[c] * fb;
                for (int k = 0; k < 4; ++k)
                {
                    const float y = a * v + st[c][k];
                    st[c][k] = v - a * y;
                    v = y;
                }
                fbz[c] = softClip (v);
                io[c][i] = 0.5f * (x + v) * 1.4f;
            }
        }
    }
private:
    float sr = 44100.0f, st[2][4] {}, fbz[2] {};
    Phase ph;
};

// TREMOLO: amplitude modulation, sine to square
class TremoloModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; depth.prepare (s, 15.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], dTarget = pct (p[1]), k = 0.1f + pct (p[2]) * 10.0f, norm = 1.0f / std::tanh (k);
        for (int i = 0; i < n; ++i)
        {
            const float lfo = 0.5f + 0.5f * std::tanh (k * sine (ph.tick (rate, sr))) * norm;
            const float g = 1.0f - depth.next (dTarget) * (1.0f - lfo);
            l[i] *= g; r[i] *= g;
        }
    }
private:
    float sr = 44100.0f;
    Phase ph;
    Smooth depth;
};

// SLICER: a rhythmic gate (the sound is cut into pulses)
class SlicerModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; g = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float rate = p[0], duty = pct (p[1]), c = 1.0f - timeCoef ((0.5f + pct (p[2]) * 14.0f) * 0.001f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float target = ph.tick (rate, sr) < duty ? 1.0f : 0.0f;
            g += (target - g) * c;
            l[i] *= g; r[i] *= g;
        }
    }
private:
    float sr = 44100.0f, g = 0.0f;
    Phase ph;
};

//==============================================================================
// shared tone filter of the drive modules: a two-pole low-pass that sweeps 800 Hz .. 12 kHz
struct ToneLp
{
    OnePole a[2], b[2];
    void set (float tone01, double sr)
    {
        const float fc = 800.0f * std::pow (15.0f, tone01);
        for (int c = 0; c < 2; ++c) { a[c].setFc (fc, sr); b[c].setFc (fc, sr); }
    }
    float tick (int c, float x) { return b[c].lp (a[c].lp (x)); }
    void reset() { for (int c = 0; c < 2; ++c) { a[c].reset(); b[c].reset(); } }
};

// OVERDRIVE: soft clipping with a mid hump in front of the clipper
class OverdriveModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = s; tone.reset(); gain.prepare (s, 20.0f); out.prepare (s, 20.0f);
        for (int c = 0; c < 2; ++c) { lpMid[c].setFc (700.0f, s); lpMid[c].reset(); dc[c].highpass (25.0f, 0.7f, s); dc[c].reset(); }
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float gTarget = dbToGain (4.0f + pct (p[0]) * 36.0f), oTarget = dbToGain (p[2]);
        tone.set (pct (p[1]), sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.next (gTarget), o = out.next (oTarget);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float pre = x + 1.4f * (x - lpMid[c].lp (x));              // lift above ~700 Hz
                const float v = g * pre;
                const float y = v >= 0.0f ? softClip (v) : softClip (v * 0.85f) * 1.1f;   // slightly asymmetric
                io[c][i] = tone.tick (c, dc[c].tick (y)) * o * 0.8f;
            }
        }
    }
private:
    double sr = 44100.0;
    ToneLp tone;
    OnePole lpMid[2];
    Biquad dc[2];
    Smooth gain, out;
};

// DISTORTION: hard, tight distortion (high gain, symmetrical clipping)
class DistortionModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = s; tone.reset(); gain.prepare (s, 20.0f); out.prepare (s, 20.0f);
        for (int c = 0; c < 2; ++c) { hp[c].highpass (110.0f, 0.7f, s); hp[c].reset(); }
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float gTarget = dbToGain (10.0f + pct (p[0]) * 42.0f), oTarget = dbToGain (p[2]);
        tone.set (pct (p[1]), sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.next (gTarget), o = out.next (oTarget);
            for (int c = 0; c < 2; ++c)
            {
                const float v = g * hp[c].tick (io[c][i]);
                const float y = std::clamp (softClip (1.6f * v) * 1.25f, -0.9f, 0.9f);
                io[c][i] = tone.tick (c, y) * o * 0.7f;
            }
        }
    }
private:
    double sr = 44100.0;
    ToneLp tone;
    Biquad hp[2];
    Smooth gain, out;
};

// FUZZ: very high gain, diode-style squared-off wave with a little bias for the "dying battery" gating
class FuzzModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = s; tone.reset(); gain.prepare (s, 20.0f); out.prepare (s, 20.0f);
        for (int c = 0; c < 2; ++c) { dc[c].highpass (35.0f, 0.7f, s); dc[c].reset(); }
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float f = pct (p[0]), gTarget = dbToGain (14.0f + f * 40.0f), oTarget = dbToGain (p[2]), bias = 0.05f + 0.25f * f;
        tone.set (pct (p[1]) * 0.9f, sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.next (gTarget), o = out.next (oTarget);
            for (int c = 0; c < 2; ++c)
            {
                const float v = g * io[c][i] + bias;
                const float y = v >= 0.0f ? 1.0f - std::exp (-v) : -(1.0f - std::exp (v * 1.6f)) * 0.9f;
                io[c][i] = tone.tick (c, dc[c].tick (y)) * o * 0.6f;
            }
        }
    }
private:
    double sr = 44100.0;
    ToneLp tone;
    Biquad dc[2];
    Smooth gain, out;
};

// BOOST: clean gain with a treble tilt
class BoostModule final : public Module
{
public:
    void prepare (double s) override { sr = s; gain.prepare (s, 20.0f); for (auto& f : hs) f.reset(); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float gTarget = dbToGain (p[0]);
        for (auto& f : hs) f.highShelf (3000.0f, (p[1] - 50.0f) * 0.16f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.next (gTarget);
            l[i] = hs[0].tick (l[i]) * g;
            r[i] = hs[1].tick (r[i]) * g;
        }
    }
private:
    double sr = 44100.0;
    Biquad hs[2];
    Smooth gain;
};

// VOLUME: level and balance
class VolumeModule final : public Module
{
public:
    void prepare (double s) override { gain.prepare (s, 15.0f); bal.prepare (s, 15.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float gTarget = p[0] <= -59.9f ? 0.0f : dbToGain (p[0]), bTarget = pct (p[1]);
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.next (gTarget), b = bal.next (bTarget);
            l[i] *= g * (b > 0.0f ? 1.0f - b : 1.0f);
            r[i] *= g * (b < 0.0f ? 1.0f + b : 1.0f);
        }
    }
private:
    Smooth gain, bal;
};

//==============================================================================
// COMPRESSOR: stereo-linked feed-forward compressor with a soft knee
class CompressorModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; grDb = 0.0f; makeup.prepare (s, 30.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float thr = p[0], slope = 1.0f - 1.0f / std::max (1.0f, p[1]), mTarget = dbToGain (p[2]);
        const float att = timeCoef (0.008f, sr), rel = timeCoef (0.12f, sr), knee = 6.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = gainToDb (std::max (std::abs (l[i]), std::abs (r[i])));
            const float over = lvl - thr;
            float red = 0.0f;
            if (2.0f * over > knee) red = slope * over;
            else if (2.0f * over > -knee) red = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            const float c = red > grDb ? att : rel;
            grDb = red + (grDb - red) * c;
            const float g = dbToGain (-grDb) * makeup.next (mTarget);
            l[i] *= g; r[i] *= g;
        }
    }
private:
    float sr = 44100.0f, grDb = 0.0f;
    Smooth makeup;
};

// SUSTAINER: heavy compression with automatic make-up gain, so notes ring on
class SustainerModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; grDb = 0.0f; out.prepare (s, 30.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float sus = pct (p[0]), thr = -6.0f - sus * 46.0f, slope = 1.0f - 1.0f / 12.0f;
        const float makeupDb = 0.5f * (-thr) * slope, oTarget = dbToGain (p[2]);
        const float att = timeCoef ((1.0f + p[1] * 0.6f) * 0.001f, sr), rel = timeCoef (0.35f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = gainToDb (std::max (std::abs (l[i]), std::abs (r[i])));
            float red = std::max (0.0f, (lvl - thr) * slope);
            if (lvl < -62.0f) red = std::max (0.0f, red - (-62.0f - lvl) * 0.8f);       // do not lift the noise floor
            const float c = red > grDb ? att : rel;
            grDb = red + (grDb - red) * c;
            const float g = dbToGain (makeupDb * (sus > 0.0f ? 1.0f : 0.0f) - grDb) * out.next (oTarget);
            l[i] = safe (l[i] * g); r[i] = safe (r[i] * g);
        }
    }
private:
    static float safe (float y) { const float a = std::abs (y); return a <= 0.9f ? y : std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), y); }
    float sr = 44100.0f, grDb = 0.0f;
    Smooth out;
};

// LIMITER: instant-attack peak limiter with a ceiling
class LimiterModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; g = 1.0f; drive.prepare (s, 20.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float dTarget = dbToGain (p[0]), ceiling = dbToGain (p[2]), rel = timeCoef (p[1] * 0.001f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float d = drive.next (dTarget);
            const float a = l[i] * d, b = r[i] * d;
            const float peak = std::max (std::abs (a), std::abs (b));
            const float target = peak > ceiling ? ceiling / peak : 1.0f;
            g = target < g ? target : target + (g - target) * rel;
            l[i] = std::clamp (a * g, -ceiling, ceiling);
            r[i] = std::clamp (b * g, -ceiling, ceiling);
        }
    }
private:
    float sr = 44100.0f, g = 1.0f;
    Smooth drive;
};

// GATE: closes below the threshold
class GateModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; env = 0.0f; g = 0.0f; open = false; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float thr = dbToGain (p[0]), closeThr = thr * 0.7f;
        const float envRel = timeCoef (0.03f, sr), openC = timeCoef (0.001f, sr), closeC = timeCoef (p[1] * 0.001f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (l[i]), std::abs (r[i]));
            env = a > env ? a : a + (env - a) * envRel;
            if (! open && env > thr) open = true;
            else if (open && env < closeThr) open = false;
            const float target = open ? 1.0f : 0.0f;
            g = target + (g - target) * (open ? openC : closeC);
            l[i] *= g; r[i] *= g;
        }
    }
private:
    float sr = 44100.0f, env = 0.0f, g = 0.0f;
    bool open = false;
};

// TRANSIENT SHAPER: compares a fast and a slow level follower. Where the fast one is higher (the start of a note) ATTACK
// raises or lowers the level; where it is lower (the tail) SUSTAIN does. It works on the shape, so it does not depend on the level.
class TransientModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; fast = slow = 0.0f; gainDb = 0.0f; out.prepare (s, 20.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float att = p[0] * 0.01f, sus = p[1] * 0.01f, oTarget = dbToGain (p[2]);
        const float fa = timeCoef (0.0005f, sr), fr = timeCoef (0.012f, sr), sa = timeCoef (0.015f, sr), sr2 = timeCoef (0.14f, sr), gs = timeCoef (0.002f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (l[i]), std::abs (r[i]));
            fast = a > fast ? a + (fast - a) * fa : a + (fast - a) * fr;
            slow = a > slow ? a + (slow - a) * sa : a + (slow - a) * sr2;
            const float d = gainToDb (fast + 1.0e-6f) - gainToDb (slow + 1.0e-6f);       // > 0 at an onset, < 0 in the tail
            float g = d > 0.0f ? att * d : sus * -d;
            g = std::clamp (g, -15.0f, 10.0f);
            if (slow < 0.0005f) g = 0.0f;                                                // leave the noise floor alone
            gainDb = g + (gainDb - g) * gs;
            const float gain = dbToGain (gainDb) * out.next (oTarget);
            l[i] *= gain; r[i] *= gain;
        }
    }
private:
    float sr = 44100.0f, fast = 0.0f, slow = 0.0f, gainDb = 0.0f;
    Smooth out;
};

// EXPANDER: downward expander, turns quiet sounds further down (gentler than the gate: the RATIO sets how much)
class ExpanderModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; env = 0.0f; grDb = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float thr = p[0], slope = std::max (1.0f, p[1]) - 1.0f, knee = 6.0f;
        const float envRel = timeCoef (0.04f, sr), openC = timeCoef (0.002f, sr), closeC = timeCoef (p[2] * 0.001f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (l[i]), std::abs (r[i]));
            env = a > env ? a : a + (env - a) * envRel;
            const float under = thr - gainToDb (env);                                   // dB below the threshold
            float red = 0.0f;
            if (under > knee * 0.5f) red = slope * under;
            else if (under > -knee * 0.5f) red = slope * (under + knee * 0.5f) * (under + knee * 0.5f) / (2.0f * knee);
            red = std::min (red, 60.0f);
            const float c = red > grDb ? closeC : openC;
            grDb = red + (grDb - red) * c;
            const float g = dbToGain (-grDb);
            l[i] *= g; r[i] *= g;
        }
    }
private:
    float sr = 44100.0f, env = 0.0f, grDb = 0.0f;
};

//==============================================================================
// EQ: low shelf, mid peak, high shelf
class EqModule final : public Module
{
public:
    void prepare (double s) override { sr = s; for (auto& c : band) for (auto& f : c) f.reset(); }
    void process (float* l, float* r, int n, const float* p) override
    {
        for (auto& c : band)
        {
            c[0].lowShelf (120.0f, p[0], sr);
            c[1].peak (900.0f, 0.8f, p[1], sr);
            c[2].highShelf (5000.0f, p[2], sr);
        }
        for (int i = 0; i < n; ++i)
        {
            l[i] = band[0][2].tick (band[0][1].tick (band[0][0].tick (l[i])));
            r[i] = band[1][2].tick (band[1][1].tick (band[1][0].tick (r[i])));
        }
    }
private:
    double sr = 44100.0;
    Biquad band[2][3];
};

// ENHANCER: adds harmonics above a frequency (an exciter)
class EnhancerModule final : public Module
{
public:
    void prepare (double s) override { sr = s; for (int c = 0; c < 2; ++c) { hp1[c].reset(); hp2[c].reset(); } amount.prepare (s, 20.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        for (int c = 0; c < 2; ++c) { hp1[c].highpass (p[0], 0.707f, sr); hp2[c].highpass (p[0], 0.707f, sr); }
        const float aTarget = pct (p[1]) * 0.7f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float a = amount.next (aTarget);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], v = hp1[c].tick (x) * 3.0f;
                const float h = hp2[c].tick (softClip (v + 0.5f * v * v));
                io[c][i] = x + h * a;
            }
        }
    }
private:
    double sr = 44100.0;
    Biquad hp1[2], hp2[2];
    Smooth amount;
};

// AUTO WAH: a band-pass filter whose frequency follows how hard you play
class WahModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; env = 0.0f; amt.prepare (s, 8.0f); for (auto& v : ic) v[0] = v[1] = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float lo = p[0], hi = std::min(5000.0f, lo * 7.0f), sens = 0.5f + pct (p[1]) * 11.0f, q = 1.5f + pct (p[2]) * 10.0f, k = 1.0f / q;
        const float att = timeCoef (0.004f, sr), rel = timeCoef (0.08f, sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float a = 0.5f * (std::abs (l[i]) + std::abs (r[i]));
            env = a > env ? a + (env - a) * att : a + (env - a) * rel;
            const float x01 = amt.next (std::clamp (env * sens, 0.0f, 1.0f));
            const float fc = lo * std::pow (hi / lo, x01);
            const float g = std::tan (kPi * fc / sr), a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], v3 = x - ic[c][1];
                const float v1 = a1 * ic[c][0] + a2 * v3, v2 = ic[c][1] + a2 * ic[c][0] + a3 * v3;
                ic[c][0] = 2.0f * v1 - ic[c][0];
                ic[c][1] = 2.0f * v2 - ic[c][1];
                io[c][i] = v1 * k * (2.0f + q * 0.35f) + x * 0.15f;
            }
        }
    }
private:
    float sr = 44100.0f, env = 0.0f, ic[2][2] {};
    Smooth amt;
};

//==============================================================================
// OCTAVE: one octave down (flip-flop divider) and one up (full-wave rectifier); best with single notes
class OctaveModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (auto& c : ch)
        {
            c = Chan();
            c.track.lowpass (700.0f, 0.7f, s);
            c.upLp.lowpass (2500.0f, 0.7f, s);
            c.dc.highpass (60.0f, 0.7f, s);
            c.sq1.setFc (550.0f, s); c.sq2.setFc (550.0f, s);
        }
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float dry = pct (p[0]), down = pct (p[1]) * 1.6f, up = pct (p[2]) * 1.3f;
        const float att = timeCoef (0.003f, sr), rel = timeCoef (0.06f, sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                const float x = io[c][i], t = s.track.tick (x), a = std::abs (x);
                s.env = a > s.env ? a + (s.env - a) * att : a + (s.env - a) * rel;
                if (t > 0.003f && ! s.pos) { s.pos = true; s.flip = ! s.flip; }
                else if (t < -0.003f) s.pos = false;
                const float sq = s.sq2.lp (s.sq1.lp (s.flip ? 1.0f : -1.0f));
                const float dn = sq * s.env * 1.4f;
                const float upv = s.dc.tick (std::abs (s.upLp.tick (x))) * 2.0f;
                io[c][i] = x * dry + dn * down + upv * up;
            }
    }
private:
    struct Chan
    {
        Biquad track, upLp, dc;
        OnePole sq1, sq2;
        float env = 0.0f;
        bool pos = false, flip = false;
    };
    float sr = 44100.0f;
    Chan ch[2];
};

//==============================================================================
// REVERB: a small algorithmic room / hall (juce::Reverb), size, damping and mix
class ReverbModule final : public Module
{
public:
    void prepare (double s) override { rev.setSampleRate (s); rev.reset(); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float mix = pct (p[2]);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + pct (p[0]) * 0.69f;
        rp.damping = pct (p[1]);
        rp.wetLevel = 0.4f * mix;
        rp.dryLevel = 1.0f - mix;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        rev.setParameters (rp);
        rev.processStereo (l, r, n);
    }
private:
    juce::Reverb rev;
};

// STEREO SPREADER: mid / side width, a decorrelating delay on the side signal, and a mono bass
class SpreaderModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s; line.prepare (0.03f * sr);
        low1.reset(); low2.reset(); width.prepare (s, 30.0f); spread.prepare (s, 30.0f);
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float wTarget = pct (p[0]), sTarget = pct (p[1]);
        const float bass = p[2];
        if (bass > 20.0f) { low1.setFc (bass, sr); low2.setFc (bass, sr); }
        for (int i = 0; i < n; ++i)
        {
            const float w = width.next (wTarget), sp = spread.next (sTarget);
            const float m = 0.5f * (l[i] + r[i]);
            float s = 0.5f * (l[i] - r[i]);
            if (bass > 20.0f) s -= low2.lp (low1.lp (s));                    // the bass stays in the middle
            s *= w;
            const float sd = line.tap (0.0006f * sr + sp * 0.018f * sr);
            line.push (s);
            s = s * (1.0f - 0.55f * sp) + sd * (0.55f * sp) * 1.2f;
            l[i] = m + s; r[i] = m - s;
        }
    }
private:
    float sr = 44100.0f;
    DelayLine line;
    OnePole low1, low2;
    Smooth width, spread;
};


//==============================================================================
// FILTER: resonant low-pass / band-pass / high-pass (state-variable filter)
class FilterModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& v : ic) v[0] = v[1] = 0.0f; freq.prepare (s, 12.0f); res.prepare (s, 12.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const int type = juce::jlimit (0, 2, (int) std::lround (p[2]));
        const float fTarget = std::clamp (p[0], 20.0f, 0.45f * sr), rTarget = pct (p[1]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float fc = freq.next (fTarget), q = 0.7f + res.next (rTarget) * res.v * 14.0f, k = 1.0f / q;
            const float g = std::tan (kPi * fc / sr), a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], v3 = x - ic[c][1];
                const float v1 = a1 * ic[c][0] + a2 * v3, v2 = ic[c][1] + a2 * ic[c][0] + a3 * v3;
                ic[c][0] = 2.0f * v1 - ic[c][0];
                ic[c][1] = 2.0f * v2 - ic[c][1];
                const float y = type == 0 ? v2 : type == 1 ? v1 * k : x - k * v1 - v2;
                io[c][i] = std::clamp (y, -8.0f, 8.0f);
            }
        }
    }
private:
    float sr = 44100.0f, ic[2][2] {};
    Smooth freq, res;
};

// PAN: position plus an auto-pan LFO (equal-loudness balance: the louder side stays at full level)
class PanModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; pos.prepare (s, 15.0f); depth.prepare (s, 15.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float posT = pct (p[0]), rate = p[1], depthT = pct (p[2]);
        for (int i = 0; i < n; ++i)
        {
            const float m = std::clamp (pos.next (posT) + depth.next (depthT) * sine (ph.tick (rate, sr)), -1.0f, 1.0f);
            const float theta = (m + 1.0f) * kPi * 0.25f;
            l[i] *= std::min (1.0f, 1.4142f * std::cos (theta));
            r[i] *= std::min (1.0f, 1.4142f * std::sin (theta));
        }
    }
private:
    float sr = 44100.0f;
    Phase ph;
    Smooth pos, depth;
};

//==============================================================================
// MOD DELAY: a delay whose time is modulated, so the repeats chorus and wobble (the speed follows the MOD amount)
class ModDelayModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c) { line[c].prepare (2.1f * sr); lp[c].lowpass (4500.0f, 0.6f, s); lp[c].reset(); }
        time.prepare (s, 80.0f); ph.ph = 0.0f;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float tt = p[0] * 0.001f * sr, fb = std::min (pct (p[1]), 0.92f), mod = pct (p[2]);
        const float rate = 0.4f + 1.6f * mod, depth = mod * 0.0035f * sr;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float t = ph.tick (rate, sr), ts = time.next (tt);
            const float lfo[2] = { sine (t), sine (t + 0.25f) };
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i], y = lp[c].tick (line[c].tap (ts + lfo[c] * depth));
                line[c].push (x + softClip (y * fb));
                io[c][i] = x + y * 0.6f;
            }
        }
    }
private:
    float sr = 44100.0f;
    DelayLine line[2];
    Biquad lp[2];
    Smooth time;
    Phase ph;
};

// PING-PONG: the echoes bounce between left and right
class PingPongModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; left.prepare (2.1f * sr); right.prepare (2.1f * sr); time.prepare (s, 60.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float tt = p[0] * 0.001f * sr, fb = std::min (pct (p[1]), 0.92f), mix = pct (p[2]);
        for (int i = 0; i < n; ++i)
        {
            const float ts = time.next (tt), xl = l[i], xr = r[i], mono = 0.5f * (xl + xr);
            const float yl = left.tap (ts), yr = right.tap (ts);
            left.push (mono + softClip (yr * fb));                  // the first echo goes to the left ...
            right.push (yl);                                        // ... then to the right, then back to the left
            l[i] = xl + yl * mix;
            r[i] = xr + yr * mix;
        }
    }
private:
    float sr = 44100.0f;
    DelayLine left, right;
    Smooth time;
};

// REVERSE DELAY: the last TIME of sound is played backwards (two overlapping Hann-windowed read heads)
class ReverseModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (auto& d : line) d.prepare (4.3f * sr);
        head[0].n = 0.0f; head[1].n = -1.0f; head[0].len = head[1].len = 0.4f * sr;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float len = std::clamp (p[0] * 0.001f * sr, 0.02f * sr, 2.05f * sr), fb = std::min (pct (p[1]), 0.9f), mix = pct (p[2]);
        if (head[1].n < 0.0f) { head[0].len = head[1].len = len; head[1].n = len * 0.5f; }
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            float wet[2] = { 0.0f, 0.0f };
            for (auto& h : head)
            {
                const float w = std::sin (kPi * h.n / h.len), win = w * w;
                for (int c = 0; c < 2; ++c) wet[c] += win * line[c].tap (2.0f * h.n + 1.0f);
                h.n += 1.0f;
                if (h.n >= h.len) { h.n -= h.len; h.len = len; }
            }
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                line[c].push (x + softClip (wet[c] * fb));
                io[c][i] = x + wet[c] * mix;
            }
        }
    }
private:
    struct Head { float n = 0.0f, len = 1.0f; };
    float sr = 44100.0f;
    DelayLine line[2];
    Head head[2];
};

// SPRING: a delay loop with a long chain of all-pass filters (the dispersion that makes the spring "drip")
class SpringModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c)
        {
            tank[c] = Tank();
            tank[c].line.prepare (0.06f * sr);
            tank[c].delay = (c == 0 ? 0.0371f : 0.0413f) * sr;
            tank[c].hp.highpass (110.0f, 0.7f, s);
            for (auto& a : tank[c].ap) a = 0.0f;
        }
        ph.ph = 0.0f;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float fb = 0.35f + 0.6f * pct (p[0]), mix = pct (p[2]);
        const float fc = 1500.0f * std::pow (5.0f, pct (p[1]));
        for (auto& t : tank) t.damp.setFc (fc, sr);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float mod = sine (ph.tick (0.4f, sr)) * 0.0003f * sr, mono = 0.5f * (l[i] + r[i]);
            for (int c = 0; c < 2; ++c)
            {
                auto& t = tank[c];
                float y = t.line.tap (t.delay + mod * (c == 0 ? 1.0f : -1.0f));
                for (auto& z : t.ap) { const float o = kA * y + z; z = y - kA * o; y = o; }      // dispersion
                y = softClip (t.damp.lp (y));
                t.line.push (t.hp.tick (mono) + y * fb);
                io[c][i] += y * mix * 1.2f;
            }
        }
    }
private:
    static constexpr float kA = -0.7f;
    struct Tank { DelayLine line; Biquad hp; OnePole damp; float ap[30] {}; float delay = 0.0f; };
    float sr = 44100.0f;
    Tank tank[2];
    Phase ph;
};

// SHIMMER: a reverb whose tail is fed back an octave higher
class ShimmerModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        rev.setSampleRate (s); rev.reset();
        for (auto& sh : shifter) { sh.line.prepare (0.06f * sr + 8.0f); sh.ph = 0.0f; }
        for (auto& f : fb) for (auto& v : f) v = 0.0f;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f + 0.44f * pct (p[0]);
        rp.damping = 0.35f;
        rp.wetLevel = 0.33f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        rev.setParameters (rp);
        const float amt = pct (p[1]) * 0.5f, mix = pct (p[2]);
        const float grain = 0.05f * sr;
        float tl[kChunk], tr[kChunk];
        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int m = std::min (kChunk, n - pos);
            for (int i = 0; i < m; ++i) { tl[i] = l[pos + i] + fb[0][i]; tr[i] = r[pos + i] + fb[1][i]; }
            rev.processStereo (tl, tr, m);                                  // wet only
            for (int i = 0; i < m; ++i)
            {
                float* wet[2] = { &tl[i], &tr[i] };
                for (int c = 0; c < 2; ++c)
                {
                    const float up = shifter[c].tick (*wet[c], grain);       // one octave up
                    fb[c][i] = softClip (up * amt);
                    const float x = c == 0 ? l[pos + i] : r[pos + i];
                    const float w = *wet[c] + up * amt * 0.6f;
                    (c == 0 ? l : r)[pos + i] = x * (1.0f - mix) + w * mix;
                }
            }
        }
    }
private:
    static constexpr int kChunk = 32;
    struct Shifter
    {
        DelayLine line;
        float ph = 0.0f;
        float tick (float x, float grain)
        {
            const float p2 = ph + 0.5f < 1.0f ? ph + 0.5f : ph - 0.5f;
            const float w1 = std::sin (kPi * ph), w2 = std::sin (kPi * p2);
            const float y = w1 * w1 * line.tap (grain * (1.0f - ph) + 1.0f) + w2 * w2 * line.tap (grain * (1.0f - p2) + 1.0f);
            line.push (x);
            ph += 1.0f / grain;
            if (ph >= 1.0f) ph -= 1.0f;
            return y;
        }
    };
    float sr = 44100.0f;
    juce::Reverb rev;
    Shifter shifter[2];
    float fb[2][kChunk] {};
};

//==============================================================================
// PITCH SHIFTER: shift the notes up or down (two Hann-windowed read heads on a delay line); mix with the dry sound for a harmony
class PitchModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& c : ch) { c.line.prepare (0.07f * sr + 8.0f); c.ph = 0.0f; } mix.prepare (s, 20.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float ratio = std::pow (2.0f, (p[0] + p[1] * 0.01f) / 12.0f), grain = 0.06f * sr, mTarget = pct (p[2]);
        const float step = std::abs (1.0f - ratio) / grain;           // how fast the read heads slide through the grain
        const bool up = ratio >= 1.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.next (mTarget);
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                const float p2 = s.ph + 0.5f < 1.0f ? s.ph + 0.5f : s.ph - 0.5f;
                const float w1 = std::sin (kPi * s.ph), w2 = std::sin (kPi * p2);
                const float d1 = grain * (up ? 1.0f - s.ph : s.ph), d2 = grain * (up ? 1.0f - p2 : p2);
                const float y = w1 * w1 * s.line.tap (d1 + 1.0f) + w2 * w2 * s.line.tap (d2 + 1.0f);
                const float x = io[c][i];
                s.line.push (x);
                s.ph += step; if (s.ph >= 1.0f) s.ph -= 1.0f;
                io[c][i] = x * (1.0f - m * 0.5f) + y * m;
            }
        }
    }
private:
    struct Chan { DelayLine line; float ph = 0.0f; };
    float sr = 44100.0f;
    Chan ch[2];
    Smooth mix;
};

// RING MODULATOR: the input is multiplied by an oscillator (metallic, bell-like sounds)
class RingModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; mix.prepare (s, 20.0f); freq.prepare (s, 25.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const int wave = juce::jlimit (0, 2, (int) std::lround (p[2]));
        const float fTarget = p[0], mTarget = pct (p[1]);
        for (int i = 0; i < n; ++i)
        {
            const float t = ph.tick (freq.next (fTarget), sr), m = mix.next (mTarget);
            const float car = wave == 0 ? sine (t) : wave == 1 ? 4.0f * std::abs (t - 0.5f) - 1.0f : (t < 0.5f ? 1.0f : -1.0f);
            l[i] += (l[i] * car - l[i]) * m;
            r[i] += (r[i] * car - r[i]) * m;
        }
    }
private:
    float sr = 44100.0f;
    Phase ph;
    Smooth mix, freq;
};

// BIT CRUSHER: fewer bits and a lower sample rate (lo-fi, digital grit)
class CrusherModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; for (auto& h : hold) h = 0.0f; counter = 1.0f; mix.prepare (s, 20.0f); }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float levels = std::exp2 (std::round (p[0]) - 1.0f), inc = std::min (1.0f, p[1] / sr), mTarget = pct (p[2]);
        for (int i = 0; i < n; ++i)
        {
            counter += inc;
            if (counter >= 1.0f)
            {
                counter -= 1.0f;
                hold[0] = std::round (l[i] * levels) / levels;
                hold[1] = std::round (r[i] * levels) / levels;
            }
            const float m = mix.next (mTarget);
            l[i] += (hold[0] - l[i]) * m;
            r[i] += (hold[1] - r[i]) * m;
        }
    }
private:
    float sr = 44100.0f, counter = 1.0f, hold[2] {};
    Smooth mix;
};

//==============================================================================
// TAPE: gentle saturation that keeps the level of quiet sounds; WARMTH adds a low "head bump" and rolls off the highs
class TapeModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c) { bump[c].reset(); lp1[c].reset(); lp2[c].reset(); line[c].prepare (0.02f * sr); }
        drive.prepare (s, 20.0f); out.prepare (s, 20.0f); ph.ph = 0.0f;
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float warm = pct (p[1]), dTarget = 1.0f + pct (p[0]) * 5.0f, oTarget = dbToGain (p[2]);
        const float fc = 16000.0f * std::pow (0.3125f, warm);                   // 16 kHz .. 5 kHz
        for (int c = 0; c < 2; ++c) { bump[c].lowShelf (90.0f, 4.0f * warm, sr); lp1[c].setFc (fc, sr); lp2[c].setFc (fc, sr); }
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.next (dTarget), o = out.next (oTarget);
            const float flutter = (1.5f + sine (ph.tick (0.7f, sr)) * 0.8f) * 0.0001f * sr * (0.3f + warm);   // a little wow
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                line[c].push (x);
                const float d = line[c].tap (flutter + 2.0f);
                const float y = std::tanh (g * bump[c].tick (d)) / g * (1.0f + 0.12f * (g - 1.0f));
                io[c][i] = lp2[c].lp (lp1[c].lp (y)) * o;
            }
        }
    }
private:
    float sr = 44100.0f;
    Biquad bump[2];
    OnePole lp1[2], lp2[2];
    DelayLine line[2];
    Smooth drive, out;
    Phase ph;
};

// TUBE: smooth, asymmetric saturation (even harmonics); BIAS sets how lopsided it is
class TubeModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (int c = 0; c < 2; ++c) { dc[c].highpass (20.0f, 0.7f, s); dc[c].reset(); lp[c].setFc (12000.0f, s); lp[c].reset(); }
        drive.prepare (s, 20.0f); out.prepare (s, 20.0f); bias.prepare (s, 20.0f);
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float dTarget = 1.0f + pct (p[0]) * 7.0f, bTarget = pct (p[1]) * 0.6f, oTarget = dbToGain (p[2]);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.next (dTarget), b = bias.next (bTarget), o = out.next (oTarget), off = std::tanh (b);
            for (int c = 0; c < 2; ++c)
            {
                const float y = (std::tanh (g * io[c][i] + b) - off) / g * (1.0f + 0.1f * (g - 1.0f));
                io[c][i] = lp[c].lp (dc[c].tick (y)) * o;
            }
        }
    }
private:
    float sr = 44100.0f;
    Biquad dc[2];
    OnePole lp[2];
    Smooth drive, bias, out;
};

// ROTARY: a rotary speaker. A 800 Hz split sends the highs to a fast horn and the lows to a slower drum; each one
// pulses in level (two "microphones" 90 degrees apart) and has a Doppler pitch wobble. The speed ramps up and down slowly.
class RotaryModule final : public Module
{
public:
    void prepare (double s) override
    {
        sr = (float) s;
        for (auto& l : horn) l.prepare (0.012f * sr);
        for (auto& l : drum) l.prepare (0.012f * sr);
        lp1.reset(); lp2.reset(); lp1.setFc (800.0f, s); lp2.setFc (800.0f, s);
        thH = thD = 0.0f; fH = fD = 0.7f; mix.prepare (s, 20.0f);
    }
    void process (float* l, float* r, int n, const float* p) override
    {
        const float speed = pct (p[0]), depth = pct (p[1]), mTarget = pct (p[2]);
        const float hTarget = 0.7f + speed * 6.0f, dTarget = 0.6f + speed * 4.4f;
        const float cH = 1.0f - timeCoef (0.6f, sr), cD = 1.0f - timeCoef (2.2f, sr);
        const float dopH = depth * 0.00045f * sr, dopD = depth * 0.0002f * sr, ampH = depth * 0.65f, ampD = depth * 0.4f, base = 0.0007f * sr;
        for (int i = 0; i < n; ++i)
        {
            fH += (hTarget - fH) * cH; fD += (dTarget - fD) * cD;
            thH += kTwoPi * fH / sr; if (thH > kTwoPi) thH -= kTwoPi;
            thD += kTwoPi * fD / sr; if (thD > kTwoPi) thD -= kTwoPi;
            const float m = 0.5f * (l[i] + r[i]), lo = lp2.lp (lp1.lp (m)), hi = m - lo, mx = mix.next (mTarget);
            const float hornTap = horn[0].tap (base + dopH * (1.0f + std::sin (thH)));
            horn[0].push (hi);
            const float drumTap = drum[0].tap (base + dopD * (1.0f + std::sin (thD)));
            drum[0].push (lo);
            const float gHL = 1.0f - ampH * (0.5f - 0.5f * std::cos (thH)), gHR = 1.0f - ampH * (0.5f - 0.5f * std::cos (thH + 1.6f));
            const float gDL = 1.0f - ampD * (0.5f - 0.5f * std::cos (thD)), gDR = 1.0f - ampD * (0.5f - 0.5f * std::cos (thD + 1.6f));
            const float wl = hornTap * gHL + drumTap * gDL, wr = hornTap * gHR + drumTap * gDR;
            l[i] += (wl - l[i]) * mx;
            r[i] += (wr - r[i]) * mx;
        }
    }
private:
    float sr = 44100.0f, thH = 0.0f, thD = 0.0f, fH = 0.7f, fD = 0.7f;
    DelayLine horn[1], drum[1];
    OnePole lp1, lp2;
    Smooth mix;
};

// UNI-VIBE: four all-pass stages swept (unevenly) by a lamp-style LFO; CHORUS adds the dry sound, VIBRATO is the phase shift alone
class UniVibeModule final : public Module
{
public:
    void prepare (double s) override { sr = (float) s; ph.ph = 0.0f; for (auto& c : st) for (auto& z : c) z = 0.0f; }
    void process (float* l, float* r, int n, const float* p) override
    {
        static const float lo[4] = { 130.0f, 250.0f, 480.0f, 900.0f }, hi[4] = { 1100.0f, 2000.0f, 3300.0f, 5200.0f };
        const float rate = p[0], depth = 0.15f + 0.85f * pct (p[1]);
        const bool vibrato = (int) std::lround (p[2]) == 1;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float t = ph.tick (rate, sr);
            for (int c = 0; c < 2; ++c)
            {
                const float s0 = 0.5f + 0.5f * sine (t + (c == 0 ? 0.0f : 0.08f));
                const float lamp = std::pow (s0, 1.6f) * depth;                   // the lamp / photocell is not linear
                const float x = io[c][i];
                float v = x;
                for (int k = 0; k < 4; ++k)
                {
                    const float fc = lo[k] * std::pow (hi[k] / lo[k], lamp), tn = std::tan (kPi * fc / sr), a = (tn - 1.0f) / (tn + 1.0f);
                    const float y = a * v + st[c][k];
                    st[c][k] = v - a * y;
                    v = y;
                }
                io[c][i] = vibrato ? v : 0.5f * (x + v) * 1.35f;
            }
        }
    }
private:
    float sr = 44100.0f, st[2][4] {};
    Phase ph;
};

//==============================================================================
template <typename T>
std::unique_ptr<Module> make() { return std::make_unique<T>(); }

enum Group { kDelay, kModulation, kDrive, kDynamics, kFilter, kPitch, kSpace };

const ModuleInfo kModules[] =
{
    { "DELAY", "DELAY", "CLEAN DIGITAL DELAY", kDelay, 3,
      { { "TIME", 20, 1000, 380, 250, "ms", 0 }, { "FEEDBACK", 0, 95, 35, 0, "%", 0 }, { "MIX", 0, 100, 30, 0, "%", 0 } }, make<DelayModule>, 8.0f, 0 },
    { "ECHO", "ECHO", "WARM TAPE-STYLE ECHO", kDelay, 3,
      { { "TIME", 40, 1200, 320, 300, "ms", 0 }, { "REPEATS", 0, 100, 40, 0, "%", 0 }, { "LEVEL", 0, 100, 50, 0, "%", 0 } }, make<EchoModule>, 8.0f, 0 },

    { "MODDELAY", "MOD DELAY", "WOBBLING CHORUSED ECHOES", kDelay, 3,
      { { "TIME", 20, 1000, 350, 250, "ms", 0 }, { "FEEDBACK", 0, 92, 40, 0, "%", 0 }, { "MOD", 0, 100, 40, 0, "%", 0 } }, make<ModDelayModule>, 8.0f, 0 },
    { "PINGPONG", "PING-PONG", "ECHOES BOUNCING LEFT AND RIGHT", kDelay, 3,
      { { "TIME", 20, 1000, 400, 250, "ms", 0 }, { "FEEDBACK", 0, 92, 40, 0, "%", 0 }, { "MIX", 0, 100, 35, 0, "%", 0 } }, make<PingPongModule>, 8.0f, 0 },
    { "REVERSE", "REVERSE", "BACKWARDS ECHOES", kDelay, 3,
      { { "TIME", 50, 1000, 400, 400, "ms", 0 }, { "FEEDBACK", 0, 90, 20, 0, "%", 0 }, { "MIX", 0, 100, 50, 0, "%", 0 } }, make<ReverseModule>, 4.0f, 0 },
    { "CHORUS", "CHORUS", "LUSH DOUBLING", kModulation, 3,
      { { "RATE", 0.1f, 8, 0.8f, 1.5f, "Hz", 1 }, { "DEPTH", 0, 100, 50, 0, "%", 0 }, { "MIX", 0, 100, 50, 0, "%", 0 } }, make<ChorusModule>, 0.1f, 0 },
    { "VIBRATO", "VIBRATO", "WOBBLING PITCH", kModulation, 2,
      { { "RATE", 0.5f, 10, 5, 4, "Hz", 1 }, { "DEPTH", 0, 100, 40, 0, "%", 0 }, { nullptr, 0, 0, 0, 0, nullptr, 0 } }, make<VibratoModule>, 0.0f, 0 },
    { "FLANGER", "FLANGER", "JET-PLANE SWEEP", kModulation, 3,
      { { "RATE", 0.05f, 5, 0.35f, 0.5f, "Hz", 2 }, { "DEPTH", 0, 100, 70, 0, "%", 0 }, { "REGEN", -90, 90, 45, 0, "%", 0 } }, make<FlangerModule>, 0.1f, 0 },
    { "PHASER", "PHASER", "SWIRLING NOTCHES", kModulation, 3,
      { { "RATE", 0.05f, 8, 0.5f, 0.8f, "Hz", 2 }, { "DEPTH", 0, 100, 70, 0, "%", 0 }, { "FEEDBACK", 0, 90, 40, 0, "%", 0 } }, make<PhaserModule>, 0.1f, 0 },
    { "ROTARY", "ROTARY", "SWIRLING ROTARY SPEAKER", kModulation, 3,
      { { "SPEED", 0, 100, 30, 0, "%", 0 }, { "DEPTH", 0, 100, 70, 0, "%", 0 }, { "MIX", 0, 100, 100, 0, "%", 0 } }, make<RotaryModule>, 0.1f },
    { "UNIVIBE", "UNI-VIBE", "THROBBING PHASE VIBRATO", kModulation, 3,
      { { "RATE", 0.1f, 10, 1.5f, 2, "Hz", 1 }, { "INTENSITY", 0, 100, 70, 0, "%", 0 }, { "MODE", 0, 1, 0, 0, "", 0, "CHORUS|VIBRATO" } }, make<UniVibeModule>, 0.1f, 0 },
    { "RINGMOD", "RING MOD", "METALLIC BELL-LIKE TONES", kModulation, 3,
      { { "FREQ", 20, 5000, 400, 400, "Hz", 0 }, { "MIX", 0, 100, 50, 0, "%", 0 }, { "WAVE", 0, 2, 0, 0, "", 0, "SINE|TRIANGLE|SQUARE" } }, make<RingModule>, 0.0f },
    { "TREMOLO", "TREMOLO", "PULSING VOLUME", kModulation, 3,
      { { "RATE", 0.5f, 15, 5, 5, "Hz", 1 }, { "DEPTH", 0, 100, 60, 0, "%", 0 }, { "SHAPE", 0, 100, 0, 0, "%", 0 } }, make<TremoloModule>, 0.0f, 0 },
    { "SLICER", "SLICER", "RHYTHMIC GATE", kModulation, 3,
      { { "RATE", 0.5f, 16, 4, 4, "Hz", 1 }, { "DUTY", 10, 90, 50, 0, "%", 0 }, { "SMOOTH", 0, 100, 25, 0, "%", 0 } }, make<SlicerModule>, 0.0f, 0 },

    { "OVERDRIVE", "OVERDRIVE", "WARM TUBE-STYLE DRIVE", kDrive, 3,
      { { "DRIVE", 0, 100, 40, 0, "%", 0 }, { "TONE", 0, 100, 50, 0, "%", 0 }, { "LEVEL", -30, 6, -6, 0, "dB", 1 } }, make<OverdriveModule>, 0.0f },
    { "DISTORTION", "DISTORTION", "TIGHT HARD CLIPPING", kDrive, 3,
      { { "DRIVE", 0, 100, 55, 0, "%", 0 }, { "TONE", 0, 100, 50, 0, "%", 0 }, { "LEVEL", -30, 6, -9, 0, "dB", 1 } }, make<DistortionModule>, 0.0f },
    { "FUZZ", "FUZZ", "THICK SQUARED-OFF FUZZ", kDrive, 3,
      { { "FUZZ", 0, 100, 60, 0, "%", 0 }, { "TONE", 0, 100, 50, 0, "%", 0 }, { "LEVEL", -30, 6, -9, 0, "dB", 1 } }, make<FuzzModule>, 0.0f },
    { "TAPE", "TAPE", "SOFT TAPE WARMTH AND COMPRESSION", kDrive, 3,
      { { "DRIVE", 0, 100, 40, 0, "%", 0 }, { "WARMTH", 0, 100, 50, 0, "%", 0 }, { "LEVEL", -12, 6, 0, 0, "dB", 1 } }, make<TapeModule>, 0.0f },
    { "TUBE", "TUBE", "SMOOTH EVEN-HARMONIC GLOW", kDrive, 3,
      { { "DRIVE", 0, 100, 40, 0, "%", 0 }, { "BIAS", 0, 100, 40, 0, "%", 0 }, { "LEVEL", -12, 6, 0, 0, "dB", 1 } }, make<TubeModule>, 0.0f },
    { "CRUSHER", "BIT CRUSHER", "LO-FI DIGITAL GRIT", kDrive, 3,
      { { "BITS", 1, 16, 8, 0, "bit", 0 }, { "RATE", 500, 48000, 12000, 8000, "Hz", 0 }, { "MIX", 0, 100, 100, 0, "%", 0 } }, make<CrusherModule>, 0.0f },
    { "BOOST", "BOOST", "CLEAN GAIN BOOST", kDrive, 2,
      { { "GAIN", 0, 24, 10, 0, "dB", 1 }, { "TONE", 0, 100, 50, 0, "%", 0 }, { nullptr, 0, 0, 0, 0, nullptr, 0 } }, make<BoostModule>, 0.0f },

    { "COMPRESSOR", "COMPRESSOR", "EVENS OUT THE LEVEL", kDynamics, 3,
      { { "THRESH", -50, 0, -20, 0, "dB", 0 }, { "RATIO", 1, 20, 4, 4, ":1", 1 }, { "GAIN", 0, 24, 6, 0, "dB", 1 } }, make<CompressorModule>, 0.0f },
    { "SUSTAINER", "SUSTAINER", "NOTES THAT RING ON", kDynamics, 3,
      { { "SUSTAIN", 0, 100, 60, 0, "%", 0 }, { "ATTACK", 1, 50, 10, 10, "ms", 0 }, { "LEVEL", -12, 12, 0, 0, "dB", 1 } }, make<SustainerModule>, 0.0f },
    { "LIMITER", "LIMITER", "KEEPS PEAKS UNDER A CEILING", kDynamics, 3,
      { { "DRIVE", 0, 24, 0, 0, "dB", 1 }, { "RELEASE", 10, 500, 80, 80, "ms", 0 }, { "CEILING", -12, 0, -1, 0, "dB", 1 } }, make<LimiterModule>, 0.0f },
    { "GATE", "NOISE GATE", "SILENCES THE NOISE", kDynamics, 2,
      { { "THRESH", -80, -10, -50, 0, "dB", 0 }, { "RELEASE", 10, 800, 120, 120, "ms", 0 }, { nullptr, 0, 0, 0, 0, nullptr, 0 } }, make<GateModule>, 0.0f },
    { "TRANSIENT", "TRANSIENT SHAPER", "PUNCH UP OR SOFTEN THE ATTACK", kDynamics, 3,
      { { "ATTACK", -100, 100, 0, 0, "%", 0 }, { "SUSTAIN", -100, 100, 0, 0, "%", 0 }, { "LEVEL", -12, 12, 0, 0, "dB", 1 } }, make<TransientModule>, 0.0f },
    { "EXPANDER", "EXPANDER", "TURNS QUIET SOUNDS FURTHER DOWN", kDynamics, 3,
      { { "THRESH", -80, -10, -45, 0, "dB", 0 }, { "RATIO", 1, 10, 2, 2, ":1", 1 }, { "RELEASE", 10, 800, 150, 150, "ms", 0 } }, make<ExpanderModule>, 0.0f },
    { "VOLUME", "VOLUME", "LEVEL AND BALANCE", kDynamics, 2,
      { { "LEVEL", -60, 12, 0, 0, "dB", 1 }, { "BALANCE", -100, 100, 0, 0, "", 0 }, { nullptr, 0, 0, 0, 0, nullptr, 0 } }, make<VolumeModule>, 0.0f },

    { "FILTER", "FILTER", "RESONANT LOW, BAND AND HIGH PASS", kFilter, 3,
      { { "FREQ", 20, 20000, 2000, 1000, "Hz", 0 }, { "RES", 0, 100, 20, 0, "%", 0 }, { "TYPE", 0, 2, 0, 0, "", 0, "LOW PASS|BAND PASS|HIGH PASS" } }, make<FilterModule>, 0.0f },
    { "EQ", "EQ", "THREE-BAND TONE CONTROL", kFilter, 3,
      { { "LOW", -12, 12, 0, 0, "dB", 1 }, { "MID", -12, 12, 0, 0, "dB", 1 }, { "HIGH", -12, 12, 0, 0, "dB", 1 } }, make<EqModule>, 0.0f },
    { "ENHANCER", "ENHANCER", "ADDS AIR AND SPARKLE", kFilter, 2,
      { { "FREQ", 1000, 8000, 3000, 3000, "Hz", 0 }, { "AMOUNT", 0, 100, 40, 0, "%", 0 }, { nullptr, 0, 0, 0, 0, nullptr, 0 } }, make<EnhancerModule>, 0.0f },
    { "WAH", "AUTO WAH", "FILTER THAT FOLLOWS YOUR PLAYING", kFilter, 3,
      { { "FREQ", 200, 1000, 350, 400, "Hz", 0 }, { "SENS", 0, 100, 60, 0, "%", 0 }, { "PEAK", 0, 100, 60, 0, "%", 0 } }, make<WahModule>, 0.0f },

    { "PITCH", "PITCH SHIFTER", "SHIFT THE NOTES OR ADD A HARMONY", kPitch, 3,
      { { "SHIFT", -12, 12, 7, 0, "st", 0 }, { "FINE", -50, 50, 0, 0, "ct", 0 }, { "MIX", 0, 100, 50, 0, "%", 0 } }, make<PitchModule>, 0.1f },
    { "OCTAVE", "OCTAVE", "ONE OCTAVE DOWN AND UP", kPitch, 3,
      { { "DRY", 0, 100, 70, 0, "%", 0 }, { "DOWN", 0, 100, 60, 0, "%", 0 }, { "UP", 0, 100, 0, 0, "%", 0 } }, make<OctaveModule>, 0.0f },

    { "REVERB", "REVERB", "ROOMS AND HALLS", kSpace, 3,
      { { "SIZE", 0, 100, 50, 0, "%", 0 }, { "DAMP", 0, 100, 50, 0, "%", 0 }, { "MIX", 0, 100, 25, 0, "%", 0 } }, make<ReverbModule>, 5.0f },
    { "SPRING", "SPRING", "DRIPPY SPRING TANK", kSpace, 3,
      { { "DECAY", 0, 100, 55, 0, "%", 0 }, { "TONE", 0, 100, 50, 0, "%", 0 }, { "MIX", 0, 100, 30, 0, "%", 0 } }, make<SpringModule>, 4.0f },
    { "SHIMMER", "SHIMMER", "OCTAVE-UP REVERB", kSpace, 3,
      { { "SIZE", 0, 100, 60, 0, "%", 0 }, { "SHIMMER", 0, 100, 50, 0, "%", 0 }, { "MIX", 0, 100, 30, 0, "%", 0 } }, make<ShimmerModule>, 8.0f },
    { "PAN", "PAN", "POSITION AND AUTO-PAN", kSpace, 3,
      { { "POSITION", -100, 100, 0, 0, "", 0 }, { "RATE", 0.1f, 10, 1, 1.5f, "Hz", 1 }, { "DEPTH", 0, 100, 0, 0, "%", 0 } }, make<PanModule>, 0.0f, 1 },
    { "SPREADER", "STEREO SPREADER", "WIDENS THE STEREO IMAGE", kSpace, 3,
      { { "WIDTH", 0, 200, 130, 100, "%", 0 }, { "SPREAD", 0, 100, 30, 0, "%", 0 }, { "BASS", 0, 300, 120, 0, "Hz", 0 } }, make<SpreaderModule>, 0.1f },
};

const char* const kGroupNames[] = { "DELAY & ECHO", "MODULATION", "DRIVE", "DYNAMICS", "FILTER & EQ", "PITCH", "SPACE" };
const juce::uint32 kGroupColours[] = { 0xff5276E5, 0xff36B779, 0xffF07835, 0xffE9B93F, 0xff28A9D5, 0xffE84B91, 0xff8959D5 };
} // namespace

juce::uint32 groupColour (int g) { return kGroupColours[juce::jlimit (0, (int) std::size (kGroupColours) - 1, g)]; }

int numModules() { return (int) std::size (kModules); }
const ModuleInfo& moduleInfo (int i) { return kModules[juce::jlimit (0, numModules() - 1, i)]; }
int numGroups() { return (int) std::size (kGroupNames); }
const char* groupName (int g) { return kGroupNames[juce::jlimit (0, numGroups() - 1, g)]; }

int indexOfModule (const juce::String& id)
{
    for (int i = 0; i < numModules(); ++i)
        if (id == kModules[i].id) return i;
    return -1;
}

juce::String formatValue (const ParamSpec& s, float v)
{
    if (s.labels != nullptr)
    {
        const auto names = juce::StringArray::fromTokens (s.labels, "|", "");
        return names[juce::jlimit (0, names.size() - 1, (int) std::lround (v))];
    }
    juce::String t (v, s.decimals);
    if (s.min < 0.0f && v > 0.0f) t = "+" + t;
    if (s.unit != nullptr && s.unit[0] != 0) t << (s.unit[0] == ':' || s.unit[0] == '%' ? "" : " ") << s.unit;
    return t;
}

namespace
{
const char* const kDivNames[kNumDivisions] = { "1/1", "1/2.", "1/2", "1/4.", "1/4", "1/8.", "1/8", "1/16.", "1/8T", "1/16", "1/16T", "1/32" };
const float kDivBeats[kNumDivisions] = { 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
}

const char* divisionName (int i) { return kDivNames[juce::jlimit (0, kNumDivisions - 1, i)]; }

float syncedValue (const ParamSpec& s, int division, double bpm)
{
    const float seconds = kDivBeats[juce::jlimit (0, kNumDivisions - 1, division)] * 60.0f / (float) juce::jmax (20.0, bpm);
    if (s.unit != nullptr && juce::String (s.unit) == "ms") return std::min (seconds * 1000.0f, 2000.0f);
    return std::clamp (1.0f / seconds, 0.02f, 30.0f);                      // a cycle lasts one note length
}
} // namespace me1
