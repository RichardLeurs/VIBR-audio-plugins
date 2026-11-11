// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Amp + cabinet engine (IR-2 style). Chain per channel:
//   amp (2x oversampled): input high-pass (bright) -> mid emphasis -> 1-3 tanh gain stages with high-pass coupling between them
//   tone stack: BASS (low shelf) / MIDDLE (peak) / TREBLE (high shelf)
//   cabinet: high-pass, low and presence peaks, two low-pass stages (a filter model, not a measured impulse response)
//   LEVEL, then AMBIENCE: a small stereo room reverb.
// The 11 amp types are a table (AmpType); their names are generic style names, not the real amps' names.
#include <JuceHeader.h>
#include <cmath>

namespace ir2
{
constexpr int kNumTypes = 11;

struct AmpType
{
    const char* name;
    float gainLoDb, gainHiDb;      // first-stage gain range of the GAIN knob
    float hpHz;                    // input high-pass (tightness / brightness)
    float midHz, midDb;            // mid emphasis in front of the gain stages
    int stages;                    // number of clipping stages
    float asym;                    // asymmetry (even harmonics)
    float cabLpHz;                 // cabinet roll-off
    float trimDb;                  // loudness match
};

// order = selector order, lower left to lower right
constexpr AmpType kTypes[kNumTypes] = {
    { "CLEAN",   -6.0f, 14.0f,  60.0f,  600.0f, -1.0f, 1, 0.00f, 6500.0f,  6.0f },
    { "TWN",     -4.0f, 18.0f,  50.0f,  500.0f, -3.0f, 1, 0.05f, 6200.0f,  4.0f },
    { "TWEED",    2.0f, 24.0f,  70.0f,  800.0f,  3.0f, 1, 0.25f, 4800.0f,  1.0f },
    { "DIAMOND",  8.0f, 28.0f, 150.0f, 1500.0f,  3.0f, 1, 0.10f, 6000.0f,  0.0f },
    { "CRUNCH",  10.0f, 30.0f,  90.0f, 1000.0f,  2.0f, 2, 0.15f, 5200.0f, -2.0f },
    { "BRIT",    14.0f, 36.0f, 100.0f, 1000.0f,  4.0f, 2, 0.10f, 5000.0f, -3.0f },
    { "BROWN",   16.0f, 38.0f,  80.0f,  700.0f,  3.0f, 2, 0.20f, 4800.0f, -3.0f },
    { "HI-GAIN", 22.0f, 46.0f, 120.0f, 1600.0f,  2.0f, 3, 0.05f, 5200.0f, -6.0f },
    { "SLDN",    24.0f, 48.0f,  90.0f,  900.0f,  5.0f, 3, 0.10f, 4600.0f, -6.0f },
    { "MODDED",  22.0f, 46.0f, 110.0f, 1200.0f,  3.0f, 3, 0.15f, 5000.0f, -6.0f },
    { "RFIER",   24.0f, 48.0f,  70.0f,  500.0f, -3.0f, 3, 0.05f, 5000.0f, -6.0f },
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float shape (float x, float asym) { return std::tanh (x + asym) - std::tanh (asym); }

// RBJ biquad, transposed direct form II
class Biquad
{
public:
    void lowpass (double sr, float f, float q)   { prep (sr, f, q); set (( 1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha); }
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

// the part of the amp that runs at the oversampled rate
struct AmpPre
{
    Biquad hp, mid, inter1, inter2;
    void reset() { hp.reset(); mid.reset(); inter1.reset(); inter2.reset(); }
    void setType (double osRate, const AmpType& t)
    {
        hp.highpass (osRate, t.hpHz, 0.7071f);
        mid.peak (osRate, t.midHz, 0.9f, t.midDb);
        inter1.highpass (osRate, 140.0f, 0.7071f);
        inter2.highpass (osRate, 140.0f, 0.7071f);
    }
    float process (float x, float g1, const AmpType& t)
    {
        float y = shape (mid.process (hp.process (x)) * g1, t.asym);
        if (t.stages >= 2) y = shape (inter1.process (y) * 3.0f, t.asym * 0.5f);
        if (t.stages >= 3) y = shape (inter2.process (y) * 3.0f, 0.0f);
        return y;
    }
};

// tone stack + cabinet
struct PostChain
{
    Biquad bass, middle, treble, cabHp, cabLow, cabPres, cabLp1, cabLp2;
    void reset() { bass.reset(); middle.reset(); treble.reset(); cabHp.reset(); cabLow.reset(); cabPres.reset(); cabLp1.reset(); cabLp2.reset(); }
    void set (double sr, float bass01, float mid01, float treb01, const AmpType& t)
    {
        bass.lowShelf (sr, 120.0f, (bass01 - 0.5f) * 24.0f);
        middle.peak (sr, 650.0f, 0.8f, (mid01 - 0.5f) * 20.0f);
        treble.highShelf (sr, 3200.0f, (treb01 - 0.5f) * 24.0f);
        cabHp.highpass (sr, 85.0f, 0.7071f);
        cabLow.peak (sr, 110.0f, 1.2f, 3.0f);
        cabPres.peak (sr, 2800.0f, 1.0f, 3.0f);
        cabLp1.lowpass (sr, t.cabLpHz, 0.7071f);
        cabLp2.lowpass (sr, t.cabLpHz * 1.15f, 0.7071f);
    }
    float process (float x)
    {
        float y = treble.process (middle.process (bass.process (x)));
        return cabLp2.process (cabLp1.process (cabPres.process (cabLow.process (cabHp.process (y)))));
    }
};

// small stereo room (Freeverb-like: 4 damped combs and 2 all-passes per side)
class Room
{
public:
    void prepare (double sr)
    {
        static constexpr int combLen[4] = { 1116, 1188, 1277, 1356 }, apLen[2] = { 556, 441 };
        const double k = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            const int spread = c * 23;
            for (int i = 0; i < 4; ++i) { comb[c][i].buf.assign ((size_t) ((combLen[i] + spread) * k) + 4, 0.0f); comb[c][i].pos = 0; comb[c][i].lp = 0.0f; }
            for (int i = 0; i < 2; ++i) { ap[c][i].buf.assign ((size_t) ((apLen[i] + spread) * k) + 4, 0.0f); ap[c][i].pos = 0; }
        }
    }
    float process (int c, float x)
    {
        float sum = 0.0f;
        for (auto& cb : comb[c])
        {
            const float o = cb.buf[(size_t) cb.pos];
            cb.lp = o * 0.7f + cb.lp * 0.3f;                          // damping
            cb.buf[(size_t) cb.pos] = x * 0.03f + cb.lp * 0.80f;
            if (++cb.pos >= (int) cb.buf.size()) cb.pos = 0;
            sum += o;
        }
        for (auto& a : ap[c])
        {
            const float b = a.buf[(size_t) a.pos];
            const float y = -sum + b;
            a.buf[(size_t) a.pos] = sum + b * 0.5f;
            if (++a.pos >= (int) a.buf.size()) a.pos = 0;
            sum = y;
        }
        return sum;
    }
private:
    struct Comb { std::vector<float> buf; int pos = 0; float lp = 0.0f; };
    struct Allpass { std::vector<float> buf; int pos = 0; };
    Comb comb[2][4];
    Allpass ap[2][2];
};
} // namespace ir2
