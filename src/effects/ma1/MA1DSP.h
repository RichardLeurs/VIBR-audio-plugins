// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// MA-1 engine: noise gate -> tight boost -> multi-stage amp (oversampled) -> tone stack / presence / resonance -> cabinet
// (two impulse responses, or a filter model when none is loaded) -> parametric EQ -> limiter.
#include <JuceHeader.h>
#include <cmath>

namespace ma1
{
constexpr int kNumTypes = 4;

struct AmpType
{
    const char* name;
    float gainLoDb, gainHiDb;      // first-stage gain range of the GAIN knob
    float hpHz;                    // input high-pass (tightness)
    float midHz, midDb;            // mid emphasis in front of the gain stages
    int stages;                    // clipping stages (2 .. 4)
    float asym;                    // asymmetry of the first stage
    float interHpHz;               // coupling high-pass between the stages
    float lpHz;                    // fizz filter before the cabinet
    float trimDb;                  // loudness match
};

constexpr AmpType kTypes[kNumTypes] = {
    { "CRUNCH",  -2.0f, 24.0f,  90.0f,  800.0f, 3.0f, 2, 0.20f, 120.0f, 6500.0f,  4.0f },
    { "MODERN",  14.0f, 40.0f, 130.0f, 1400.0f, 3.0f, 3, 0.08f, 160.0f, 6000.0f, -2.0f },
    { "BRUTAL",  20.0f, 46.0f, 140.0f,  900.0f, 4.0f, 4, 0.05f, 180.0f, 5600.0f, -5.0f },
    { "ULTRA",   26.0f, 52.0f, 170.0f, 1800.0f, 5.0f, 4, 0.00f, 200.0f, 5200.0f, -8.0f },
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float shape (float x, float asym) { return std::tanh (x + asym) - std::tanh (asym); }

// RBJ biquad, transposed direct form II
class Biquad
{
public:
    void lowpass (double sr, float f, float q)   { prep (sr, f, q); set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); }
    void highpass (double sr, float f, float q)  { prep (sr, f, q); set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); }
    void peak (double sr, float f, float q, float db)
    {
        prep (sr, f, q); const double A = std::pow (10.0, db / 40.0);
        set (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }
    void lowShelf (double sr, float f, float db)
    {
        prep (sr, f, 0.7071f); const double A = std::pow (10.0, db / 40.0), sq = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * cw + sq), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sq),
             (A + 1) + (A - 1) * cw + sq, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sq);
    }
    void highShelf (double sr, float f, float db)
    {
        prep (sr, f, 0.7071f); const double A = std::pow (10.0, db / 40.0), sq = 2 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * cw + sq), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sq),
             (A + 1) - (A - 1) * cw + sq, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sq);
    }
    void reset() { z1 = z2 = 0.0f; }
    float process (float x)
    {
        const float y = c0 * x + z1;
        z1 = c1 * x - d1 * y + z2;
        z2 = c2 * x - d2 * y;
        return y;
    }
private:
    void prep (double sr, float f, float q)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * juce::jmin ((double) f, 0.45 * sr) / sr;
        cw = std::cos (w); alpha = std::sin (w) / (2.0 * (double) q);
    }
    void set (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        c0 = (float) (b0 / a0); c1 = (float) (b1 / a0); c2 = (float) (b2 / a0); d1 = (float) (a1 / a0); d2 = (float) (a2 / a0);
    }
    double cw = 1.0, alpha = 0.0;
    float c0 = 1, c1 = 0, c2 = 0, d1 = 0, d2 = 0, z1 = 0, z2 = 0;
};

// ---- parameter mappings (knobs run 0 .. 10) ----
inline float gateThresholdDb (float k)  { return -70.0f + 4.0f * k; }                        // -70 .. -30 dB
inline float gateReleaseMs (float k)    { return 30.0f * std::pow (13.0f, k * 0.1f); }       // 30 .. 400 ms
inline float tightHz (float k)          { return 40.0f * std::pow (22.5f, k * 0.1f); }       // 40 .. 900 Hz
inline float bipolarDb (float k)        { return (k - 5.0f) * 2.4f; }                        // +-12 dB
inline float eqFreqHz (float k)         { return 200.0f * std::pow (15.0f, k * 0.1f); }      // 200 Hz .. 3 kHz
inline float lowCutHz (float k)         { return 40.0f * std::pow (10.0f, k * 0.1f); }       // 40 .. 400 Hz
inline float highCutHz (float k)        { return 3000.0f * std::pow (5.3f, k * 0.1f); }      // 3 kHz .. 16 kHz
inline float irLevelGain (float k)      { return k <= 0.01f ? 0.0f : dbToGain ((k - 7.0f) * 3.0f); }

// Noise gate with a high-passed detector, hold and hysteresis.
class Gate
{
public:
    void prepare (double sampleRate) { sr = sampleRate; hp.highpass (sr, 150.0f, 0.7071f); reset(); }
    void reset() { hp.reset(); env = 0.0f; gain = 0.0f; open = false; holdLeft = 0; }

    // returns the gain for this sample; peakIn = max |input| of the channels
    float process (float peakIn, float thresholdDb, float releaseMs)
    {
        const float x = std::fabs (hp.process (peakIn));
        const float aC = std::exp (-1.0f / (0.0005f * (float) sr)), rC = std::exp (-1.0f / (0.030f * (float) sr));
        env = x > env ? aC * env + (1.0f - aC) * x : rC * env + (1.0f - rC) * x;
        const float db = 20.0f * std::log10 (env + 1.0e-9f);
        if (db > thresholdDb) { open = true; holdLeft = (int) (0.015 * sr); }
        else if (open && db < thresholdDb - 4.0f)
        {
            if (holdLeft > 0) --holdLeft; else open = false;
        }
        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? std::exp (-1.0f / (0.001f * (float) sr)) : std::exp (-1.0f / (0.001f * releaseMs * (float) sr));
        gain = c * gain + (1.0f - c) * target;
        return gain;
    }
    bool isOpen() const { return open; }

private:
    double sr = 44100.0;
    Biquad hp;
    float env = 0.0f, gain = 0.0f;
    bool open = false;
    int holdLeft = 0;
};

// the part of the signal path that runs at the oversampled rate: boost + amp stages
struct AmpPre
{
    Biquad tight, boostMid, hp, mid, bright, inter1, inter2, inter3;
    float sagEnv = 0.0f;
    void reset() { tight.reset(); boostMid.reset(); hp.reset(); mid.reset(); bright.reset(); inter1.reset(); inter2.reset(); inter3.reset(); sagEnv = 0.0f; }

    void setBoost (double osRate, float tightK)
    {
        tight.highpass (osRate, tightHz (tightK), 0.7071f);
        boostMid.peak (osRate, 720.0f, 0.7f, 5.0f);
    }
    void setAmp (double osRate, const AmpType& t, bool brightOn)
    {
        hp.highpass (osRate, t.hpHz, 0.7071f);
        mid.peak (osRate, t.midHz, 0.9f, t.midDb);
        bright.highShelf (osRate, 2800.0f, brightOn ? 6.0f : 0.0f);
        inter1.highpass (osRate, t.interHpHz, 0.7071f);
        inter2.highpass (osRate, t.interHpHz, 0.7071f);
        inter3.highpass (osRate, t.interHpHz, 0.7071f);
    }
    float processBoost (float x, float drive, float levelGain)
    {
        float y = boostMid.process (tight.process (x));
        y = shape (y * drive, 0.0f) / std::sqrt (drive);                // drive 1 = clean, higher = softly clipped, level kept in check
        return y * levelGain;
    }
    float processAmp (float x, float g1, const AmpType& t, float sag, float sagCoefAtk, float sagCoefRel)
    {
        float y = shape (bright.process (mid.process (hp.process (x))) * g1, t.asym);
        const float a = std::fabs (y);
        sagEnv = a > sagEnv ? sagCoefAtk * sagEnv + (1.0f - sagCoefAtk) * a : sagCoefRel * sagEnv + (1.0f - sagCoefRel) * a;
        const float sagGain = 1.0f - sag * 0.35f * juce::jmin (1.0f, sagEnv);       // the supply sags under load: later stages get less gain
        y = shape (inter1.process (y) * 3.0f * sagGain, t.asym * 0.5f);
        if (t.stages >= 3) y = shape (inter2.process (y) * 3.0f * sagGain, 0.0f);
        if (t.stages >= 4) y = shape (inter3.process (y) * 2.5f * sagGain, 0.0f);
        return y;
    }
};

// filter model of a 4x12 cabinet, used while no impulse response is loaded
struct FactoryCab
{
    Biquad hp, low, pres, lp1, lp2;
    void reset() { hp.reset(); low.reset(); pres.reset(); lp1.reset(); lp2.reset(); }
    void set (double sr, float lpHz)
    {
        hp.highpass (sr, 85.0f, 0.7071f);
        low.peak (sr, 110.0f, 1.2f, 3.0f);
        pres.peak (sr, 2800.0f, 1.0f, 3.0f);
        lp1.lowpass (sr, lpHz, 0.7071f);
        lp2.lowpass (sr, lpHz * 1.15f, 0.7071f);
    }
    float process (float x) { return lp2.process (lp1.process (pres.process (low.process (hp.process (x))))); }
};

// peak limiter at the end of the chain (fast attack, 80 ms release; the gain is applied from the following sample)
struct Limiter
{
    float gain = 1.0f;
    void reset() { gain = 1.0f; }
    float process (float peak, double sr, float ceiling)
    {
        const float target = peak > ceiling ? ceiling / peak : 1.0f;
        const float c = target < gain ? std::exp (-1.0f / (0.0002f * (float) sr)) : std::exp (-1.0f / (0.080f * (float) sr));
        gain = c * gain + (1.0f - c) * target;
        return gain;
    }
};
} // namespace ma1
